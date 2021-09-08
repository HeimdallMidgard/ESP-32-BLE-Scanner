// Arduino
#include <Arduino.h>
#include <ArduinoJson.h> //JSON for Saving Values and Sending Data over MQTT

// SPIFFS
#include "SPIFFS.h"

// BLE
#include <NimBLEAdvertisedDevice.h>
#include <NimBLEDevice.h>

#include "NimBLEBeacon.h"

// Webserver
#include "ESPAsyncWebServer.h"
#include <AsyncElegantOTA.h>

// Connection
#include <AsyncMqttClient.h>
#include <ESP32Ping.h>
#include <WiFi.h>

// Scanner
#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00) >> 8) + (((x)&0xFF) << 8))
#define STRING(num) #num

class ScanDevice {
    std::vector<float> distances;
    std::string _id;
    std::string _name;
    std::string _type;

  public:
    ScanDevice(std::string var_uuid, std::string var_name, std::string var_type) {
      _id = var_uuid;
      _name = var_name;
      _type = var_type;
    }

    std::string uuid() { return _id; }
    bool uuid(std::string var_id) { return (var_id == _id); }
    std::string name() { return _name; }
    std::string type() { return _type; }

    void tick() {
      if (distances.size() > 0) distances.erase(distances.begin());
    }

    float distance() {
      if (distances.size() == 0) return 99999;
      return std::accumulate(distances.begin(), distances.end(), 0.0) / distances.size();
    }

    float distance(float new_distance) {
      distances.push_back(new_distance);
      if (distances.size() > 30) distances.erase(distances.begin());
      return distance();
    }

    std::string distanceStr() { return STRING(distance()); }

    std::string json() {
      char out[100];
      sprintf(out, "{ \"id\": \"%s\", \"name\": \"%s\", \"distance\": %f }", _id.c_str(), _name.c_str(), distance());
      return out;
    }
};

// Scanner Variables
BLEDevice *pBLEDev;
BLEScan *pBLEScan;
std::vector<BLEAddress> ignored;

std::string scan_topic, telemetry_topic;
std::string mqtt_topic_root("ESP32 BLE Scanner");
std::string mqtt_scan_prefix("ESP32 BLE Scanner/Scan/");
std::string status_topic("ESP32 BLE Scanner/Status/");
std::string mqtt_telemetry_prefix("ESP32 BLE Scanner/tele/");

std::string settingsFile("/settings.json");
std::string devicesFile("/devices.json");

StaticJsonDocument<1024> settings, devices;
std::vector<ScanDevice> device_list;

// IPAddress ip(192, 168, 1, 177);  // not in use

boolean wifi_ap_result;

// Wifi
WiFiClient espClient;

// MQTT
AsyncMqttClient mqttClient;

// Webserver
AsyncWebServer server(80);
AsyncWebSocket ws("/ws");
int wifi_errors = 0;

// Delete device distance every other scan
bool do_delete_distance = false;

// END OF DEFINITION

void write_to_logs(const char *new_log_entry) {
  // Copies to logs to publish in Weblog
  ws.printfAll(new_log_entry);
  Serial.println(new_log_entry);
}

void reboot() {
  write_to_logs("Rebooting.");
  delay(1000);
  ESP.restart();
}

//
// Settings functions
//

String loadFile(std::string filename, String defaultValue) {
  if (settings["device"]["debug"]) Serial.printf("loadFile(%s, %s)\n", filename.c_str(), defaultValue.c_str());
  File file = SPIFFS.open(filename.c_str());
  String data = (file.available()) ? file.readString() : defaultValue;
  if (data == "null") data = defaultValue;
  file.close();
  return data;
}

void loadJson(std::string filename, StaticJsonDocument<1024> &json, String defaultValue) {
  if (settings["device"]["debug"]) Serial.printf("loadJson(%s, %s)\n", filename.c_str(), defaultValue.c_str());
  String data = loadFile(filename, defaultValue);
  Serial.println(data);

  DeserializationError json_error = deserializeJson(json, data);
  if (json_error) {
    Serial.printf("deserializeJson() for %s failed: \n", filename.c_str());
    Serial.println(json_error.c_str());
    reboot();
  } else if (settings["device"]["debug"]) {
    Serial.printf("Loaded %s: ", filename.c_str());
    serializeJson(json, Serial);
    Serial.println();
  }
}

