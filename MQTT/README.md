# MQTT Auto-Discovery Messages

Below are examples of MQTT topics and messages that can be (manually) submitted via a MQTT client, that will autamically create `Devices` and `Entities` in `Home Assistant` (HA) using the auto-discovery feature of HA.  
See [HA Discovery documentation](https://www.home-assistant.io/docs/mqtt/discovery/) and the [Mosquitto reference](https://mosquitto.org/man/mosquitto_pub-1.html). 

**Remarks**
- These examples are based on the Mosquitto client using the `mosquitto_pub` command..
- In the provided command examples, set the proper values for all fields marked with **<>**. Set the following:
  1) IP address of server hosting the MQTT Broker. (marked \<Broker IP\> below)
  2) MQTT Broker username and password, if required for your environment.
  3) MQTT topic scheme/structure to match your implementation.
  4) Device name, Sensor names, and unique ID's.
  5) Provide the Device Identifier(s), and also the (HA) Name, Model and Manufacturer. 
  6) For auto discovery to work, the topic must start with "homeassistant" and end with "config".
  Note: "homeassistant" is the default MQTT discovery topic, but can be overwritten in `configuration.yaml`.
- Based on my experience, I have to use the "retain" option (-r) to create the device in HA and make it survive a HA restart. Without the -r flag, the device would be created but was not visible after I restarted HA.
- The HA Device or Entity can be deleted using the auto-discovery topic with (only) the "-n" parameter.   
The device can also be deleted from within HA.

## 1. (MQTT) *Device* and *State Entity* for the Device State.
- **Create**:   
	```mosquitto_pub -h \<Broker IP\> -r -d -t "homeassistant/sensor/<unique_topic_id>/config" -m '{"name":"<HA Entity Name>","unique_id":"<unique_sensor_id>","state_topic":"<MQTT state change topic>","json_attributes_topic":"<MQTT attribute change topic>","unit_of_measurement":"%","value_template":"{{value_json.wifi}}","device":{"identifiers":["<Unique_Device_ID>"],"name":"<Device Name>","model":"<board model>","manufacturer":"<board manufacturer>"}}' ```   
  *For how the sketch was implemented, <state_topic> and <json_attributes_topic> should be the same.*
- **Delete**:   
  ```mosquitto_pub -h \<Broker IP\> -d -t "homeassistant/sensor/<unique_topic_id>/config" -n ```


## 2. *Temperature Entity* for the Temperature sensor, associated to device created in 1.
- **Create**:   
	```mosquitto_pub -h \<Broker IP\> -r -t "homeassistant/binary_sensor/<unique_topic_id>/config" -m '{"name":"<HA Entity Name>","unique_id":"<unique_sensor_id>","device_class": "MOTION", "payload_on":"on", "payload_off":"off", "off_delay":30, "state_topic":"<MQTT state change topic>", "qos":0, "device": {"identifiers": ["<Unique_Device_ID>"]}}' ```   
  *Note: the DeviceID must be the same as in (1.), in order to link this Entity to the same Device.*
- **Delete**:   
	```mosquitto_pub -h \<Broker IP\> -t "homeassistant/binary_sensor/<unique_topic_id>/config" -n ```
   
   
## 3. *Movement Entity* for the PIR sensor, associated to device created in 1.
- **Create**:   
  ```mosquitto_pub -h \<Broker IP\> -r -t "homeassistant/sensor/<unique_topic_id>/config" -m '{"name":"<HA Entity Name>","unique_id":"<unique_sensor_id>","device_class":"TEMPERATURE","unit_of_measurement":"°C","state_topic": "<MQTT state change topic>", "qos":0, "device": {"identifiers": ["<Unique_Device_ID>]}}' ```   
  *Note: the DeviceID must be the same as in (1.), in order to link this Entity to the same Device.*
- **Delete**:   
	```mosquitto_pub -h \<Broker IP\> -t "homeassistant/sensor/<unique_topic_id>/config" -n ```   
   
      
----      
   
   
# Actual Example, based on my implementation. 
  I called my device "GateMonitor". Replace with your device name, and change the unique_id's of the Device and Entities accordingly.
   
### Device and Status entity:
- **Create**:   
	```mosquitto_pub -h 192.168.1.1 -r -d -t "homeassistant/sensor/GateMonitorStatus01/config" -m '{"name":"GateMonitor Status","unique_id":"gatemonitorstatus01","state_topic":"gate/monitor/state","json_attributes_topic":"gate/monitor/state","unit_of_measurement":"%","value_template":"{{value_json.wifi}}","device":{"identifiers":["MONITORCAM01"],"name":"GateMonitor","model":"ESP32-Cam","manufacturer":"AI-Thinker"}}' ```
- **Delete**:   
 ```mosquitto_pub -h 192.168.1.1 -d -t "homeassistant/sensor/GateMonitorStatus01/config" -n ```
   
### PIR Sensor entity:
- **Create**:   
	`mosquitto_pub -h 192.168.1.1 -r -t "homeassistant/binary_sensor/GateMonitorPIR01/config" -m '{"name":"Gate Movement","unique_id":"gatemonitorPIR01","device_class": "MOTION", "payload_on":"on", "payload_off":"off", "off_delay":30, "state_topic":"gate/motion/state", "qos":0, "device": {"identifiers": ["MONITORCAM01"]}}' `
- **Delete**:   
	`mosquitto_pub -h 192.168.1.1 -t "homeassistant/binary_sensor/GateMonitorPIR01/config" -n `

### Temperature Sensor entity:
- **Create**:   
  ```mosquitto_pub -h 192.168.1.1 -r -t "homeassistant/sensor/outsidetemp01/config" -m '{"name":"Outside Temperature","unique_id":"outsidetemp01","device_class":"TEMPERATURE","unit_of_measurement":"°C","state_topic": "gate/temperature/state", "qos":0, "device": {"identifiers": ["MONITORCAM01"]}}' ```
- **Delete**:   
	```mosquitto_pub -h 192.168.1.1 -t "homeassistant/sensor/outsidetemp01/config" -n ```
