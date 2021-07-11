# ESP-32-BLE-Scanner
for Home Assistant. [See thread at Home Assistant Board.](https://community.home-assistant.io/t/esp-32-ble-scanner-a-room-presence-detection-solution/315205)

### What does this program do?
This is basicly a room presence detector. Use it with Home Assistant to trigger automations when you enter a room, disarm your alarm etc. It reports the presence of known bluetooth (Beacon) devices and reports it (via MQTT) to Home Assistant. I designed the program to work together with the HA Android App (IOS not tested yet). 

### Please keep in mind
This is my first project on github, I am programming in my free time during my 9 to 5 job and I am not a professional programmer. I will try to support and fix errors as good and as fast as I can.




<img src="https://user-images.githubusercontent.com/50184150/121602995-6839b300-ca48-11eb-886f-b7b27d54ad3e.PNG" width="60%" height="60%">
<img src="https://user-images.githubusercontent.com/50184150/121775554-bf46a180-cb88-11eb-899e-792d83775994.PNG" width="50%" height="50%">



### Features
- Web UI to set everything up
- MQTT Client to report state to Home Assistant
- BLE Beacon Scanner for 3 different devices (more to come in the future)
- Works fine with Android Home Assistant App
- Easy Integration into Home Assistant
- Raw distance calculation


### Roadmap
- [X] provide bin for making first flash easier via ESP flash tool
- [X] improve MQTT and MQTT response
- [X] online status for the ESP32
- [ ] make update via Web ui possible(binary upload)
- [ ] better distance calculation
- [ ] more network settings (fixed ip etc.)
- [ ] add bluetooth scanner settings
- [ ] add support for more bluetooth devices like smartwatches etc.
- [ ] add support for more devices being saved


Settings:



## How to install (via ESP Download Tool)
<details>
<summary>
Click to expand
</summary>


1. Download and extract the program
2. [Download ESP32 Download Tool / Flash Download Tools](https://www.espressif.com/en/support/download/other-tools)
3. Start the Download Tool (ESP32 and Developer Mode)
4. Plug in your ESP32 and change Com Port according to it
5. Set the following settings:
```
Address for bin: 0
SPI Speed: 40mhz
SPI Mode: DIO
Flash Size: 32Mbit
DoNotChgBin: Set to true
```
6. Click start and the bin will be flashed

</details>



## How to install (via Plattformio)
<details>
<summary>
Click to expand
</summary>

0. [For Plattformio installation (on Visual Studio Code or Atom.io) see here](https://platformio.org/install)
1. Download and extract the program
3. Open the project folder in Plattformio
4. Open Plattformio menue
5. Plug in your ESP32
6. **(Optional)** Open devices.json and settings.json located at /data and change your credentials **(you can later change all settings over the web ui)**
7. Click Build Filesystem Image (Make sure there is no open serial connection to the ESP32 - you can close it with the little garbage can icon in the terminal)
8. Click Upload Filesystem Image
9. Click Upload and Monitor
10. Check Serial Connections for errors
</details>

## Set up the ESP32
<details>
<summary>
Click to expand
</summary>

1. The ESP32 should start its own AP - look for a Wifi named "ESP32-BLE-Scanner" / If you changed settings.json you can jump to 5.
2. Connect to the Wifi (it should not have a password)
3. Go to http://192.168.4.1 Setup and change your Wifi and MQTT settings
<img src="https://user-images.githubusercontent.com/50184150/121598013-9ff12c80-ca41-11eb-9cf0-02f066f84f3c.PNG" width="20%" height="20%">

5. Wait for the ESP32 to restart and check for the IP adress with an scan tool or check the serial connection for the device ip
6. Connect to the Scanner and fill your Bluetooth details under devices. (See Setup HA APP for how to get your UUID)
  
<img src="https://user-images.githubusercontent.com/50184150/121597100-93200900-ca40-11eb-92a1-570dcc807636.PNG" width="20%" height="20%">
</details>

## Setup HA App (Android) (Iphone is untested yet)

<details>
<summary>
Click to expand
</summary>

Open HA App go to App Configuration -> Manage Sensors -> Bluetooth Sensors -> BLE Transmitter

<img src="https://user-images.githubusercontent.com/50184150/121693805-c0fd6000-cac9-11eb-8fdf-cca56ad042ca.jpg" width="20%" height="20%">

Enable Sensor (should be disabled from the start)

<img src="https://user-images.githubusercontent.com/50184150/121693104-18e79700-cac9-11eb-8867-d69edade137e.jpg" width="20%" height="20%">

 -> Copy and paste the UUID into the device adress in the ESP32 BLE Scanner under devices
 
<img src="https://user-images.githubusercontent.com/50184150/121693158-26048600-cac9-11eb-8b2a-ba6e72766d86.jpg" width="20%" height="20%">!
</details>

## Setup Home Assistant

<details>
<summary>
Click to expand
</summary>

Add to your config.yaml:
```
sensor:
- platform: mqtt_room
  device_id: 'Your BLE UUID'
  name: 'Name of your mobile device or your name not your room/hostname'
  state_topic: 'ESP32 BLE Scanner/Scan'
  timeout: 60
  away_timeout: 30
```
 
[For more see HA Documentation](https://www.home-assistant.io/integrations/mqtt_room/)
</details>

