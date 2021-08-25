// Arduino
#include <Arduino.h>
#include <ArduinoJson.h> //JSON for Saving Values and Sending Data over MQTT

// SPIFFS
#include "SPIFFS.h"

// BLE
#include <NimBLEDevice.h>
#include <NimBLEAdvertisedDevice.h>
#include "NimBLEEddystoneURL.h"
#include "NimBLEEddystoneTLM.h"
#include "NimBLEBeacon.h"

//Webserver
#include "ESPAsyncWebServer.h"

// Connection
#include <WiFi.h>
#include <AsyncMqttClient.h>

// Scanner
#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00) >> 8) + (((x)&0xFF) << 8))

// Scanner Variables
int scanTime = 5; //In seconds //5
BLEScan *pBLEScan;

//MQTT MSG
char logs[255];
char log_msg[255];
char mqtt_msg[120];
uint16_t msg_error;

char scan_topic[60];
const char *mqtt_scan_prefix = "ESP32 BLE Scanner/Scan/";
const char *status_topic = "ESP32 BLE Scanner/Status/";

StaticJsonDocument<600> settings;
StaticJsonDocument<600> devices;

//IPAddress ip(192, 168, 1, 177);  // not in use

boolean wifi_ap_result;

// Wifi
WiFiClient espClient;

// MQTT
AsyncMqttClient mqttClient;

//Webserver
AsyncWebServer server(80);
int wifi_errors = 0;

// END OF DEFINITION



//
// FUNCTIONS
//

void WiFiStationConnected(WiFiEvent_t event, WiFiEventInfo_t info){
   
  Serial.println("User connected to Hotspot");
}
 
void WiFiStationDisconnected(WiFiEvent_t event, WiFiEventInfo_t info){
   
  Serial.println("User disconnected");
}

void WiFiGotIP(WiFiEvent_t event, WiFiEventInfo_t info){
  Serial.println("WiFi connected");
  Serial.print("IP address: ");
  Serial.println(WiFi.localIP());
}

