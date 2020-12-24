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
Remarks:
- Confirm the ESP32-Cam board output voltage is 3.3V. Apparently some boards are set to 5V. This can be changed with a very small zero ohm resistor/jumper close to the output volate pin (may differ per board manufacturer).
- I powered both the AM312 and DS18B20 from the ESP32-Cam output voltage pin (the DS18B20 can also run on 5V).
- I had issues getting the DS18B20 to work. The temperature reading was -127°C, which is an error value indicating something went wrong. It worked for me using 3.3V instead of 5V, and putting a 4K7 pull-up resistor across the data pin and 3.3V.

### Fritzing Diagram

![Fritzing Diagram](https://github.com/JJFourie/ESP32Cam-MQTT-SPIFFS-PIR/blob/main/Images/Fritzing_ESP32Cam_DS18B20_AM312_FTDI.png)
- The white wires are only necessary when flashing the board with a new sketch, or when debugging in order to see the debug statements in the IDE.   
- After development, when the board is deployed, connect the *red* and *black* wires from the FTDI board to the positive (+) and ground (-) of a suitable 5V external power source.

## Home Assistant Integration
The board is integrated with `Home Assistant` (HA), where the movement detection is displayed and the temperature readings are captured. Capturing a photo can also be triggered from HA via MQTT. 

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
