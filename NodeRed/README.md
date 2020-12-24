# Node Red Integration with Device

## 1. Notification and Photo to Telegram Bot Flow
A "photo uploaded" MQTT message received from the ESP32-Cam triggers Node Red to send an alert notification to a Telegram bot, and upload the photo to Telegram.   
This requires a Telegram account and a Telegram bot being set up.

### Remarks
- Requires the `node-red-contrib-telegrambot` Node Red pallette to be installed.
- Replace \<YOUR_CHAT_ID\> (in each of the two `Function` nodes) with your own Telegram Chat ID, that you received when creating the Telegram bot.
- Replace \<HA_TELEGRAM_BOT\> (`Telegram sender` node) with your own Node Red Telegram Bot name.  

```
[{"id":"c28d1213.3a847","type":"mqtt in","z":"34a09e59.64c992","name":"Gate Movement","topic":"gate/motion/state","qos":"0","datatype":"auto","broker":"e50612ee.93d158","x":160,"y":1080,"wires":[["b6291d0.4869e6"]]},{"id":"b6291d0.4869e6","type":"function","z":"34a09e59.64c992","name":"send text alert","func":"var runtime = new Date().toLocaleString();\nvar message = 'Alert \\n\\nMovement at ' + runtime + \"\\n\";\nmsg.payload = {chatId : <YOUR_CHAT_ID>, type : 'message', content : message};\n\nreturn msg;","outputs":1,"noerr":0,"initialize":"","finalize":"","x":400,"y":1080,"wires":[["a7335a72.663118"]]},{"id":"a7335a72.663118","type":"telegram sender","z":"34a09e59.64c992","name":"","bot":"3fcf81b.6a6e5fe","x":710,"y":1080,"wires":[["7ba4626e.ee3864"]]},{"id":"687d36e3.d5432","type":"function","z":"34a09e59.64c992","name":"create image message","func":"var runtime = new Date().toLocaleString();\n//process.env[\"NTBA_FIX_350\"] = 1;\n\n//    content: \"/data/photos/gatecam_ok.jpg\" };\n\nvar payload = { \n    chatId: <YOUR_CHAT_ID>,\n    caption: \"\\nPhoto @ \" + runtime + \"\\n\",\n    type: \"photo\",\n    content: \"/share/photos/gatecam.jpg\" };\n\nreturn {payload};","outputs":1,"noerr":0,"initialize":"","finalize":"","x":420,"y":1040,"wires":[["a7335a72.663118"]]},{"id":"7ba4626e.ee3864","type":"debug","z":"34a09e59.64c992","name":"Telegram Msg Send","active":true,"tosidebar":true,"console":false,"tostatus":false,"complete":"payload","targetType":"msg","statusVal":"","statusType":"auto","x":960,"y":1060,"wires":[]},{"id":"80fd4f5b.1c397","type":"inject","z":"34a09e59.64c992","name":"","props":[{"p":"payload"},{"p":"topic","vt":"str"}],"repeat":"","crontab":"","once":false,"onceDelay":0.1,"topic":"","payload":"","payloadType":"date","x":160,"y":1000,"wires":[["687d36e3.d5432"]]},{"id":"de43add8.fd4d48","type":"mqtt in","z":"34a09e59.64c992","name":"Photo Uploaded","topic":"gate/camera/state","qos":"0","datatype":"auto","broker":"e50612ee.93d158","x":160,"y":1040,"wires":[["687d36e3.d5432"]]},{"id":"e50612ee.93d158","type":"mqtt-broker","name":"MQTT_Broker","broker":"mosquitto","port":"1883","clientid":"","usetls":false,"compatmode":false,"keepalive":"60","cleansession":true,"birthTopic":"","birthQos":"0","birthPayload":"","closeTopic":"","closeQos":"0","closePayload":"","willTopic":"","willQos":"0","willPayload":""},{"id":"3fcf81b.6a6e5fe","type":"telegram bot","botname":"<HA_TELEGRAM_BOT>","usernames":"","chatids":"","baseapiurl":"","updatemode":"polling","pollinterval":"60000","usesocks":false,"sockshost":"","socksport":"6667","socksusername":"anonymous","sockspassword":"","bothost":"","localbotport":"8443","publicbotport":"8443","privatekey":"","certificate":"","useselfsignedcertificate":false,"sslterminated":false,"verboselogging":true}]
```

## 2. Garden Light Flow
The "movement detected" MQTT message is used to sound a notification buzzer inside the house, and to switch on a garden light when dark.

```

```
