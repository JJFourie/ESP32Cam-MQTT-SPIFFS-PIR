# ESP32Cam PIR MQTT SPIFFS Webserver

A ESP32-Cam based project that uses a PIR sensor to trigger photo upload to a web server. The PIR motion triggers are also broadcast using MQTT. The sketch runs a local webserver making live video streaming possible. It also publishes the ambient temperature readings from a connected temperature sensor. Configuration and camera settings are maintained through MQTT, and are stored in local SPIFFS files so they can survive a restart.

This started as a project to detect movement at a garden gate, that would then sound a notification in the house and switch on a garden light when dark. Once I decided to use a ESP32-Cam, it evolved to include photo capture and video streaming. I use Node Red to upload the captured photo to a Telegram bot. I then slapped on a temperature sensor to upload the outside temperature to Home Assistant. 

## Project Features
- C++ sketch for a ESP32-Cam board. 
- PIR sensor (AM312) triggers photo capture and photo upload to (PHP) web server. 
- Temperature sensor (DS18B20) readings are published using MQTT.
- Motion triggers are published using MQTT. 
- Runs a local webserver to allow realtime video streaming using e.g. MotionEye or Home Assistant (or just a browser).
- Camera settings can be maintained using MQTT. Camera settings are stored in a SPIFFS file, and are used to initialize the camera after startup.
- App configuration is maintained using MQTT. Configuration settings are stored in a SPIFFS file in JSON 6 format, and are loaded on startup. 
- The board status, including WiFi strength, SoC core temperature, time since last restart, etc. is published using MQTT.

## Wiring
**Remarks**:
- Confirm the ESP32-Cam board output voltage is 3.3V. Apparently the output pin voltage of some boards are set to 5V. This can be changed with a very small zero ohm resistor/jumper close to the output volate pin (may differ per board manufacturer).
- I powered both the AM312 and DS18B20 from the ESP32-Cam output voltage pin (the DS18B20 can also run on 5V).
- I had issues getting the DS18B20 to work. The temperature reading was -127°C, which is an error value indicating something went wrong. It worked for me using 3.3V instead of 5V, and putting a 4K7 pull-up resistor across the data pin and 3.3V.

### Pin Connections
ESP32-Cam Pin | Description
-- | --
GPIO 2 | DS18B20 Data Pin
GPIO 13 | AM312 Data Pin
3.3V Output Pin | DS18B20 (+) Pin <br> AM312 (+) Pin
Gnd | DS18B20 (-) Pin <br> AM312 (-) Pin <br> Power Source (-)
5V | Power Source (+)

### Fritzing Diagram

