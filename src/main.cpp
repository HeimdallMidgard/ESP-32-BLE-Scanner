// Arduino
#include <Arduino.h>
#include <ArduinoJson.h>  //JSON for Saving Values and Sending Data over MQTT

// SPIFFS
#include "SPIFFS.h"

// BLE
#include <NimBLEAdvertisedDevice.h>
#include <NimBLEDevice.h>

#include "NimBLEBeacon.h"
#include "NimBLEEddystoneTLM.h"
#include "NimBLEEddystoneURL.h"

// Webserver
#include "ESPAsyncWebServer.h"

// Connection
#include <AsyncMqttClient.h>
#include <WiFi.h>

// Scanner
#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00) >> 8) + (((x)&0xFF) << 8))

// Scanner Variables
BLEScan *pBLEScan;

// MQTT MSG
char logs[255];
char log_msg[255];
char mqtt_msg[120];
uint16_t msg_error;

char scan_topic[60];
const char *mqtt_scan_prefix = "ESP32 BLE Scanner/Scan/";
const char *status_topic = "ESP32 BLE Scanner/Status/";

const char settingsFile[15] = "/settings.json";
const char devicesFile[15] = "/devices.json";

StaticJsonDocument<1024> settings;
StaticJsonDocument<1024> devices;

// IPAddress ip(192, 168, 1, 177);  // not in use

boolean wifi_ap_result;

// Wifi
WiFiClient espClient;

// MQTT
AsyncMqttClient mqttClient;

// Webserver
AsyncWebServer server(80);
int wifi_errors = 0;

// END OF DEFINITION

void reboot() {
  Serial.println("Rebooting.");
  delay(1000);
  ESP.restart();
}

//
// Settings functions
//

String loadFile(const char *filename) {
  File file = SPIFFS.open(filename);
  if (!file) {
    Serial.printf("Unable to load %s", filename);
  }
  String data = file.readString();
  file.close();
  return data;
}

void saveJson(StaticJsonDocument<1024> json, const char *filename) {
  File file = SPIFFS.open(filename, "w");
  if (file) {
    serializeJson(json, file);
    Serial.printf("Saved to %s: ", filename);
    serializeJson(json, Serial);
    Serial.println();
  }
}

void saveSettings() { saveJson(settings, settingsFile); }

void loadSettings() {
  String data = loadFile(settingsFile);

  DeserializationError json_error = deserializeJson(settings, data);
  if (json_error) {
    Serial.printf("deserializeJson() for %s failed: \n", settingsFile);
    Serial.println(json_error.c_str());
    reboot();
  } else {
    Serial.printf("Loaded %s: ", settingsFile);
    serializeJson(settings, Serial);
    Serial.println();
  }
}

void saveDevices() { saveJson(devices, devicesFile); }

void loadDevices() {
  String data = loadFile(devicesFile);

  DeserializationError json_error = deserializeJson(devices, data);
  if (json_error) {
    Serial.printf("deserializeJson() for %s failed: \n", devicesFile);
    Serial.println(json_error.c_str());
    reboot();
  } else {
    Serial.printf("Loaded %s: ", devicesFile);
    serializeJson(devices, Serial);
    Serial.println();
  }
}

bool initSetting(const char key1[], const char key2[],
                 const char defaultValue[]) {
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
  if (initSetting("mqtt", "port", "1883")) changed = true;
  if (initSetting("mqtt", "user", "")) changed = true;
  if (initSetting("mqtt", "password", "")) changed = true;
  return changed;
}

bool initSettingsUi() {
  bool changed = false;
  if (initSetting("ui", "style", "default")) changed = true;
  return changed;
}

bool initSettingsBluetooth() {
  bool changed = false;
  if (initSetting("bluetooth", "scan_time", "5")) changed = true;
  if (initSetting("bluetooth", "scan_interval", "300")) changed = true;
  return changed;
}

bool initSettings() {
  serializeJson(settings, Serial);
  bool changed = false;
  if (initSettingsDevice()) changed = true;
  if (initSettingsNetwork()) changed = true;
  if (initSettingsMqtt()) changed = true;
  if (initSettingsUi()) changed = true;
  if (initSettingsBluetooth()) changed = true;
  return changed;
}