void saveJson(StaticJsonDocument<1024> &json, std::string filename) {
  if (settings["device"]["debug"]) Serial.printf("saveJson(%s)\n", filename.c_str());
  File file = SPIFFS.open(filename.c_str(), "w");
  if (file) {
    serializeJson(json, file);
    if (settings["device"]["debug"]) {
      Serial.printf("Saved to %s: ", filename.c_str());
      serializeJson(json, Serial);
      Serial.println();
    }
  }
}

void saveSettings() { saveJson(settings, settingsFile); }

void loadSettings() { loadJson(settingsFile, settings, "{}"); }

void saveDevices() { saveJson(devices, devicesFile); }

void parseDevices() {
  device_list.empty();
  JsonArray arr = devices.as<JsonArray>();
  for (JsonArray::iterator dev = arr.begin(); dev != arr.end(); ++dev) {
    JsonObject d = dev->as<JsonObject>();
    device_list.push_back(ScanDevice(d["uuid"], d["name"], d["type"]));
  }
}

void loadDevices() {
  loadJson(devicesFile, devices, "[]");
  parseDevices();
}

bool initSetting(const char key1[], const char key2[], const char defaultValue[]) {
  if (settings["device"]["debug"]) Serial.printf("initSetting(%s, %s, %s)\n", key1, key2, defaultValue);
  bool changed = false;
  if (settings[key1] == nullptr) {
    Serial.printf("settings.%s not set, creating.\n", key1);
    settings.createNestedObject(key1);
  }
  if (settings[key1][key2] == nullptr) {
    Serial.printf("settings.%s.%s not set, setting default.\n", key1, key2);
    settings[key1][key2] = defaultValue;
    changed = true;
  }
  return changed;
}

bool initSetting(const char key1[], const char key2[], int defaultValue) {
  if (settings["device"]["debug"]) Serial.printf("initSetting(%s, %s, %i)\n", key1, key2, defaultValue);
  bool changed = false;
  if (settings[key1] == nullptr) {
    Serial.printf("settings.%s not set, creating.\n", key1);
    settings.createNestedObject(key1);
  }
  if (settings[key1][key2] == nullptr) {
    Serial.printf("settings.%s.%s not set, setting default.\n", key1, key2);
    settings[key1][key2] = defaultValue;
    changed = true;
  }
  return changed;
}

bool initSetting(const char key1[], const char key2[], float defaultValue) {
  if (settings["device"]["debug"]) Serial.printf("initSetting(%s, %s, %f)\n", key1, key2, defaultValue);
  bool changed = false;
  if (settings[key1] == nullptr) {
    Serial.printf("settings.%s not set, creating.\n", key1);
    settings.createNestedObject(key1);
  }
  if (settings[key1][key2] == nullptr) {
    Serial.printf("settings.%s.%s not set, setting default.\n", key1, key2);
    settings[key1][key2] = defaultValue;
    changed = true;
  }
  return changed;
}

bool initSetting(const char key1[], const char key2[], bool defaultValue) {
  if (settings["device"]["debug"]) Serial.printf("initSetting(%s, %s, %d)\n", key1, key2, defaultValue);
  bool changed = false;
  if (settings[key1] == nullptr) {
    Serial.printf("settings.%s not set, creating.\n", key1);
    settings.createNestedObject(key1);
  }
  if (settings[key1][key2] == nullptr) {
    Serial.printf("settings.%s.%s not set, setting default.\n", key1, key2);
    settings[key1][key2] = defaultValue;
    changed = true;
  }
  return changed;
}

bool initSettingsDevice() {
  bool changed = false;
  if (initSetting("device", "room", "")) changed = true;
  if (initSetting("device", "use_ignore_list", true)) changed = true;
  if (initSetting("device", "debug", false)) changed = true;
  if (initSetting("device", "homeassistant_discovery", true)) changed = true;
  return changed;
}

bool initSettingsNetwork() {
  bool changed = false;
  if (initSetting("network", "ssid", "")) changed = true;
  if (initSetting("network", "password", "")) changed = true;
  if (initSetting("network", "hostname", "ESP32-BLE-Scanner")) changed = true;
  return changed;
}

