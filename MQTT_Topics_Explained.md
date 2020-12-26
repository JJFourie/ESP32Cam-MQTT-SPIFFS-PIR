# MQTT Topics
Below is an explanation of the MQTT topics and related parameters covered by the functionality.   
    
**Note**: my MQTT topic strategy is as shown below. Change the topics to match your own implementation.   
- "location" / "device" or "function" / "action"   
   
   
## Subscribed

1. ***Camera* Commands**:    
**Topic**: `gate/camera/cmnd`     
````
         "photo"                   : Capture and upload a photo
         "enable"                  : Enable camera and allow PIR to trigger taking/uploading photos (MQTT request still enabled)
         "disable"                 : Disable camera actions (PIR still enabled)
````

2. ***Camera* Settings**:    
**Topic**: `gate/camera/setsetting`     
````
         "<setting>:<value>"       : Update the camera settings with the provided value
````

3. ***PIR Movement* Commands**:    
**Topic**: `gate/motion/cmnd`    
````
         "enable"                  : Enable PIR motion feedback
         "disable"                 : Disable PIR motion feedback
         "delay:<seconds>"         : Set new delay/debounce between PIR triggers
````

4. ***Temperature* Commands**:    
**Topic**: `gate/temperature/cmnd`    
````
         "reading"                 : Request to report back the current temperature value
         "interval:<seconds>"      : Set the interval between temperature updates  (0=disabled)
````

5. ***App (GateMonitor)* Commands**:    
**Topic**: `gate/monitor/cmnd`    
````
         "restart"                 : Triggers software restart of ESP32. 
         "getstate"                : Request to report back the current state and telemetry values (RSSI, Uptime, Memory, ..)
         "getconfig"               : Request to report back the current configuration
         "interval:<seconds>"      : Set the interval between state updates   (0=disabled)
         "ReportState:<value>"     : Enable/disable reporting full device state    (true/false)
         "Reportwifi:<value>"      : Enable/disable reporting wifi strength        (true/false)
````    

----

## Published

1. ***App (GateMonitor)* State** - movement detected at gate (translates to "set" / "cleared")    
**Topic**: `gate/motion/state`    
**Payload**: `"yes" / "no"`    

2. ***Temperature* State** - current temperature value    
**Topic**: `gate/temperature/state`    
**Payload**: `<value>`    

3. ***App (GateMonitor)* Configuration** - list of current configuration settings (name : value)    
**Topic**: `gate/monitor/config`    
**Payload**: `<settings>`    

4. ***App (GateMonitor)* State** - list of parametry values reflecting App current state (name : value)    
**Topic**: `gate/monitor/state`    
**Payload**: `<values>`    

5. ***App (GateMonitor)* WiFi** - current WiFi RSSI value (lightweight alternative to full state)    
**Topic**: `gate/monitor/wifi`    
**Payload**: `<value>`    

6. ***Camera* State** - events related to the camera    
**Topic**: `gate/camera/state`    
**Payload**: `"photo"`    - photo was uploaded