bool migrateSettings() {
  Serial.println("Migrating settings. Currently:");
  serializeJson(settings, Serial);
  Serial.print("\n");

  settings["device"]["room"] = settings["room"];
  settings.remove("room");
  settings["network"]["ssid"] = settings["ssid"];
  settings.remove("ssid");
  settings["network"]["password"] = settings["password"];
  settings.remove("password");
  settings["mqtt"]["host"] = settings["mqttServer"];
  settings.remove("mqttServer");
  settings["mqtt"]["port"] = settings["mqttPort"];
  settings.remove("mqttPort");
  settings["mqtt"]["user"] = settings["mqttUser"];
  settings.remove("mqttUser");
  settings["mqtt"]["password"] = settings["mqttPassword"];
  settings.remove("mqttPassword");
  return true;
}

bool migrateDevices() {
  Serial.println("Migrating devices, currently:");
  serializeJson(devices, Serial);
  Serial.print("\n");

  char devicesString[600] = "[";
  for (int i = 1; i < 4; i++) {
    char uuid_key[13];
    sprintf(uuid_key, "device_uuid%i", i);
    const char *uuid = devices[uuid_key];
    if (strlen(uuid) == 0) {
      break;
    }

    char name_key[13];
    sprintf(name_key, "device_name%i", i);
    const char *name = devices[name_key];
    if (strlen(name) == 0) {
      break;
    }

    if (strlen(devicesString) > 1) {
      strcat(devicesString, ",");
    }
    char deviceString[100];
    sprintf(deviceString, "{\"name\":\"%s\",\"uuid\":\"%s\"}", name, uuid);
    strcat(devicesString, deviceString);
  }
  strcat(devicesString, "]");

  Serial.println("Migrating to:");
  Serial.println(devicesString);
  ;
  return true;
}

bool checkMigrations() {
  bool migrated = false;
  if (settings["ssid"]) {
    migrated = migrateSettings();
  }
  if (devices["device_uuid1"]) {
    migrated = migrateDevices();
  }
  return migrated;
}

bool savePostedJson(AsyncWebParameter *p, StaticJsonDocument<1024> json,
                    const char *filename) {
  json.clear();
  DeserializationError json_error = deserializeJson(json, p->value());
  if (json_error) {
    write_to_logs("DeserializeJson() failed: ");
    write_to_logs(json_error.c_str());
    write_to_logs(" \n");
  } else {
    saveJson(json, filename);
    return true;
  }
  return false;
}

//
// FUNCTIONS
//

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("User connected to Hotspot");
}

