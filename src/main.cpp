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
#include <WiFi.h>

// Scanner
#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00) >> 8) + (((x)&0xFF) << 8))

// Scanner Variables
BLEDevice *pBLEDev;
BLEScan *pBLEScan;
std::vector<BLEAddress> ignored;

char scan_topic[60], telemetry_topic[60];
const char *mqtt_scan_prefix = "ESP32 BLE Scanner/Scan/";
const char *status_topic = "ESP32 BLE Scanner/Status/";
const char *mqtt_telemetry_prefix = "ESP32 BLE Scanner/tele/";

const char settingsFile[15] = "/settings.json";
const char devicesFile[15] = "/devices.json";

StaticJsonDocument<1024> settings, devices;

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

String loadFile(const char *filename, String defaultValue) {
  if (settings["device"]["debug"]) Serial.printf("loadFile(%s, %s)\n", filename, defaultValue.c_str());
  File file = SPIFFS.open(filename);
  String data = (file.available()) ? file.readString() : defaultValue;
  if (data == "null") data = defaultValue;
  file.close();
  return data;
}

void loadJson(const char *filename, StaticJsonDocument<1024> &json, String defaultValue) {
  if (settings["device"]["debug"]) Serial.printf("loadJson(%s, %s)\n", filename, defaultValue.c_str());
  String data = loadFile(filename, defaultValue);
  Serial.println(data);

  DeserializationError json_error = deserializeJson(json, data);
  if (json_error) {
    Serial.printf("deserializeJson() for %s failed: \n", filename);
    Serial.println(json_error.c_str());
    reboot();
  } else if (settings["device"]["debug"]) {
    Serial.printf("Loaded %s: ", filename);
    serializeJson(json, Serial);
    Serial.println();
  }
}

void saveJson(StaticJsonDocument<1024> &json, const char *filename) {
  if (settings["device"]["debug"]) Serial.printf("saveJson(%s)\n", filename);
  File file = SPIFFS.open(filename, "w");
  if (file) {
    serializeJson(json, file);
    if (settings["device"]["debug"]) {
      Serial.printf("Saved to %s: ", filename);
      serializeJson(json, Serial);
      Serial.println();
    }
  }
}

void saveSettings() { saveJson(settings, settingsFile); }

void loadSettings() { loadJson(settingsFile, settings, "{}"); }

void saveDevices() { saveJson(devices, devicesFile); }

void loadDevices() { loadJson(devicesFile, devices, "[]"); }

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

bool savePostedJson(AsyncWebParameter *p, StaticJsonDocument<1024> &json, const char *filename) {
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
  if(!mqttClient.connected()) {
    write_to_logs("Connecting to MQTT");
    mqttClient.connect();
  } else {
    write_to_logs("Already connected to MQTT");
  }
}

void onMqttConnect(bool sessionPresent) {
  write_to_logs("Connected to MQTT");

  // Set last will (msg that broker will send when connection to ESP32 is gone)
  mqttClient.setWill(status_topic, 1, true, "offline");

  // Publish online status
  uint16_t msg_error;
  msg_error = mqttClient.publish(status_topic, 1, true, "online");
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

  delay(3000);
  connectToMqtt();
}

