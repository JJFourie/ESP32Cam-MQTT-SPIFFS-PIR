# Home Assistant

This document contains notes on the related components and functionality I added to [Home Assistant](https://www.home-assistant.io/) (HA).    

Note that    
- in HA there are multiple ways to do things, and what is described below may not be the best way, or may be outdated by the time I finish writing as HA is evolving all the time.
- the examples below are from my HA implementation, containing my Entity and object names. You may want to adjust these to fit your own implementation. 
- I will just cover the basics, if you need more information on how to do things please visit the [HA Documentation](https://www.home-assistant.io/docs/) or [HA Forum](https://community.home-assistant.io/). 
- Under "*`Advanced`*" I provided my current setup, making use of a custom HA card that allows the button icons to be changed based on the entity state.

## 1. "Take Photo" Button
This is a button in the HA UI that can be used to trigger the ESP32-Cam to take a photo and upload it to the server. Allowing photos to be triggered from the server end makes it possible to programatically take photos from automations, e.g. to take a timelapse series. Or whatever.    
The tricky thing here is to make the HA button behave like a "momentary switch", meaning you press it and moments later it returns to the original "off" state.    
This can be done using [HA Automations](https://www.home-assistant.io/docs/automation/basics/).

1. Create an "Input Boolean" named "takegatephoto".    
In `Configuration -> Helpers` click "Add Helper", select "Toggle", give it a name, then "Create".
2. Edit `automations.yaml`, and add the `Take Gate Photo` automation.    
(See the `automations.yaml` code below, and select the appropriate automation.)
3. Add a Button to HA Lovelace, with the following settings:
```
entity: input_boolean.takegatephoto
icon: 'mdi:camera-wireless'
tap_action:
  action: toggle
```

## 2. "Enable/Disable Movement Camera" Button
You sometimes may want to prevent the PIR Movement sensor from automatically triggering the camera to take and upload a photo when movement is detected. This is typically the case when your mother-in-law is visting. You want to know when she arrives, but you surely don't want a photo of her in your system.    
Note that this will only prevent the PIR sensor from automatically triggering the camera. The camera itself is still functional, it can be triggered "manually" (using MQTT) to take a photo, or you can connect to it (with e.g. a browser or MotionEye) to watch a live video stream.   
1. Create an "Input Boolean" named "input_boolean.gate_enable_camera".
2. Edit `automations.yaml`, and add the `GateMonitor EnableDisable Camera` automation.
3. Add a Button to HA Lovelace, with the following settings:
```
entity: input_boolean.gate_enable_camera
icon: 'mdi:camera'
```

## 3. "Enable/Disable PIR" Button
Sometimes you may want to disable the PIR Movement Sensor from reporting movement, e.g. when you host a big party (post Corona lockdown) and expect a lot of people to visit.     
Or.. you may want HA to disable the sensor automatically when you are home and the house is occupied. This can be done using e.g. a Node Red flow or a HA Automation.
1. Create an "Input Boolean" named "input_boolean.gate_enable_pir".
2. Edit `automations.yaml`, and add the `GateMonitor EnableDisable PIR` automation.
3. Add a Button to HA Lovelace, with the following settings:
```
entity: input_boolean.gate_enable_pir
icon: 'mdi:motion-sensor'
```

## 4. Display ESP32-Cam Configuraton Settings
You may want to confirm the configuration that is currently active on the ESP32-Cam.    
The most simple way is to just add a MQTT sensor in HA for the ESP32-Cam configuration, and then view this sensor status in an Entity card. Unfortunately this is not very ellegant, as the configuration is uploaded in JSON format, and therefore a bit messy to see.    
Another solution is to display the sensor attributes in e.g. a HA Markdown Card.    
1. Create a MQTT sensor in `configuration.yaml` to capture the MQTT message containing the ESP32-Cam configuration.
```
sensor:
  - platform: mqtt
    name: 'GateMonitor Config'
    unique_id: gatemonitorconfig01
    state_topic: 'gate/monitor/config'
    json_attributes_topic: 'gate/monitor/config'
    device: 
      identifiers:
        - MONITORCAM01
```
2. In Lovelace, add a `Markdown Card`, and add the following under Content:
```
|  | Name | Value |
| --- |:---- | -----:|
{%- for attr in states.sensor.gatemonitor_config.attributes %}
{%- if not loop.last %} 
{{ '| {} | {} | *{}* |'.format(  loop.index  ,  attr ,  states.sensor.gatemonitor_config.attributes[attr] ) }}
{% endif -%}  |
{%- endfor %}
```
Note: This is the most simple configuration, just to show the basics, and does not result in a very nicely aligned table. You can do your own formating from here. Adding rows to a markdown table in a loop seems not very well supported (in HA) at the moment. See the way I did it under "*Advanced*" below for a more complex but better looking result.    

#### `automation.yaml`
Below is the content of the `automation.yaml` file, for the buttons mentioned above.    
You can either add it using the file editor (copy and paste), or else (try to) add it using the HA Automations Visual Editor.    

*Remember to change the Entity- and other names to match your own implementation!* 
```
# Trigger GateMonitor Camera to take photo via MQTT msg
- alias: Take Gate Photo
  trigger:
    platform: state
    entity_id: input_boolean.takegatephoto
    to: 'on'
  condition: []
  action:
  - service: mqtt.publish
    data:
      topic: gate/camera/cmnd
      payload: photo
  - delay: 0.9
  - service: input_boolean.turn_off
    entity_id: input_boolean.takegatephoto
#
# Toggle GateMonitor PIR Enable/Disable status via MQTT msg
- alias: GateMonitor EnableDisable PIR
  trigger: 
    platform: state
    entity_id: input_boolean.gate_enable_pir
  condition: []
  action:
    - service: mqtt.publish
      data_template:
        topic: gate/motion/cmnd
        payload: "{{ 'enable' if is_state('input_boolean.gate_enable_pir', 'on') else 'disable' }}"
#
# Toggle GateMonitor CAMERA Enable/Disable status via MQTT msg
- alias: GateMonitor EnableDisable Camera
  trigger: 
    platform: state
    entity_id: input_boolean.gate_enable_camera
  condition: []
  action:
    - service: mqtt.publish
      data_template:
        topic: gate/camera/cmnd
        payload: "{{ 'enable' if is_state('input_boolean.gate_enable_camera', 'on') else 'disable' }}"
```        

### 5. WiFi Strength and other Board Status Detail
The sketch can periodically publish board status detail using MQTT. This information can then be displayed in Home Assistant as a HA **State** identity.
The frequency of this state reporting is configurable using the `gate/monitor/cmnd` MQTT topic.
- Topic: `gate/monitor/cmnd `
- Payload: `interval:10`  (for an interval of 10 seconds)
- Payload: `ReportState:false` (disable or enable the periodic reporting of the status)    

Information include e.g.
1. RSSI
2. Uptime
3. Last Reset Reason
4. SoC Core Temperature

![HA Status Identity](https://github.com/JJFourie/ESP32Cam-MQTT-SPIFFS-PIR/blob/main/Images/HA_GateMonitor_Status.jpg)    
    
Image of the `Status` identity in Home Assistant.    

### 6. MQTT Auto-Discovery Messages
Using the Mosquitto MQTT client (`mosquitto_pub`), auto-discovery topics can be published to automatically create the ESP32-Cam device and its related sensors in HA.    
(These commands could have been added to the sketch, but as this is a one-time action I decided it is not worth the effort and just clutters the final solution with unnecessary functionality that may or may not be used.)    
See the [MQTT Readme](https://github.com/JJFourie/ESP32Cam-MQTT-SPIFFS-PIR/blob/main/MQTT/README.md) for more detail.



# ADVANCED
I wanted buttons that allow the icon to be changed based on the state of the switch. This can be done with the custom [Button Card](https://github.com/custom-cards/button-card). How to install the custom buttons card is detailed in its Github page.    

Note: *Use custom HA components at your own discresion and risk*.    

![HA_Lovelace_Cards](https://github.com/JJFourie/ESP32Cam-MQTT-SPIFFS-PIR/blob/main/Images/GateMonitor_Lovelace_Cards.jpg)    
    
Above: The buttons to manage the Camera, and the ESP32-Cam current configuration settings.    



Steps to use the Lovelace configuration used to create the Lovelace view (page).
1. Make sure you have a working implementation of the custom `button-card`
2. Add a new (blank) view to Lovelace
3. Add any card (e.g. Button) temporarily. This will be overwritten in step (5).
4. Click "`Show Code Editor'"
5. Replace whatever is in the Lovelace editor, with the text below. 
- It will add grids, and grids in grids, and buttons in those grids, and lastly a markdown card showing the configuration settings.
- Still a work in progress ...

```
type: grid
columns: 1
square: false
cards:
  - type: grid
    columns: 1
    square: false
    cards:
      - type: 'custom:button-card'
        entity: input_boolean.takegatephoto
        icon: 'mdi:camera-wireless'
        size: 10%
        tooltip: Take Photo and post to Telegram
        tap_action:
          action: toggle
  - type: grid
    columns: 2
    square: false
    cards:
      - type: 'custom:button-card'
        entity: input_boolean.gate_enable_camera
        name: Gate Cam
        show_state: true
        size: 15%
        show_last_changed: true
        tooltip: Enable Movement Sensor to take photo
        styles:
          name:
            - font-size: 14px
          label:
            - color: grey
            - font-size: 11px
          state:
            - font-size: 11px
        state:
          - value: 'on'
            icon: 'mdi:camera'
            styles:
              icon:
                - color: lightgreen
          - value: 'off'
            icon: 'mdi:camera-off'
            styles:
              icon:
                - color: red
      - type: 'custom:button-card'
        entity: input_boolean.gate_enable_pir
        name: Gate PIR
        show_state: true
        size: 15%
        show_last_changed: true
        tooltip: Enable Movement Sensor
        styles:
          name:
            - font-size: 14px
          label:
            - color: grey
            - font-size: 11px
          state:
            - font-size: 11px
        state:
          - value: 'on'
            icon: 'mdi:motion-sensor'
            styles:
              icon:
                - color: lightgreen
          - value: 'off'
            icon: 'mdi:motion-sensor-off'
            styles:
              icon:
                - color: red
  - type: markdown
    content: >

      |  &nbsp;   |  &nbsp;  &nbsp;  &nbsp; Name |  &nbsp;  &nbsp;  &nbsp;
      &nbsp;  &nbsp;  &nbsp;  &nbsp;  &nbsp;  &nbsp;Value  &nbsp;  &nbsp; 
      &nbsp;  |

      | --- |:---- | -----:|


      {%- for attr in states.sensor.gatemonitor_config.attributes %}

      {%- if not loop.last %} 

      {{ '| &nbsp;  &nbsp;  &nbsp;  &nbsp; {} &nbsp;  &nbsp; |{}| *{}*
      |'.format(  loop.index  ,  attr , 
      states.sensor.gatemonitor_config.attributes[attr] ) }}

      {% endif -%}  |

      {%- endfor %}

```