bool initSettingsMqtt() {
  bool changed = false;
  if (initSetting("mqtt", "host", "")) changed = true;
  if (initSetting("mqtt", "port", 1883)) changed = true;
  if (initSetting("mqtt", "user", "")) changed = true;
  if (initSetting("mqtt", "password", "")) changed = true;
  return changed;
}

bool initSettingsBluetooth() {
  bool changed = false;
  if (initSetting("bluetooth", "scan_time", 5)) changed = true;
  if (initSetting("bluetooth", "scan_interval", 300)) changed = true;
  return changed;
}

bool initSettings() {
  if (settings["device"]["debug"]) Serial.println("initSettings()");
  bool changed = false;
  if (initSettingsDevice()) changed = true;
  if (initSettingsNetwork()) changed = true;
  if (initSettingsMqtt()) changed = true;
  if (initSettingsBluetooth()) changed = true;
  return changed;
}

bool savePostedJson(AsyncWebParameter *p, StaticJsonDocument<1024> &json, std::string filename) {
  json.clear();
  DeserializationError json_error = deserializeJson(json, p->value());
  if (json_error) {
    write_to_logs("DeserializeJson() failed: ");
    write_to_logs(json_error.c_str());
  } else {
    saveJson(json, filename);
    return true;
  }
  return false;
}

void check_mqtt_msg(uint16_t error_state) {
  if (error_state == 0) write_to_logs("Error publishing MQTT Message");
}

void connectToMqtt() {
  if (!mqttClient.connected()) {
    write_to_logs("Connecting to MQTT");
    mqttClient.connect();
  } else {
    write_to_logs("Already connected to MQTT");
  }
}

void onMqttConnect(bool sessionPresent) {
  write_to_logs("Connected to MQTT");

  // Set last will (msg that broker will send when connection to ESP32 is gone)
  mqttClient.setWill(status_topic.c_str(), 1, true, "offline");

  // Publish online status
  uint16_t msg_error;
  msg_error = mqttClient.publish(status_topic.c_str(), 1, true, "online");
  check_mqtt_msg(msg_error);
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  char msg[255];
  sprintf(msg, "Disconnected from MQTT with reason: %d\n", (int)reason);
  write_to_logs(msg);

  delay(3000);
  connectToMqtt();
}

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("WiFi connected");
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("WiFi disconnected");
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.print("WiFi connected successfuly • IP: ");
  Serial.println(WiFi.localIP());

  while (!Ping.ping(settings["mqtt"]["host"].as<const char *>(), 3))
    delay(50);

  connectToMqtt();
   // Reset Wifi Error Counter when the connection is working
   wifi_errors = 0;
}

void WiFiLostIP(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.print("WiFi lost IP, disconnecting mqtt");
  mqttClient.disconnect();
}

void startAP() {
  Serial.println(" ");
  Serial.println("Starting AP");

  WiFi.mode(WIFI_AP);
  delay(500);
  WiFi.softAP("ESP32-BLE-Scanner");

  wifi_ap_result = WiFi.softAP("ESP32-BLE-Scanner");

  server.begin(); // Start Webserver

  delay(500);
  Serial.print("Event IP address: ");
  Serial.println(WiFi.softAPIP());
}

void WiFi_Controller() {
  Serial.println("WiFi_Controller()");
  if (WiFi.status() == WL_CONNECTED || wifi_ap_result == true) return;

  wifi_errors++;
  Serial.println(" ");
  Serial.print("Wifi Errors: ");
  Serial.println(wifi_errors);

  if (wifi_errors == 10 && !wifi_ap_result) {
    startAP();
    return;
  }

  Serial.println("Disconnected from WiFi access point");
  Serial.println("Trying to Reconnect");

  WiFi.begin(settings["wifi"]["ssid"].as<const char*>(), settings["wifi"]["password"].as<const char*>());
}

void onWsEvent(AsyncWebSocket *server, AsyncWebSocketClient *client, AwsEventType type, void *arg, uint8_t *data, size_t len) {
  if (type == WS_EVT_CONNECT) {
    Serial.println("Websocket client connection received");
    client->text("Connected to websocket");
  } else if (type == WS_EVT_DISCONNECT) {
    Serial.println("Client disconnected");
  }
}