void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("User disconnected");
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info) {
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void WiFi_Controller() {
  wifi_errors++;
  Serial.println(" ");
  Serial.print("Wifi Errors: ");
  Serial.println(wifi_errors);
  delay(2500);

  if ((wifi_errors < 10) && (wifi_ap_result == false)) {
    Serial.println("Disconnected from WiFi access point");
    Serial.println("Trying to Reconnect");

    const char *ssid = settings["wifi"]["ssid"];
    const char *password = settings["wifi"]["password"];

    WiFi.begin(ssid, password);
    delay(500);

  } else if ((wifi_errors > 10) && (wifi_ap_result == false)) {
    Serial.println(" ");
    Serial.println("Starting AP");

    WiFi.mode(WIFI_AP);
    delay(500);
    WiFi.softAP("ESP32-BLE-Scanner");

    wifi_ap_result = WiFi.softAP("ESP32-BLE-Scanner");

    server.begin();  // Start Webserver

    delay(500);
    Serial.print("Event IP address: ");
    Serial.println(WiFi.softAPIP());
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

void write_to_logs(const char *new_log_entry) {
  strncat(logs, new_log_entry,
          sizeof(logs));  // Copies to logs to publish in Weblog
  Serial.println(new_log_entry);
}

void check_mqtt_msg(uint16_t error_state) {
  if (error_state == 0) {
    write_to_logs("Error publishing MQTT Message \n");
  }
}

// Distance Calculation
float calculateAccuracy(float txCalibratedPower, float rssi) {
  float ratio_db = txCalibratedPower - rssi;
  float ratio_linear = pow(10, ratio_db / 10);

  float r = sqrt(ratio_linear);
  r = r / 20;
  return r;
}

// Scanner

bool isBeacon(std::string strManufacturerData) {
  uint8_t cManufacturerData[100];
  strManufacturerData.copy((char *)cManufacturerData,
                           strManufacturerData.length(), 0);
  return (strManufacturerData.length() == 25 && cManufacturerData[0] == 0x4C &&
          cManufacturerData[1] == 0x00);
}

const char *getDeviceName(const char *uuid) {
  const char *name;
  if (strlen(uuid) == 0) {
    return name;
  }
  for (size_t i = 0; i < devices.size(); i++) {
    if (strcmp(uuid, devices[i]["uuid"]) == 0) {
      return devices[i]["name"];
    }
  }
  return name;
}

void sendDeviceMqtt(const char *uuid, const char *name, float distance) {
  if (strlen(uuid) == 0 || strlen(name) == 0) {
    return;
  }
  sprintf(mqtt_msg, "{ \"id\": \"%s\", \"name\": \"%s\", \"distance\": %f } \n",
          uuid, name, distance);
  // sprintf(mqtt_msg, "{ \"id\": \"%s\", \"name\": \"%s\", \"distance\":
  // %f, \"rssi\": %i, \"signalPower\": %i } \n", uuid, name, distance,
  // rssi, power ); Send Scanning logs to Webserver Mainpage / Index Page
  // | write_to_logs(mqtt_msg); causing bug
  server.on("/send_scan_results", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", mqtt_msg);
  });
  // Publish to MQTT
  msg_error = mqttClient.publish(scan_topic, 1, false, mqtt_msg);
  check_mqtt_msg(msg_error);
  Serial.println(mqtt_msg);
  //*mqtt_msg = '\0'; // Clear memory
}

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks {
  void onResult(BLEAdvertisedDevice *advertisedDevice) {
    if (advertisedDevice->haveManufacturerData() == false) {
      return;
    }
    std::string strManufacturerData = advertisedDevice->getManufacturerData();
    if (!isBeacon(strManufacturerData)) {
      return;
    }
    BLEBeacon oBeacon = BLEBeacon();
    oBeacon.setData(strManufacturerData);
    const char *uuid = oBeacon.getProximityUUID().toString().c_str();
    const char *name = getDeviceName(uuid);
    int rssi = advertisedDevice->getRSSI();
    int8_t power = oBeacon.getSignalPower();
    float distance = calculateAccuracy(power, rssi);
    sendDeviceMqtt(uuid, name, distance);
  }
};

// connect to mqtt

void connectToMqtt() {
  write_to_logs("Connecting to MQTT... \n");
  mqttClient.connect();
}

void onMqttConnect(bool sessionPresent) {
  write_to_logs("Connected to MQTT. \n");
}

void onMqttDisconnect(AsyncMqttClientDisconnectReason reason) {
  write_to_logs("Disconnected from MQTT. \n");
  if (WiFi.isConnected()) {
    delay(1000);
    connectToMqtt();
  }
}

//
//
//
//  SETUP
//
//
//

void startWifi() {
  const char *ssid = settings["network"]["ssid"];
  const char *password = settings["network"]["password"];
  const char *hostname = settings["network"]["hostname"];

  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE);  // Dyn IP
  // WiFi.config(ip, INADDR_NONE, INADDR_NONE, INADDR_NONE);  // Fixed IP
  WiFi.setHostname(hostname);
  WiFi.begin(ssid, password);

  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, SYSTEM_EVENT_STA_GOT_IP);
  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_AP_STACONNECTED);
  WiFi.onEvent(WiFiStationDisconnected, SYSTEM_EVENT_AP_STADISCONNECTED);
}

