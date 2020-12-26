# Runthrough of the Sketch
## Environment
C++ code created in Visual Studio Code using [PlatformIO](https://platformio.org/). 
(See [Installation](https://docs.platformio.org/en/latest/integration/ide/vscode.html#installation))

## Known Issues
1) **MQTT Mosquitto Client Disconnect**   
I experienced a ([known](https://github.com/knolleary/pubsubclient/issues/200#issuecomment-334198523)) problem where the device would stop listening to subscribed MQTT topics if it momentarily disconnected from the Mosquitto Broker due to e.g. a bad WiFi connection. After it reconnected it would still submit/publish topics, but not receive anything. Apparently the problem is related to the MQTT Broker creating a new "*clean*" session when the client connects again, but the client would still listen on the old session to published topics. It seems a workaround is to not connect the client each time with a clean session (default). A potential downside is that all messages publised during the client disconnect would still be received and processed by the client.   
- Normal way to create the connection to the Broker:    
`mqttClient.connect("<unique_id>", MQTT_user, MQTT_pwd)`
- My workaround to connect without creating a clean session (last "false" parameter below):    
`mqttClient.connect("<unique_id>", MQTT_user, MQTT_pwd, NULL, 0 ,false ,NULL, false)`

2) **ESP32-Cam Heatup**   
Some ESP32-Cam boards can get quite hot during video streaming, or when capturing/uploading large photos. This is probably subject to how fake your board is. The ESP32 SoC has a build-in temperature sensor, and in theory it can be used to shut down things and protect the board if it gets too hot. I did not go that far, but just visualizes the SoC temperature by including it in the regular *status* MQTT message.

## Key Functions
### Setup
- Set up the WiFi and MQTT connections.
- Read the App configuration from the SPIFFS config file. (defaults are set if the file does not yet exist)
- Read the Cam settings from the SPIFFS settings file. Only some key settings are saved, not all possible Cam settings.
- An `Interupt Service Request` (ISR) is created for the PIR sensor.
- The One-Wire Temperature sensor is initialized.
- The local web server is started, used for life video streaming from e.g. MotionEye, Home Assistant (or even just a browser).
- Some basic chip information is printed as debug output.

### Loop
- If the PIR detected movement, publish a MQTT message.
- If movement was detected, capture a photo and upload to the specified web server. Also done if a photo was manually requested through a received MQTT message.
- At regular intervals, read the temperature from the sensor and publish as MQTT message.
- At regular intervals,  publish the current App status detail as MQTT message.   

(All these actions can be enabled/disabled, and the intervals between reporting can be configured, using MQTT messages)

### App- and Camera settings in SPIFFS
There are routines to
- Read the settings from SPIFFS on startup. If the two files do not exist, then default values will be set and the files will be created automatically.
- Report the current configuration and settings in MQTT messages when requested.
- If the configuration or settings are changed (through MQTT), then the corresponding file will be updated accordingly.   
  a) **App Configuration**
    - Enable/Disable the PIR sensor *movement reporting*. 
    - Enable/Disable the Camera *photo upload*.
    - Set the *movement reporting delay*.
    - Set the *temperature reporting interval*. (interval of 0 = disabled)
    - Set the *status reporting interval*. (interval of 0 = disabled)   
    
  b) **Camera Settings**   
  Only the following settings are currently stored in the SPIFFS file. Add as needed. Other camera settings can be changed (MQTT), but will revert to default values after a restart.
    - Set the *frame size*. (e.g. VGA, SVGA)
    - Set the *quality*. (number of pixels)
    - Set the *contrast*.
    - Set the *brightness*.
    - Image *vertical flip*. (helpful to prevent upside down image when the camera is mounted)
    - Image *horizontal mirror*. (helpful to correct the image when the camera is mounted)

## Libraries
- Arduino.h
- WiFi.h
- esp_camera.h
- PubSubClient.h
- OneWire.h
- DallasTemperature.h
- soc/rtc_cntl_reg.h
- SPIFFS.h
- ArduinoJson.h
- rom/rtc.h
- esp_http_server.h
- esp_http_client.h
- fb_gfx.h
