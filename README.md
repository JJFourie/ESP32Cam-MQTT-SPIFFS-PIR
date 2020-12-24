# ESP32Cam PIR MQTT SPIFFS Webserver

ESP32-Cam board using a PIR sensor to trigger photo upload to a web server. Motion triggers are broadcast using MQTT. Runs a webserver for live video.


## Sketch Features
- ESP32-Cam board 
- PIR sensor (AM312) triggers photo capture and photo upload to (PHP) web server. 
- Temperature sensor (DS18B20) readings published using MQTT.
- Motion triggers are published using MQTT. 
- Runs webserver to allow realtime video monitoring using e.g. MotionEye or Home Assistant.
- Camera settings can be changed using MQTT. Camera settings are stored in a settings file in SPIFFS to make changes persistent over restarts.
- Configuration maintained using MQTT. Configuration settings stored in a SPIFFS config file to make settings persistent over restarts. 

## Home Assistant Integration
The board is integrated with `Home Assistant` (HA), where the movement detection is displayed and the temperature readings are captured. From HA photo capture can be triggered via MQTT. 

### 1. MQTT Auto-Discovery Messages
Using the Mosquitto MQTT client, `mosquitto_pub` publish auto-discovery commands to create the ESP32-Cam device and its related sensors in Home Assistant are provided in the MQTT folder.

### 2. Button to trigger MQTT Message
Some of the functionality of the sketch is controlled or managed/changed through MQTT messages. 
As example of how it can be done, below is one solution how to create a momentary button in Home Assistant to trigger the board to take a photo.

### 3. WiFi Strength and other Board Status Detail
The sketch can upload (configurable) board status detail in a MQTT message. This information can the be displayed in Home Assistant as a **State** identity (see 1.).
Information include e.g.
1. RSSI
2. Uptime
3. Last Reset Reason
4. CPU Temperature

## Node Red Integration
The board is integrated with with `Node Red` (NR), where the movement detection is used to switch on a garden light and to send a message and upload a photo to a Telegram bot. 

### 1. Photo to Telegram Bot Flow

### 2. Garden Light Flow
