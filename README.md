# ESP32Cam PIR MQTT SPIFFS Webserver

A ESP32-Cam based project that uses a PIR sensor to trigger photo upload to a web server. The PIR motion triggers are also broadcast using MQTT. The sketch runs a local webserver making live video streaming possible. It also publishes the ambient temperature readings from a connected temperature sensor. Configuration and camera settings are maintained through MQTT, and are stored in local SPIFFS files so they can survive a restart.

This started as a project to detect movement at a garden gate, that would then sound a notification in the house and switch on a garden light when dark. Once I decided to use a ESP32-Cam, it evolved to include photo capture and video streaming. I then slapped on a temperature sensor to upload the outside temperature to Home Assistant. Node Red is used to upload the captured photo to a Telegram bot. 

## Project Features
- C++ sketch for a **ESP32-Cam** board. 
- PIR motion sensor (**AM312**) events published using MQTT. 
- Motion detection triggers photo capture and **photo upload to a (PHP) web server**. 
- Temperature sensor (**DS18B20**) readings are published using MQTT.
- Runs a local webserver to allow realtime **video streaming** using e.g. MotionEye or Home Assistant (or just a browser).
- Camera settings can be maintained using MQTT. Camera settings are used to initialize the camera after startup.
- App configuration is maintained using MQTT. Configuration settings are used to initialize the device after startup. 
- Camera and App settings are stored in **JSON 6** format files in **SPIFFs**, to make the settings persistant and survive restarts.
- The board status, including WiFi strength, SoC Core temperature, time since last restart, etc. is published using MQTT.

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
  **Note** Ensure your FTDI card pins match the layout in the diagram! My (fake?) card pin layout was the opposite, an exact mirror image of the most common cards.
- After development, when the board is deployed, connect the *red* and *black* wires providing power from the FTDI board during programing, to the positive (+) and ground (-) of a suitable 5V external power source.

## Camera Settings

Function |	Description |	Values | Default
-- | -- | -- | --
set_quality() |	Quality | 10 - 36 | 10 (high)    
set_brightness() |	Brightness |	-2 to 2 | 0
set_contrast() |	Contrast |	-2 to 2 | 0
set_saturation() |	Saturation |	-2 to 2 | 0
set_special_effect() |	Special Effect |	0 – No Effect <br> 1 – Negative <br>  2 – Grayscale <br>  3 – Red Tint <br>  4 – Green Tint <br>  5 – Blue Tint <br>  6 – Sepia | 0
set_whitebal() |	Enable White Balance |	0 – disable <br> 1 – enable | 1
set_awb_gain() |	Enable White Balance Gain |	0 – disable <br> 1 – enable | 1
set_wb_mode() |	White Balance Mode | 0 – Auto <br> 1 – Sunny <br> 2 – Cloudy <br> 3 – Office <br> 4 – Home | 0 (Auto)
set_exposure_ctrl() |	Enable Exposure Control | 0 – disable <br> 1 – enable | 1
set_aec2()	| AEC (night mode?) |	0 – disable <br> 1 – enable | 0
set_ae_level()	| Automatic Exposure Level |	-2 to 2 | 0
set_aec_value()	| Automatic Exposure Correction |	0 to 1200 | 204
set_gain_ctrl()	| Enable Gain Control |	0 – disable <br> 1 – enable | 1
set_agc_gain()	| Gain Level |	0 to 30 | 5
set_gainceiling()	| Gain Ceiling (2x - 128x) |	0 to 6 | 0
set_bpc()	| Black Pixel Correction |	0 – disable <br> 1 – enable
set_wpc()	| White Pixel Correction |	0 – disable <br> 1 – enable | 1
set_raw_gma() | Enable Raw GMA |	0 – disable <br> 1 – enable | 1
set_lenc() |	Lens Correction |	0 – disable <br> 1 – enable | 1
set_hmirror() |	Horizontal Mirror |	0 – disable <br> 1 – enable | 0
set_vflip()	| Vertical Flip |	0 – disable <br> 1 – enable | 0
set_dcw()	| DCW (Downsize EN) |	0 – disable <br> 1 – enable | 1
set_colorbar() |	Show Colorbar | 0 – disable <br> 1 – enable | -
face_detect() |	Face Detection | 0 – disable <br> 1 – enable | -
face_recognize() | Face Recognition | 0 – disable <br> 1 – enable | -
set_framesize() |	Resolution (0-10) | 10	UXGA(1600x1200) <br> 9	SXGA(1280x1024) <br> 8	XGA(1024x768) <br> 7	SVGA(800x600) <br> 6	VGA(640x480) <br> 5	CIF(400x296) <br> 4	QVGA(320x240) <br> 3	HQVGA(240x176) <br> 0	QQVGA(160x120) | 6 VGA(640x480)    

**Reference**:   
- [OV2640](https://www.uctronics.com/download/cam_module/OV2640DS.pdf)
- [RandomNerdTutorials](https://randomnerdtutorials.com/esp32-cam-ov2640-camera-settings/)

## MQTT Detail
The ESP32-Cam interacts with the outside world using MQTT.
The following is supported:
- Publish 
    1. Movement Detection (can be enabled/disabled)
    2. Status Updates (periodic or when manually requested)
    3. Configuration Settings (on configuration change, or when manually requested)
    4. Camera Settings (only some key settings. when manually requested)
- Subscribed 
    1. Trigger to take photo
    2. Update Camera settings (see table above)
    3. Update ESP32-Cam Configuration settings
    4. Request Status, Configuration
    5. Restart ESP32-Cam
See the [MQTT Readme](https://github.com/JJFourie/ESP32Cam-MQTT-SPIFFS-PIR/blob/main/MQTT/README.md) for detail on the MQTT setup and topics.

## Home Assistant Integration
In my setup the ESP32-Cam is integrated with `Home Assistant` (HA), where the movement detection is displayed and the temperature readings are captured. Capturing a photo can also be triggered from HA via MQTT. 
See the [Home Assistant Readme](https://github.com/JJFourie/ESP32Cam-MQTT-SPIFFS-PIR/blob/main/HomeAssistant/README.md) for more detail.

## Node Red Integration
Some of the board automations are handled in `Node Red`.   
See the [Node Red Readme](https://github.com/JJFourie/ESP32Cam-MQTT-SPIFFS-PIR/blob/main/NodeRed/README.md).

## PHP Server
In my implementation the ESP32-Cam uploads captured photo's to a PHP server, from where `Node Red` will publish each newly captured photo to a Telegram bot.
See the [PHP Readme](https://github.com/JJFourie/ESP32Cam-MQTT-SPIFFS-PIR/blob/main/PHP/README.md)