// Alternative Range Calculation
/*
float calculateAccuracy(double txPower, double rssi_calc) {

  float defaultTxPower = -72;
        float distFl;

  if (rssi_calc == 0) {
      return -1.0;
  }

  if (!txPower) {
      // somewhat reasonable default value
      txPower = defaultTxPower;
  }

        if (txPower > 0) {
                txPower = txPower * -1;
        }

  const float ratio = rssi_calc * 1.0 / txPower;
  if (ratio < 1.0) {
      distFl = pow(ratio, 10);
  } else {
      distFl = (0.89976) * pow(ratio, 7.7095) + 0.111;
  }
        return round(distFl * 100) / 100;
}
*/

// Distance Calculation
float calculateAccuracy(float txCalibratedPower, float rssi) {
  if (settings["device"]["debug"]) Serial.println("calculateAccuracy()");
  float ratio_db = txCalibratedPower - rssi;
  float ratio_linear = pow(10, ratio_db / 10);

  float r = sqrt(ratio_linear);
  r = r / 20;
  return r;
}

// Checks if devices are inside devices.json
bool devices_set_up() { return (devices.size() > 0); }

// Scanner

bool isBeacon(std::string strManufacturerData) {
  if (settings["device"]["debug"]) Serial.println("isBeacon()");
  uint8_t cManufacturerData[100];
  strManufacturerData.copy((char *)cManufacturerData, strManufacturerData.length(), 0);
  return (strManufacturerData.length() == 25 && cManufacturerData[0] == 0x4C &&
          cManufacturerData[1] == 0x00);
}

std::string getDeviceName(std::string uuid) {
  if (settings["device"]["debug"]) Serial.printf("getDeviceName(%s)\n", uuid.c_str());

  if (uuid.length() == 0) return "";
  for (size_t i = 0; i < devices.size(); i++) {
    if (uuid == devices[i]["uuid"]) return devices[i]["name"].as<std::string>();
  }
  return "";
}

void sendMqtt(std::string msg) {
  if (settings["device"]["debug"]) Serial.printf("sendMqtt(%s)\n", msg.c_str());

  uint16_t msg_error;
  msg_error = mqttClient.publish(scan_topic.c_str(), 1, false, msg.c_str());
  check_mqtt_msg(msg_error);
  write_to_logs(msg.c_str());
}

void sendDeviceMqtt(std::string uuid, std::string name, float distance) {
  if (settings["device"]["debug"]) Serial.printf("sendDeviceMqtt(%s, %s, %f)\n", uuid.c_str(), name.c_str(), distance);

  char msg[120];
  sprintf(msg, "{ \"id\": \"%s\", \"name\": \"%s\", \"distance\": %f }", uuid.c_str(), name.c_str(), distance);
  std::string strMsg(msg);
  sendMqtt(strMsg);
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice *advertisedDevice) {
    if (settings["device"]["debug"]) Serial.println("onResult()");
    if (advertisedDevice->haveManufacturerData() == false) return;

    std::string strManufacturerData = advertisedDevice->getManufacturerData();

    BLEAddress addr = advertisedDevice->getAddress();
    if (settings["device"]["debug"]) {
      char debugMsg[100];
      sprintf(debugMsg, "Found device %s", addr.toString().c_str());
      write_to_logs(debugMsg);
    }

    if (settings["device"]["debug"]) write_to_logs("  Getting RSSI");
    int rssi = advertisedDevice->getRSSI();

    if (settings["device"]["use_ignore_list"] && !isBeacon(strManufacturerData)) {
      pBLEDev->addIgnored(addr);
      ignored.push_back(addr);
      if (settings["device"]["debug"]) write_to_logs("  Device isn't a beacon, adding to ignore list");
      return;
    }

    if (settings["device"]["debug"]) write_to_logs("  Creating BLEBeacon");

    BLEBeacon oBeacon = BLEBeacon();
    oBeacon.setData(strManufacturerData);

    if (settings["device"]["debug"]) write_to_logs("  Getting proximity UUID");
    std::string uuid = oBeacon.getProximityUUID().toString();

    if (settings["device"]["debug"]) write_to_logs("  Checking device list");
    auto dev = std::find_if(device_list.begin(), device_list.end(), [&uuid](ScanDevice &d) { return d.uuid(uuid); });
    if (dev == device_list.end()) {
      if (settings["device"]["debug"]) write_to_logs("  Device not wanted");
      return;
    }

    ScanDevice &device = *dev;

    if (settings["device"]["debug"]) write_to_logs("  Getting signal power");
    int8_t power = oBeacon.getSignalPower();
    float distance = calculateAccuracy(power, rssi);
    // float distance = getAverageDistance(uuid, calculateAccuracy(power,
    // rssi));

    device.distance(distance);

    if (mqttClient.connected()) {
      sendMqtt(device.json());
      return;
    }

    char msg[100];
    sprintf(msg, "Found device %s but MQTT is not connected", device.name().c_str());
    write_to_logs(msg);
  }
};

