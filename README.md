# ESP-32-BLE-Scanner
ESP 32 BLE Scanner for Home Assistant

Hello everyone. Welcome to my first project on github. I will try to support this program as good as I can. Please keep in mind that this is my first project, Iam programming in my free time during my 9 to 5 job and Iam not a professional programmer.

### What does this program do?
It reports the presence of known bluetooth (Beacon) devices and reports it (via MQTT) to Home Assistant. This works fine with the HA Android App (IOS not tested yet). There are other projects like this outside which are running on a raspberry pi or aren't working anymore or lacking of features I would like to have so I decided to write my own and give a little bit back to the HA community.

### Features
- Web UI to set everything up
- MQTT Client
- BLE Beacon Scanner for 3 different devices (more to come in the future)
- works flawless with android Home Assistant App
- Easy Integration into Home Assistant
- Raw distance calculation


### Roadmap
- provide bin for making first flash easier via ESP flash tool
- Update via Web ui (binary upload)
- better distance calculation
- more network settings (fixed ip etc.)
- add bluetooth scanner settings
- add support for more bluetooth devices like smartwatches etc.
- add support for more devices being saved

## How to install (via Plattformio)
1. Download the program
2. open and extract the content of the zip file to any place on your pc
3. Open the project folder in Plattformio
4. Open Plattformio menue
5. Plug in your ESP32
6. (Optional) Open devices.json and settings.json located at /data and change your credentials - you can later change all settings over the web ui
7. Click Build Filesystem Image (Make sure there is no open serial connection to the ESP32 - you can close it with the little garbage can icon in the terminal)
8. Click Upload Filesystem Image
9. Click Upload and Monitor
10. Check Serial Connections for errors
11. The ESP32 should start its own AP - look for a Wifi named "ESP32-BLE-Scanner"
12. Connect to the Wifi (it should not have a password)
13. Go to settings and change your Wifi and MQTT settings
14. Wait for the ESP32 to restart and check for the IP adress with an scan tool or check the serial connection for the device ip
15. Connect to the Scanner and fill your Bluetooth details untder devices. (Tip: You can find and change your own BL adress in the HA App under Settings - Sensors - BLE Transmitter - Activate - Copy and paste the UUID into the device adress


