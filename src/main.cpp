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
#include <PubSubClient.h>

// Scanner
#define ENDIAN_CHANGE_U16(x) ((((x)&0xFF00) >> 8) + (((x)&0xFF) << 8))

// Scanner Variables
int scanTime = 5; //In seconds
BLEScan *pBLEScan;

//MQTT MSG
char log_msg[120];
char mqtt_msg[120];

// Wifi and MQTT Variables
const char* ssid;
const char* password;
const char* mqttServer;
long        mqttPort;
const char* mqttUser;
const char* mqttPassword;
const char* room;


char hostname[60];
const char *hostname_prefix = "ESP 32 BLE Scanner ";

char scan_topic[100];
const char *mqtt_scan_prefix = "ESP32 BLE Scanner/Scan/";


IPAddress ip(192, 168, 1, 177);

boolean wifi_ap_result;


// Wifi and MQTT
WiFiClient espClient;
PubSubClient client(espClient);

//Webserver
AsyncWebServer server(80);


int wifi_errors = 0;

// END OF DEFINITION





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
    delay(2000);

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

            // Read saved Devices and check for Errors
              if(!SPIFFS.begin(true)) {
                Serial.println("Error initializing SPIFFS");
                while(true){} // 
              }

              File file = SPIFFS.open("/devices.json");
              if(file) {

                  StaticJsonDocument<600> doc;
                  DeserializationError json_error = deserializeJson(doc, file);
                  
                  // Check for Json Errors
                  if (json_error) {
                    Serial.println("deserializeJson() failed: ");
                    Serial.println(json_error.c_str());
                  }
                    
                    for (byte i = 1; i < 3; i = i + 1) {              
                      
                      // Modify Device ID for later checkup
                      String namey ="device_name";
                      String devicey ="device_uuid";
                      devicey = devicey + i;
                      namey  = namey  + i;
                      String device = doc[namey];

                      // DEBUG
                      //Serial.println(namey);
                      //Serial.println(devicey);
                      //Serial.print("i: ");
                      //Serial.println(i);

                        // check for the known devices
                         if ( oBeacon.getProximityUUID().toString() == doc[devicey]  ) {

                            // Debug
                            // Serial.printf("ID: %04X Major: %d Minor: %d UUID: %s Power: %d\n", oBeacon.getManufacturerId(), ENDIAN_CHANGE_U16(oBeacon.getMajor()), ENDIAN_CHANGE_U16(oBeacon.getMinor()), oBeacon.getProximityUUID().toString().c_str(), oBeacon.getSignalPower() );
                            
                            float distance = calculateAccuracy(oBeacon.getSignalPower(), advertisedDevice->getRSSI());
                          
                            sprintf(mqtt_msg, "{ \"id\": \"%s\", \"name\": \"%s\", \"distance\": %f } \n", oBeacon.getProximityUUID().toString().c_str(), device.c_str(), distance );
                            
                            
                            sprintf(log_msg, "Name: %s Id: %s Distance: %f TX Power: %d RSSI: %d \n", device.c_str(), oBeacon.getProximityUUID().toString().c_str(), distance, oBeacon.getSignalPower(), advertisedDevice->getRSSI() );
                            
                            // Publish to Console and Web UI
                            Serial.printf(log_msg);

                            // Publish to MQTT
                            client.publish(scan_topic, mqtt_msg);
                         }
                    }
                file.close();
                }
            }
        }
        return;
      }
};


// Reconnect Settings
void reconnect() {
  while (!client.connected()) {
    Serial.println("Attempting MQTT connection...");
    if (client.connect("ESP32_BLE_presence_detector", mqttUser, mqttPassword )) {      //Name des MQTT Clients fehlt
      Serial.println("connected");
      client.publish("esp/test", "Reconnect Bla Bla Bla");

    } else {
      Serial.print("failed with state ");
      Serial.print(client.state());
      Serial.println(" try again in 5 seconds");
      delay(5000);
      ESP.restart();
    }
  }
}