void publishTelemetry(NimBLEScanResults devices) {
  if (settings["device"]["debug"]) Serial.println("publishTelemetry()");
  if (!mqttClient.connected()) return;
  char msg[100];
  int uptime = (int)(esp_timer_get_time() / 1000000);
  sprintf(msg,
          "{ \"results_last_scan\": \"%i\", \"free_heap\": \"%i\", \"uptime\": \"%i\" }",
          devices.getCount(), ESP.getFreeHeap(), uptime);
  ws.printfAll(msg);
  uint16_t msg_error;
  msg_error = mqttClient.publish(telemetry_topic.c_str(), 1, false, msg);
  check_mqtt_msg(msg_error);
  Serial.println(msg);
}

//
//
//
//  SETUP
//
//
//

void startWifi() {
  if (settings["device"]["debug"]) Serial.println("startWifi()");
  const char *ssid = settings["network"]["ssid"];
  const char *password = settings["network"]["password"];
  const char *hostname = settings["network"]["hostname"];

  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE); // Dyn IP
  // WiFi.config(ip, INADDR_NONE, INADDR_NONE, INADDR_NONE);  // Fixed IP

  char msg[50];
  sprintf(msg, "Connecting to WiFi @ %s:%s \n", ssid, password);
  write_to_logs(msg);

  WiFi.setHostname(hostname);

  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_STA_CONNECTED);
  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_STA_DISCONNECTED);
  WiFi.onEvent(WiFiGotIP, SYSTEM_EVENT_STA_GOT_IP);
  WiFi.onEvent(WiFiLostIP, SYSTEM_EVENT_STA_LOST_IP);
  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_AP_STACONNECTED);
  WiFi.onEvent(WiFiStationDisconnected, SYSTEM_EVENT_AP_STADISCONNECTED);

  WiFi.begin(ssid, password);
}

void startMqtt() {
  if (settings["device"]["debug"]) Serial.println("startMqtt()");
  const char *hostname = settings["network"]["hostname"];
  const char *mqttHost = settings["mqtt"]["host"];
  int mqttPort = settings["mqtt"]["port"];
  const char *mqttUser = settings["mqtt"]["user"];
  const char *mqttPassword = settings["mqtt"]["password"];

  char msg[255];
  sprintf(msg, "Setting up MQTT • %s:%i • %s@%s \n", mqttHost, mqttPort, mqttUser, mqttPassword);
  write_to_logs(msg);

  // Combine Room and Prefix to MQTT topic
  scan_topic = mqtt_topic_root + "/Scan/" + settings["device"]["room"].as<std::string>();
  telemetry_topic = mqtt_topic_root + "/tele/" + settings["device"]["room"].as<std::string>();

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);

  randomSeed(micros());

  // Start MQTT Connection
  mqttClient.setServer(mqttHost, mqttPort);
  mqttClient.setClientId(hostname);

  if (strlen(mqttUser) == 0)
    Serial.println("No MQTT User set");
  else
    mqttClient.setCredentials(mqttUser, mqttPassword);
}

void startScanner() {
  if (settings["device"]["debug"]) Serial.println("startScanner()");
  int interval = settings["bluetooth"]["scan_interval"];
  int window = (int)(interval * 0.9);
  pBLEDev = new BLEDevice;
  pBLEDev->init("");
  pBLEScan = pBLEDev->getScan(); // create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  // active scan uses more power, but get results faster
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(interval);
  pBLEScan->setWindow(window); // less or equal setInterval value
}