void startMqtt() {
  const char *hostname = settings["network"]["hostname"];
  const char *mqttHost = settings["mqtt"]["host"];
  long mqttPort = settings["mqtt"]["port"];
  const char *mqttUser = settings["mqtt"]["user"];
  const char *mqttPassword = settings["mqtt"]["password"];

  // Combine Room and Prefix to MQTT topic
  strcpy(scan_topic, mqtt_scan_prefix);
  strcat(scan_topic, settings["device"]["room"]);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.setWill(status_topic, 1, true, "offline");

  randomSeed(micros());

  // Start MQTT Connection
  mqttClient.setServer(mqttHost, mqttPort);
  mqttClient.setClientId(hostname);

  if (strlen(mqttUser) == 0) {
    Serial.println("No MQTT User set");
  } else {
    mqttClient.setCredentials(mqttUser, mqttPassword);
  }

  delay(500);
  connectToMqtt();
  delay(500);

  // Publish online status
  msg_error = mqttClient.publish(status_topic, 1, true, "online");
  check_mqtt_msg(msg_error);
}

void startScanner() {
  int interval = atoi(settings["bluetooth"]["scan_interval"]);
  int window = (int)(interval * 0.9);
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan();  // create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  // active scan uses more power, but get results faster
  pBLEScan->setActiveScan(true);
  pBLEScan->setInterval(interval);
  pBLEScan->setWindow(window);  // less or equal setInterval value
}

void startWebServer() {
  // Serve css
  server.serveStatic("/css", SPIFFS, "/web/css");

  // Serve js
  server.serveStatic("/js", SPIFFS, "/web/js");

  // Webserver Mainpage
  server.on("/", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/web/index.html", String(), false);
  });

  // Send Scanning logs to Webserver Mainpage / Index Page
  server.on("/send_logs", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(200, "text/plain", logs);
    *logs = '\0';  // Release memory
  });

  // Settings page
  server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/web/settings.html", String(), false);
  });

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
      saved = savePostedJson(request->getParam("settings", true), settings,
                             settingsFile);
    } else {
      Serial.println("Has no settings param");
    }
    if (saved) {
      request->send(200);
      reboot();
    } else {
      Serial.println("Unable to save settings");
      request->send(503);
    }
  });

  // Reset
  server.on("/api/reset", HTTP_GET, [](AsyncWebServerRequest *request) {
    Serial.println("Rebooting.");
    delay(1000);
    ESP.restart();
  });

  // Load Devices Page
  server.on("/devices", HTTP_GET, [](AsyncWebServerRequest *request) {
    request->send(SPIFFS, "/web/devices.html", String(), false);
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
      saved = savePostedJson(request->getParam("devices", true), devices,
                             devicesFile);
    } else {
      Serial.println("Has no devices param");
    }
    if (saved) {
      request->send(200);
      reboot();
    } else {
      Serial.println("Unable to save devices");
      request->send(503);
    }
  });

  // Start Webserver
  server.begin();
}

void setup() {
  Serial.begin(115200);
  Serial.println("");
  write_to_logs("Starting...\n");

  // Initialize SPIFFS
  if (!SPIFFS.begin(true)) {
    write_to_logs("Error initializing SPIFFS \n");
    while (true) {
    }
  }

  loadSettings();
  loadDevices();

  if (initSettings() || checkMigrations()) {
    saveSettings();
    saveDevices();
    reboot();
  }

  startWifi();
  startWebServer();
  startMqtt();
  startScanner();
  delay(500);
}

void loop() {
  if ((WiFi.status() == WL_DISCONNECTED) && (wifi_ap_result == false)) {
    Serial.println(WiFi.status());
    WiFi_Controller();

  } else if ((WiFi.status() == WL_CONNECTED)) {
    // Scanner
    if (!pBLEScan->isScanning()) {
      int scanTime = (int)settings["bluetooth"]["scan_time"];
      Serial.println("Scanning...");
      pBLEScan->start(scanTime, false);
      Serial.println("_____________________________________");
      // delete results fromBLEScan buffer to release memory
      pBLEScan->clearResults();
    }
  }
}