void WiFi_Controller(){

    wifi_errors ++;
    Serial.println(" ");
    Serial.print("Wifi Errors: ");
    Serial.println(wifi_errors);
    delay(2500);

    const char *ssid = settings["wifi"]["ssid"];
    const char *password = settings["wifi"]["password"];

    if ((wifi_errors < 10) && (wifi_ap_result == false)) {
          Serial.println("Disconnected from WiFi access point");
          Serial.println("Trying to Reconnect");
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


void write_to_logs(const char* new_log_entry) {

  strncat(logs, new_log_entry, sizeof(logs));   // Copies to logs to publish in Weblog
  Serial.println(new_log_entry);
}


void check_mqtt_msg(uint16_t error_state) {
                        
      if (error_state == 0) { 
          write_to_logs("Error publishing MQTT Message \n");
      }
}


// Distance Calculation
float calculateAccuracy(float txCalibratedPower, float rssi)
 {
    float ratio_db = txCalibratedPower - rssi;
    float ratio_linear = pow(10, ratio_db / 10);

    float r = sqrt(ratio_linear);
    r = r /20;
    return r;
}



// Scanner

class MyAdvertisedDeviceCallbacks : public BLEAdvertisedDeviceCallbacks
{

    void onResult(BLEAdvertisedDevice *advertisedDevice)
    {

        if (advertisedDevice->haveManufacturerData() == true)
        {

          std::string strManufacturerData = advertisedDevice->getManufacturerData();
          uint8_t cManufacturerData[100];
          strManufacturerData.copy((char *)cManufacturerData, strManufacturerData.length(), 0);

          if (strManufacturerData.length() == 25 && cManufacturerData[0] == 0x4C && cManufacturerData[1] == 0x00)   // Beacon identifikation
          {
            BLEBeacon oBeacon = BLEBeacon();
            oBeacon.setData(strManufacturerData);

            const char *uuid = oBeacon.getProximityUUID().toString().c_str();
            const char *name;

            for(size_t i=0; i<devices.size(); i++){
              const char *deviceUuid = devices[i]["uuid"];
              if(strcmp(deviceUuid, uuid)==0){
                name = devices[i]["name"];
                break;
              }
            }
            if(!name){ return; }

            int rssi = advertisedDevice->getRSSI();
            int8_t power = oBeacon.getSignalPower();
            float distance = calculateAccuracy(power, rssi);
            sprintf(mqtt_msg, "{ \"id\": \"%s\", \"name\": \"%s\", \"distance\": %f, \"rssi\": %i, \"signalPower\": %i } \n", uuid, name, distance, rssi, power );
            // Send Scanning logs to Webserver Mainpage / Index Page  | write_to_logs(mqtt_msg); causing bug
            server.on("/send_scan_results", HTTP_GET, [](AsyncWebServerRequest *request){
              request->send(200, "text/plain", mqtt_msg);
            });
            // Publish to MQTT
            msg_error = mqttClient.publish(scan_topic, 1, false, mqtt_msg);
            check_mqtt_msg(msg_error);
            Serial.println(mqtt_msg);
            //*mqtt_msg = '\0'; // Clear memory
          }
        }
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

void reboot()
{
  Serial.println("Rebooting.");
  delay(1000);
  ESP.restart();
}

bool savePostedJson(AsyncWebParameter* p, StaticJsonDocument<600> json, const char filename[])
{
  DeserializationError json_error = deserializeJson(json, p->value());
  if(json_error)
  {
    write_to_logs("DeserializeJson() failed: ");
    write_to_logs(json_error.c_str());
    write_to_logs(" \n");
  }
  else
  {
    File file = SPIFFS.open(filename,"w");
    if(file)
    {
      if(serializeJson(json, file) > 0)
      {
        Serial.printf("Saved to %s\n", filename);
        return true;
      }
    }
    else
    {
      Serial.printf("File not found: %s", filename);
    } file.close();
  }
  return false;
}


//
//
//
//  SETUP
//
//
//

void setup()
{

  Serial.begin(115200);
  Serial.println("");
  write_to_logs("Starting...\n");


// Initialize SPIFFS
  if(!SPIFFS.begin(true)) {
    write_to_logs("Error initializing SPIFFS \n");
    while(true){} // 
  }


  // Load Settings from SPIFFS for WIFI Start
  File settingsFile = SPIFFS.open("/settings.json");
  if (settingsFile)
  {
    DeserializationError json_error = deserializeJson(settings, settingsFile);
    if (json_error)
    {
      Serial.println("deserializeJson() for settings.json failed: ");
      Serial.println(json_error.c_str());
      reboot();
    }
  } settingsFile.close();

  File devicesFile = SPIFFS.open("/devices.json");
  if (devicesFile)
  {
    DeserializationError json_error = deserializeJson(devices, devicesFile);
    if (json_error)
    {
      Serial.println("deserializeJson() for devices.json failed: ");
      Serial.println(json_error.c_str());
      reboot();
    }
  } devicesFile.close();

  const char *ssid         = settings["network"]["ssid"];
  const char *password     = settings["network"]["password"];
  const char *hostname     = settings["network"]["hostname"];
  const char *mqttHost     = settings["mqtt"]["host"];
  long        mqttPort     = settings["mqtt"]["port"];
  const char *mqttUser     = settings["mqtt"]["user"];
  const char *mqttPassword = settings["mqtt"]["password"];

  // Combine Room and Prefix to MQTT topic
  strcpy(scan_topic,mqtt_scan_prefix);
  strcat(scan_topic,settings["device"]["room"]);

  Serial.println(" ");
  Serial.println("SPIFFS Data:");
  Serial.println(ssid);
  Serial.println(password);
  Serial.println(hostname);
  Serial.println(mqttHost);
  Serial.println(mqttPort);
  Serial.println(mqttUser);
  Serial.println(mqttPassword);
  Serial.println(scan_topic);
  Serial.println("__________________________________________________________");

// Starting WIFI

  WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE); // Dyn IP
  //WiFi.config(ip, INADDR_NONE, INADDR_NONE, INADDR_NONE);  // Fixed IP
  WiFi.setHostname(hostname);
  WiFi.begin(ssid, password);

  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, SYSTEM_EVENT_STA_GOT_IP);
  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_AP_STACONNECTED);
  WiFi.onEvent(WiFiStationDisconnected, SYSTEM_EVENT_AP_STADISCONNECTED);

  mqttClient.onConnect(onMqttConnect);
  mqttClient.onDisconnect(onMqttDisconnect);
  mqttClient.setWill(status_topic, 1, true, "offline");

  randomSeed(micros());


// Start MQTT Connection
  mqttClient.setServer(mqttHost, mqttPort);
  mqttClient.setClientId(hostname);

  if(strlen(mqttUser) == 0) {
      Serial.println("No MQTT User set");
  }else{
      mqttClient.setCredentials(mqttUser, mqttPassword);
  }  
  
  delay(500);
  connectToMqtt();
  delay(500);

  // Publish online status
  msg_error = mqttClient.publish(status_topic, 1, true, "online");
  check_mqtt_msg(msg_error);


// Set up the scanner
  write_to_logs("Starting to Scan... \n");
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(90); // less or equal setInterval value

  delay(500);

//
//
//  Webpage Settings
//
//

// Serve css
server.serveStatic("/css", SPIFFS, "/web/css");

// Serve js
server.serveStatic("/js", SPIFFS, "/web/js");

// Webserver Mainpage
server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false);
}); 

  // Load CSS