void WiFi_Controller() {
  if(WiFi.status() == WL_CONNECTED || wifi_ap_result == true) {
    return;
  }
  wifi_errors++;
  Serial.println(" ");
  Serial.print("Wifi Errors: ");
  Serial.println(wifi_errors);

  if ((wifi_errors < 9) && (wifi_ap_result == false)) {
    Serial.println("Disconnected from WiFi access point");
    Serial.println("Trying to Reconnect");

    const char *ssid = settings["wifi"]["ssid"];
    const char *password = settings["wifi"]["password"];

    WiFi.begin(ssid, password);

  } else if ((wifi_errors > 9) && (wifi_ap_result == false)) {
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
  float ratio_db = txCalibratedPower - rssi;
  float ratio_linear = pow(10, ratio_db / 10);

  float r = sqrt(ratio_linear);
  r = r / 20;
  return r;
}

// Checks if devices are inside devices.json
bool devices_set_up() { return (devices.size() > 0); }

// float getAverageDistance(const char *uuid, float distance) {
//   if (deviceDistances.count(uuid) == 0) {
//     std::vector<float> distances;
//     deviceDistances[uuid] = distances;
//   }
//   if (deviceDistances[uuid].size() == 30) {
//     deviceDistances[uuid].erase(deviceDistances[uuid].begin());
//   }
//   deviceDistances[uuid].push_back(distance);
//   return std::accumulate(deviceDistances[uuid].begin(),
//   deviceDistances[uuid].end(), 0LL) / deviceDistances[uuid].size();
// }

// Scanner

bool isBeacon(std::string strManufacturerData) {
  uint8_t cManufacturerData[100];
  strManufacturerData.copy((char *)cManufacturerData, strManufacturerData.length(), 0);
  return (strManufacturerData.length() == 25 && cManufacturerData[0] == 0x4C &&
          cManufacturerData[1] == 0x00);
}

const char *getDeviceName(const char *uuid) {
  if (strlen(uuid) == 0) return nullptr;
  for (size_t i = 0; i < devices.size(); i++) {
    if (strcmp(uuid, devices[i]["uuid"]) == 0) return devices[i]["name"];
  }
  return nullptr;
}

void sendDeviceMqtt(const char *uuid, const char *name, float distance) {
  char msg[120];
  sprintf(msg, "{ \"id\": \"%s\", \"name\": \"%s\", \"distance\": %f }", uuid, name, distance);

  uint16_t msg_error;
  msg_error = mqttClient.publish(scan_topic, 1, false, msg);
  check_mqtt_msg(msg_error);
  write_to_logs(msg);
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice *advertisedDevice) {
    if (advertisedDevice->haveManufacturerData() == false) return;

    std::string strManufacturerData = advertisedDevice->getManufacturerData();
    if (settings["device"]["use_ignore_list"] && !isBeacon(strManufacturerData)) {
      BLEAddress addr = advertisedDevice->getAddress();
      pBLEDev->addIgnored(addr);
      ignored.push_back(addr);
      if (settings["device"]["debug"]) {
        char debugMsg[100];
        sprintf(debugMsg, "Added %s to ignore list", addr.toString().c_str());
        write_to_logs(debugMsg);
      }
      return;
    }
    BLEBeacon oBeacon = BLEBeacon();
    oBeacon.setData(strManufacturerData);
    const char *uuid = oBeacon.getProximityUUID().toString().c_str();
    const char *name = getDeviceName(uuid);
    if (name == nullptr || strlen(name) == 0) return;

    int rssi = advertisedDevice->getRSSI();
    int8_t power = oBeacon.getSignalPower();
    float distance = calculateAccuracy(power, rssi);
    // float distance = getAverageDistance(uuid, calculateAccuracy(power,
    // rssi));

    if(mqttClient.connected())
      sendDeviceMqtt(uuid, name, distance);
    else
      sprintf("Found device %s but MQTT is not connected", name);
  }
};

void publishTelemetry(NimBLEScanResults devices) {
  if (!mqttClient.connected()) return;
  char msg[100];
  int uptime = (int)(esp_timer_get_time() / 1000000);
  sprintf(msg,
          "{ \"results_last_scan\": \"%i\", \"free_heap\": \"%i\", \"uptime\": \"%i\" }",
          devices.getCount(), ESP.getFreeHeap(), uptime);
  ws.printfAll(msg);
  uint16_t msg_error;
  msg_error = mqttClient.publish(telemetry_topic, 1, false, msg);
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
  WiFi.onEvent(WiFiGotIP, SYSTEM_EVENT_STA_GOT_IP);
  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_AP_STACONNECTED);
  WiFi.onEvent(WiFiStationDisconnected, SYSTEM_EVENT_AP_STADISCONNECTED);

  delay(1000);
  WiFi.begin(ssid, password);
  delay(3000);
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
  strcpy(scan_topic, mqtt_scan_prefix);
  strcat(scan_topic, settings["device"]["room"]);
  strcpy(telemetry_topic, mqtt_telemetry_prefix);
  strcat(telemetry_topic, settings["device"]["room"]);

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
    char json[1000];
    serializeJson(settings, json);
    request->send(200, "application/json", json);
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
    char json[1000];
    serializeJson(devices, json);
    request->send(200, "application/json", json);
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
  delay(500);

  if (devices_set_up() == false) write_to_logs("No devices set up. Not scanning.");
}

void loop() {
  if ((WiFi.status() == WL_DISCONNECTED) && (wifi_ap_result == false)) {
    delay(5000);
    WiFi_Controller();
  } else if ((WiFi.status() == WL_CONNECTED)) {
    // Reset Wifi Error Counter when the connection is working
    if (wifi_errors > 0) wifi_errors = 0;

    if (devices.size() == 0) {
      write_to_logs("No devices set up. Not scanning.");
      delay(5000);
      return;
    }
    // Scanner
    if (pBLEScan->isScanning()) return;

    int scanTime = settings["bluetooth"]["scan_time"];
    Serial.println("Scanning...");
    pBLEScan->start(scanTime, publishTelemetry, false);
    Serial.println("_____________________________________");
    pBLEScan->clearResults(); // delete results fromBLEScan buffer to
                              // release memory
  }
}