void startWebServer() {
  if (settings["device"]["debug"]) Serial.println("startWebserver()");

  // Serve css
  server.serveStatic("/assets", SPIFFS, "/web/assets");

  // Serve partials
  server.serveStatic("/partials", SPIFFS, "/web/partials");

  // Webserver Mainpage
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/web/index.html", String(), false);
  });

  // Send 200
  server.on("/api/status", HTTP_GET, [](AsyncWebServerRequest *request) { request->send(200); });

  // Send settings json
  server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    std::string json;
    serializeJson(settings, json);
    request->send(200, "application/json", json.c_str());
  });

  // Save settings json
  server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println("POST Request to /api/settings");
    bool saved = false;
    if (request->hasParam("settings", true)) {
      saved = savePostedJson(request->getParam("settings", true), settings, settingsFile);
    } else {
      Serial.println("Has no settings param");
    }
    if (saved) {
      request->send(200);
      // reboot();
    } else {
      Serial.println("Unable to save settings");
      request->send(503);
    }
  });

  // Reset
  server.on("/api/reboot", HTTP_GET, [](AsyncWebServerRequest *request) { reboot(); });

  // Clear Ignores
  server.on("/api/clearignores", HTTP_GET, [](AsyncWebServerRequest *request) {
    int numAddr = ignored.size();
    while (ignored.size() > 0) {
      BLEAddress addr = ignored[0];
      pBLEDev->removeIgnored(addr);
      ignored.erase(ignored.begin());
      if (settings["device"]["debug"]) {
        char debugMsg[100];
        sprintf(debugMsg, "Removed %s from ignore list", addr.toString().c_str());
        write_to_logs(debugMsg);
      }
    }
    char msg[100];
    sprintf(msg, "Removed %i devices from ignore list", numAddr);
    write_to_logs(msg);
    request->send(200);
  });

  // Send devices json
  server.on("/api/devices", HTTP_GET, [](AsyncWebServerRequest *request) {
    std::string json;
    serializeJson(devices, json);
    request->send(200, "application/json", json.c_str());
  });

  // Save devices json
  server.on("/api/devices", HTTP_POST, [](AsyncWebServerRequest *request) {
    Serial.println("POST Request to /api/devices");
    bool saved = false;
    if (request->hasParam("devices", true)) {
      saved = savePostedJson(request->getParam("devices", true), devices, devicesFile);
    } else {
      Serial.println("Has no devices param");
    }
    if (saved) {
      request->send(200);
    } else {
      Serial.println("Unable to save devices");
      request->send(503);
    }
  });

  ws.onEvent(onWsEvent);
  server.addHandler(&ws);
  AsyncElegantOTA.begin(&server);

  // Start Webserver
  server.begin();
}

void setup() {
  if (settings["device"]["debug"]) Serial.println("setup()");
  Serial.begin(115200);
  Serial.println("");
  write_to_logs("Starting...");

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    write_to_logs("Error initializing SPIFFS");
    while (true) {
    }
  }

  loadSettings();
  loadDevices();

  if (initSettings()) {
    saveSettings();
    saveDevices();
    reboot();
  }

  startWifi();
  startWebServer();
  startMqtt();
  startScanner();

  if (devices_set_up() == false) write_to_logs("No devices set up. Not scanning.");
}

void delete_distance(ScanDevice &dev) { dev.tick(); }

void delete_distances() {
  std::for_each(device_list.begin(), device_list.end(), delete_distance);
  do_delete_distance = !do_delete_distance;
}

void loop() {
  if (settings["device"]["debug"]) Serial.println("loop()");

  while (wifi_ap_result) delay(5000);

  if (!WiFi.isConnected() && !wifi_ap_result) {
    delay(5000);
    WiFi_Controller();
    return;
  }
  if (devices.size() == 0) {
    write_to_logs("No devices set up. Not scanning.");
    delay(5000);
    return;
  }
  while (pBLEScan->isScanning()) delay((int)(settings["bluetooth"]["scan_time"].as<int>() / 4));

  //if (do_delete_distance) delete_distances();
  // Scanner
  int scanTime = settings["bluetooth"]["scan_time"];
  Serial.println("Scanning...");
  pBLEScan->start(scanTime, publishTelemetry, false);
  Serial.println("_____________________________________");
  pBLEScan->clearResults(); // delete results fromBLEScan buffer to
                            // release memory
}