server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    char *stylesheet = "/mini-";
    strcat(stylesheet, settings["ui"]["style"]);
    strcat(stylesheet, ".min.css");
    request->send(SPIFFS, stylesheet, "text/css");
});

// Send Scanning logs to Webserver Mainpage / Index Page
server.on("/send_logs", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(200, "text/plain", logs );
    *logs = '\0'; //Release memory
});

// Settings page
server.on("/settings", HTTP_GET, [](AsyncWebServerRequest *request){
  request->send(SPIFFS, "/web/settings.html", String(), false);
});

// Send settings json
server.on("/api/settings", HTTP_GET, [](AsyncWebServerRequest *request){
  char json[1000];
  serializeJson(settings, json);
  request->send(200, "application/json", json);
});

// Save settings json
server.on("/api/settings", HTTP_POST, [](AsyncWebServerRequest *request){
  Serial.println("POST Request to /api/settings");
  bool saved = false;
  if(request->hasParam("settings", true)){
    saved = savePostedJson(request->getParam("settings", true), settings, "/settings.json");
  }
  else { Serial.println("Has no settings param"); }
  if(saved)
  {
    request->send(200);
  }
  else
  {
    Serial.println("Unable to save settings");
    request->send(503);
  }
});

// Reset
server.on("/api/reset", HTTP_GET, [](AsyncWebServerRequest *request){
  Serial.println("Rebooting.");
  delay(1000);
  ESP.restart();
});


// Load Devices Page
server.on("/devices", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/web/devices.html", String(), false);
});

// Send devices json
server.on("/api/devices", HTTP_GET, [](AsyncWebServerRequest *request){
  char json[1000];
  serializeJson(devices, json);
  Serial.println(json);
  request->send(200, "application/json", json);
});

// Save devices json
server.on("/api/devices", HTTP_POST, [](AsyncWebServerRequest *request){
  Serial.println("POST Request to /api/devices");
  bool saved = false;
  if(request->hasParam("devices", true)){
    saved = savePostedJson(request->getParam("devices", true), devices, "/devices.json");
  }
  else { Serial.println("Has no devices param"); }
  if(saved)
  {
    request->send(200);
  }
  {
    Serial.println("Unable to save devices");
    request->send(503);
  }
});


// Start Webserver
server.begin();


} // SETUP END



void loop()
{
      if ((WiFi.status() == WL_DISCONNECTED) && (wifi_ap_result == false)) {

        Serial.println(WiFi.status());
          WiFi_Controller();

          
      }else if ((WiFi.status() == WL_CONNECTED)) {

        // Scanner
        if(!pBLEScan->isScanning())
        {
          BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
          Serial.println("_____________________________________");
          pBLEScan->clearResults(); // delete results fromBLEScan buffer to release memory
        }
      }

}