// Load Settings
String processor(const String& var){
  
  if(!SPIFFS.begin(true)) {
    Serial.println("Error initializing SPIFFS");
    while(true){} // 
  }

  File file = SPIFFS.open("/settings.json");
  if(file) {
    
      StaticJsonDocument<600> doc;
      DeserializationError json_error = deserializeJson(doc, file);  // deserializeJson
      
        // Check for Json Errors
        if (json_error) {
        Serial.println("deserializeJson() failed: ");
        Serial.println(json_error.c_str());
        }
        
        File file2 = SPIFFS.open("/devices.json");
        if(file2) {
          
              StaticJsonDocument<600> doc2;
              DeserializationError json_error = deserializeJson(doc2, file2);
              
              // Check for Json Errors
              if (json_error) {
              Serial.println("deserializeJson() failed: ");
              Serial.println(json_error.c_str());
              }

              if(var == "SSID"){
                return doc["ssid"];
              }
              else if(var == "PASSWORD"){
                return doc["password"];
              }
              else if(var == "ROOM"){
                return doc["room"];
              }
              else if(var == "MQTTSERVER"){
                return doc["mqttServer"];
              }
              else if(var == "MQTTPORT"){
                return doc["mqttPort"];
              }
              else if(var == "MQTTUSER"){
                return doc["mqttUser"];
              }
              else if(var == "MQTTPASSWORD"){
                return doc["mqttPassword"];
              }
              else if(var == "DEVICENAME1"){
                return doc2["device_name1"];
              }
              else if(var == "DEVICENAME2"){
                return doc2["device_name2"];
              }
              else if(var == "DEVICENAME3"){
                return doc2["device_name3"];
              }
              else if(var == "UUID1"){
                return doc2["device_uuid1"];
              }
              else if(var == "UUID2"){
                return doc2["device_uuid2"];
              }
              else if(var == "UUID3"){
                return doc2["device_uuid3"];
              }
          } file2.close();
      } file.close();
  return String();
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
  Serial.println("Starting...");


// Initialize SPIFFS
  if(!SPIFFS.begin(true)) {
    Serial.println("Error initializing SPIFFS");
    while(true){} // 
  }


  // Load Settings from SPIFFS for WIFI Start
  File file = SPIFFS.open("/settings.json");
  if(file) {
    
        StaticJsonDocument<600> doc;
        DeserializationError json_error = deserializeJson(doc, file);
        
        // Check for Json Errors
        if (json_error) {
          Serial.println("deserializeJson() failed: ");
          Serial.println(json_error.c_str());
        }
        
        ssid = doc["ssid"];
        password = doc["password"];
        mqttServer = doc["mqttServer"];
        mqttPort = doc["mqttPort"];
        mqttUser = doc["mqttUser"];
        mqttPassword = doc["mqttPassword"];
        room = doc["room"];

        // Combine Room and Prefix to Hostname
        strcpy(hostname,hostname_prefix);
        strcat(hostname,room);
        
        // Combine Room and Prefix to MQTT topic
        strcpy(scan_topic,mqtt_scan_prefix);
        strcat(scan_topic,room);

        // Print settings in Serial Connection
        Serial.println(" ");
        Serial.println("SPIFFS Data:");
        Serial.println(ssid);
        Serial.println(password);
        Serial.println(mqttServer);
        Serial.println(mqttPort);
        Serial.println(mqttUser);
        Serial.println(mqttPassword);
        Serial.println(room);
        Serial.println(hostname);
        Serial.println(scan_topic);
        Serial.println("__________________________________________________________");

  }  file.close();


// Starting WIFI
 WiFi.config(INADDR_NONE, INADDR_NONE, INADDR_NONE, INADDR_NONE); // Dyn IP
 //WiFi.config(ip, INADDR_NONE, INADDR_NONE, INADDR_NONE);  // Fixed IP
 WiFi.setHostname(hostname);
 WiFi.begin(ssid, password);

  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_STA_CONNECTED);
  WiFi.onEvent(WiFiGotIP, SYSTEM_EVENT_STA_GOT_IP);
  WiFi.onEvent(WiFiStationConnected, SYSTEM_EVENT_AP_STACONNECTED);
  WiFi.onEvent(WiFiStationDisconnected, SYSTEM_EVENT_AP_STADISCONNECTED);


/*
 Serial.printf("Connecting to WiFi..");
  while (WiFi.status() != WL_CONNECTED) {
    delay(500);
    Serial.print(".");
    WiFiStationDisconnected();
  }
*/

  randomSeed(micros());


// Start MQTT Connection

  client.setServer(mqttServer, mqttPort);
  delay(2000);
  if (!client.connected()) {
    Serial.println("Connecting to MQTT...");

    if (client.connect("ESP32_BLE_presence_detector", mqttUser, mqttPassword )) {

      Serial.println("connected");

    } else {

      Serial.println("failed with state ");
      Serial.print(client.state());
      delay(2000);

    }
  }

//  Set up the scanner
  Serial.println("Starting to Scan...");
  BLEDevice::init("");
  pBLEScan = BLEDevice::getScan(); //create new scan
  pBLEScan->setAdvertisedDeviceCallbacks(new MyAdvertisedDeviceCallbacks());
  pBLEScan->setActiveScan(true); //active scan uses more power, but get results faster
  pBLEScan->setInterval(100);
  pBLEScan->setWindow(90); // less or equal setInterval value


//
//
//  Webpage Settings
//
//

// Webserver Mainpage
server.on("/", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/index.html", String(), false, processor);
}); 

  // Load CSS
server.on("/style.css", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/style.css", "text/css");
});

// Send Scanning logs to Webserver Mainpage / Index Page
server.on("/send_logs", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send_P(200, "text/plain", log_msg);
});