![Fritzing Diagram](https://github.com/JJFourie/ESP32Cam-MQTT-SPIFFS-PIR/blob/main/Images/Fritzing_ESP32Cam_DS18B20_AM312_FTDI.png)
- The white wires are only necessary when flashing the board with a new sketch, or when debugging in order to see the output of debug statements in the IDE.   
- After development, when the board is deployed, connect the *red* and *black* wires providing power from the FTDI board during programing, to the positive (+) and ground (-) of a suitable 5V external power source.

## Camera Settings

Function |	Meaning |	Values | Default
-- | -- | -- | --
set_brightness() |	Set brightness |	-2 to 2 | 0
set_contrast() |	Set contrast |	-2 to 2 | 0
set_saturation() |	Set saturation |	-2 to 2 | 0
set_special_effect() |	Set a special effect |	0 – No Effect <br> 1 – Negative <br>  2 – Grayscale <br>  3 – Red Tint <br>  4 – Green Tint <br>  5 – Blue Tint <br>  6 – Sepia | 0
set_whitebal() |	Set white balance |	0 – disable <br> 1 – enable
set_awb_gain() |	Set white balance gain |	0 – disable <br> 1 – enable
set_wb_mode() |	Set white balance | mode	0 – Auto <br> 1 – Sunny <br> 2 – Cloudy <br> 3 – Office <br> 4 – Home
set_exposure_ctrl() |	Set exposure control | 0 – disable <br> 1 – enable
set_aec2()	| night mode enable (?) |	0 – disable <br> 1 – enable | 0
set_ae_level()	| Automatic exposure level |	-2 to 2
set_aec_value()	| Automatic exposure correction |	0 to 1200
set_gain_ctrl()	| Gain control |	0 – disable <br> 1 – enable
set_agc_gain()	| Automatic gain control |	0 to 30
set_gainceiling()	| |	0 to 6
set_bpc()	| Black pixel correction |	0 – disable <br> 1 – enable
set_wpc()	| White pixel correction |	0 – disable <br> 1 – enable
set_raw_gma() | |	0 – disable <br> 1 – enable
set_lenc() |	Set lens correction |	0 – disable <br> 1 – enable | 0
set_hmirror() |	Horizontal mirror |	0 – disable <br> 1 – enable | 0
set_vflip()	| Vertical flip |	0 – disable <br> 1 – enable | 0
set_dcw()	| |	0 – disable <br> 1 – enable
set_colorbar() |	Set a colorbar | 0 – disable <br> 1 – enable | 0    
set_framesize() |	Set framesize (0-10) | FRAMESIZE_QVGA (320 x 240) <br> FRAMESIZE_CIF (352 x 288) <br> FRAMESIZE_VGA (640 x 480) <br> FRAMESIZE_SVGA (800 x 600) <br> FRAMESIZE_XGA (1024 x 768) <br> FRAMESIZE_SXGA (1280 x 1024) <br> FRAMESIZE_UXGA (1600 x 1200) | 6 (VGA(640x480))    
set_quality() |	Set quality | 10 - 36 | 10 (high)    

**Reference**:   
- [OV2640](https://www.uctronics.com/download/cam_module/OV2640DS.pdf)
- [RandomNerdTutorials](https://randomnerdtutorials.com/esp32-cam-ov2640-camera-settings/)


## Home Assistant Integration
In my setup the ESP32-Cam is integrated with `Home Assistant` (HA), where the movement detection is displayed and the temperature readings are captured. Capturing a photo can also be triggered from HA via MQTT. 

### 1. MQTT Auto-Discovery Messages
Using the Mosquitto MQTT client (`mosquitto_pub`), auto-discovery topics can be published to automatically create the ESP32-Cam device and its related sensors in Home Assistant. See the MQTT folder.    
(These commands could have been added to the sketch, but as this is a one-time action I decided it is not worth the effort and just clutters the final solution with unnecessary functionality that may or may not be used.)

### 2. Button to trigger MQTT Message
Some of the functionality of the sketch can be controlled or managed/changed through MQTT messages. 
As an example of how to set this up, below is one solution how to create a momentary button in Home Assistant to send a MQTT message to the board to take a photo.

### 3. WiFi Strength and other Board Status Detail
The sketch can (configurable) periodically publish board status detail using MQTT. This information can then be displayed in Home Assistant as a HA **State** identity (see 1.).
Information include e.g.
1. RSSI
2. Uptime
3. Last Reset Reason
4. SoC Core Temperature

![HA Status Identity](https://github.com/JJFourie/ESP32Cam-MQTT-SPIFFS-PIR/blob/main/Images/HA_GateMonitor_Status.jpg)    
Image of the `Status` identity in Home Assistant.

## Node Red Integration
Some of the board automations are handled in `Node Red`.   
See the [Node Red Readme](https://github.com/JJFourie/ESP32Cam-MQTT-SPIFFS-PIR/blob/main/NodeRed/README.md).

## PHP Server
In my implementation the ESP32-Cam uploads captured photo's to a PHP server, from where each newly captured photo is uploaded to a Telegram bot.
See the [PHP Readme](https://github.com/JJFourie/ESP32Cam-MQTT-SPIFFS-PIR/blob/main/PHP/README.md)

