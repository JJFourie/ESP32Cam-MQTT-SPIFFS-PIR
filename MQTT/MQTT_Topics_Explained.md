# MQTT Topics
Below is an explanation of the MQTT topics and related parameters covered by the functionality.   
    
**Note**: my MQTT topic strategy is as shown below. Change the topics to match your own implementation.   
- "location" / "device" or "function" / "action"   
   
   
## Subscribed
This is a list of topics that the ESP32-Cam subscribes to, and the related functionality affected by the topic/payload:    
    
1. ***Camera* Commands**:    
**Topic**: `gate/camera/cmnd`     
````
         "photo"                   : Capture and upload a photo. MQTT alternative for PIR movement trigger.
         "enable"                  : Enable PIR movement detection to automatically trigger taking/uploading photos. 
         "disable"                 : Disable PIR to trigger camera actions. PIR still enabled. Camera still active, MQTT trigger and video stream still supported.
````

2. ***Camera* Settings**:    
**Topic**: `gate/camera/setsetting`     
````
         "<setting>:<value>"       : Update the specified camera setting with the provided value
````
See the [camera settings table](https://github.com/JJFourie/ESP32Cam-MQTT-SPIFFS-PIR#camera-settings) in the [Overview Readme](https://github.com/JJFourie/ESP32Cam-MQTT-SPIFFS-PIR/blob/main/README.md) for possible settings and values.    
Use the exact funcion name, but without "set_"    
e.g.
````
    Topic:       gate/camera/setsetting
    Payload:     brightness:-1
or
    Topic:       gate/camera/setsetting
    Payload:     vflip:1
````

3. ***PIR Movement* Commands**:    
**Topic**: `gate/motion/cmnd`    
````
         "enable"                  : Enable PIR motion feedback.
         "disable"                 : Do nothing when movement is detected: no MQTT is send, and Camera is not triggered to take photo.
         "delay:<seconds>"         : Set new delay/debounce between PIR triggers. Affects both motion feedback and camera trigger.
````

4. ***Temperature* Commands**:    
**Topic**: `gate/temperature/cmnd`    
````
         "reading"                 : Request to report back the current temperature value. If auto reporting is disabled, "manual" readings can still be requested.
         "interval:<seconds>"      : Set the interval between temperature updates  (0 = disabled).
````

5. ***App (GateMonitor)* Commands**:    
**Topic**: `gate/monitor/cmnd`    
````
         "restart"                 : Triggers software restart of ESP32. 
         "getstate"                : Request to report back the current state and telemetry values (RSSI, Uptime, Memory, ..)
         "getconfig"               : Request to report back the current ESP32-Cam configuration.
         "interval:<seconds>"      : Set the interval between state reports   (0 = disabled)
         "ReportState:<value>"     : Enable/disable reporting (complete) device state    (true/false)
         "Reportwifi:<value>"      : Enable/disable reporting (only) wifi strength       (true/false)
````    
    
----
    
## Published
This is a list of topics that the ESP32-Cam can publish:    
    
1. ***App (GateMonitor)* State**    
Movement detected.  (translates to "set" in Home Assistant)    
- **Topic**: `gate/motion/state`    
- **Payload**: `"on"`    

2. ***Temperature* State**    
Current temperature reading.
- **Topic**: `gate/temperature/state`    
- **Payload**: `<value>`    

3. ***App (GateMonitor)* Configuration**    
List of current configuration settings, in JSON format (name : value)    
e.g. Camera-Enabled, PIR-Enabled, Temperature-Reading-Interval, etc.     
- **Topic**: `gate/monitor/config`    
- **Payload**: `<settings>`    

4. ***App (GateMonitor)* State**    
List of parametry values reflecting App current state, in JSON format (name : value)    
e.g. RSSI, WiFi%, Core Temperature, Uptime, Last-Start-Reason, Memory, ..    
- **Topic**: `gate/monitor/state`    
- **Payload**: `<values>`    

5. ***App (GateMonitor)* WiFi**    
Current WiFi RSSI value (lightweight alternative to full state)    
- **Topic**: `gate/monitor/wifi`    
- **Payload**: `<value>`    

6. ***Camera* State**    
Events related to the camera    
- **Topic**: `gate/camera/state`    
- **Payload**: `"photo"`    - photo was uploaded