// Load Setup Page
server.on("/setup", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/setup.html", String(), false, processor);
});
  
// Save Setup Data in SPIFFS
server.on("/setup_get", HTTP_GET, [] (AsyncWebServerRequest *request) {
        //String message;
        String input_room          = request->getParam("room")->value();
        String input_ssid          = request->getParam("ssid")->value();
        String input_password      = request->getParam("password")->value();
        String input_mqttServer    = request->getParam("mqttServer")->value();
        String input_mqttPort      = request->getParam("mqttPort")->value();
        String input_mqttUser      = request->getParam("mqttUser")->value();
        String input_mqttPassword  = request->getParam("mqttPassword")->value();

        //Debug
        //request->send(200, "text/plain", "Hello, GET: " + input_room + input_ssid + input_password + input_mqttServer + input_mqttPort + input_mqttUser + input_mqttPassword);

        // Saving in SPIFFS
        File outfile = SPIFFS.open("/settings.json","w");
        StaticJsonDocument<1000> doc;
        doc["room"]         = input_room;
        doc["ssid"]         = input_ssid;
        doc["password"]     = input_password;
        doc["mqttServer"]   = input_mqttServer;
        doc["mqttPort"]     = input_mqttPort;
        doc["mqttUser"]     = input_mqttUser;
        doc["mqttPassword"] = input_mqttPassword;

        if(serializeJson(doc, outfile)==0) {
              request->send(200, "text/html", "<div style='text-align:center;'>Failed to save data. Rebooting</div>");
              Serial.println("Failed to write settings to SPIFFS file");
        } else {
        
              // Send MSG with Data
              request->send(200, "text/html", "<div style='text-align:center;'>Settings saved. Rebooting</div>");
              
              //Debug
              Serial.println("New Config set: ");
              Serial.println(input_ssid);
              Serial.println(input_password);
              Serial.println(input_mqttServer);
              Serial.println(input_mqttPort);
              Serial.println(input_mqttUser);
              Serial.println(input_mqttPassword);
              Serial.println(input_room);

        } outfile.close();
      
  Serial.println("Rebooting.");
  delay(1000);
  ESP.restart();

});

// Load Setup Page
server.on("/devices", HTTP_GET, [](AsyncWebServerRequest *request){
    request->send(SPIFFS, "/devices.html", String(), false, processor);
});



// Save Setup Data in SPIFFS
server.on("/devices_get", HTTP_GET, [] (AsyncWebServerRequest *request) {

        //String message;
        String input_device_name1     = request->getParam("device_name1")->value();
        String input_uuid1            = request->getParam("uuid1")->value();
        String input_device_name2     = request->getParam("device_name2")->value();
        String input_uuid2            = request->getParam("uuid2")->value();
        String input_device_name3     = request->getParam("device_name3")->value();
        String input_uuid3            = request->getParam("uuid3")->value();

        //Debug
        //request->send(200, "text/html", "Hello, GET: " + input_device_name1 + input_uuid1 + input_device_name2 + input_uuid2 + input_device_name3 + input_uuid3);

        // Saving in SPIFFS
        File outfile = SPIFFS.open("/devices.json","w");
        StaticJsonDocument<1000> doc;
        doc["device_name1"]   = input_device_name1;
        doc["device_uuid1"]   = input_uuid1;
        doc["device_name2"]   = input_device_name2;
        doc["device_uuid2"]   = input_uuid2;
        doc["device_name3"]   = input_device_name3;
        doc["device_uuid3"]   = input_uuid3;

        if(serializeJson(doc, outfile)==0) {
            request->send(200, "text/html", "<div style='text-align:center;'>Failed to save data. Rebooting</div>");
            Serial.println("Failed to write settings to SPIFFS file");
        } else {

            // Send MSG with Data
            request->send(200, "text/html", "<div style='text-align:center;'>Settings saved. Rebooting</div>");
            
            //Debug
            Serial.println("Devices saved: ");
            Serial.println(input_device_name1);
            Serial.println(input_uuid1);
            Serial.println(input_device_name2);
            Serial.println(input_uuid2);
            Serial.println(input_device_name3);
            Serial.println(input_uuid3);

        } outfile.close();  
      
    Serial.println("Rebooting.");
    delay(1000);
    ESP.restart();

});



// Start server
server.begin();


} // SETUP ENDE



void loop()
{

      client.loop(); // Loop für MQTT und Wifi

      if ((WiFi.status() != WL_CONNECTED) && (wifi_ap_result == false)) {
          //reconnect(); //MQTT
          WiFi_Controller();
          
      }else if ((WiFi.status() == WL_CONNECTED)) {

        // Scanner
          BLEScanResults foundDevices = pBLEScan->start(scanTime, false);
          Serial.println("_____________________________________");
          pBLEScan->clearResults(); // delete results fromBLEScan buffer to release memory
      }

}