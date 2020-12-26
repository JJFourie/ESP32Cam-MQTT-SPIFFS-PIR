/**************************************************************************
 * 
 * Constants and Definitions 
 * 
 **************************************************************************/

// CAMERA_MODEL_AI_THINKER
#define PWDN_GPIO_NUM     32
#define RESET_GPIO_NUM    -1
#define XCLK_GPIO_NUM      0
#define SIOD_GPIO_NUM     26
#define SIOC_GPIO_NUM     27
#define Y9_GPIO_NUM       35
#define Y8_GPIO_NUM       34
#define Y7_GPIO_NUM       39
#define Y6_GPIO_NUM       36
#define Y5_GPIO_NUM       21
#define Y4_GPIO_NUM       19
#define Y3_GPIO_NUM       18
#define Y2_GPIO_NUM        5
#define VSYNC_GPIO_NUM    25
#define HREF_GPIO_NUM     23
#define PCLK_GPIO_NUM     22

// ESP32-Cam GPIO Pins
#define pinFlashLED        4
#define pinBoardLED       33                                // Onboard LED (small red)
#define pinPIR            13                                // GPIO pin used for PIR data connection
#define pinOneWire         2                                // GPIO pin for One Wire bus    (possible: 2, 14, 15, 13, 12, 16)

// MQTT Topics
#define MQTT_PUB_TEMP           "gate/temperature/state"    // PUBLISH: current temperate value                 (value)
#define MQTT_PUB_MOTION         "gate/motion/state"         // PUBLISH: motion detected / motion stopped        (on/off)
#define MQTT_PUB_CAMERA         "gate/camera/state"         // PUBLISH: camera related events                   (photo/video/settings)
#define MQTT_PUB_CONFIG         "gate/monitor/config"       // PUBLISH: general settings                        (JSON settings)
#define MQTT_PUB_STATE          "gate/monitor/state"        // PUBLISH: telemetry metrics                       (JSON parameters)
#define MQTT_PUB_WIFI           "gate/monitor/wifi"         // PUBLISH: current WiFi (%) value                  (value)
#define MQTT_SUB_CAMCOMMAND     "gate/camera/cmnd"          // SUBSCRIBE: actions related to camera             (photo/video/enable/disable/report)
#define MQTT_SUB_CAMSETTING     "gate/camera/setsetting"    // SUBSCRIBE: set new camera setting                (<setting>:<value>)
#define MQTT_SUB_MOTION         "gate/motion/cmnd"          // SUBSCRIBE: PIR sensor behaviour                  (enable/disable/delay-<value>)
#define MQTT_SUB_TEMP           "gate/temperature/cmnd"     // SUBSCRIBE: actions related to Temperature        (update/interval)
#define MQTT_SUB_MONITOR        "gate/monitor/cmnd"         // SUBSCRIBE: actions related to Monitor (ESP32)    (update/interval)

#define CONFIGFILE "/config.json"                           // SPIFFS file with general app settings
#define SETTINGSFILE "/settings.json"                       // SPIFFS file with some camera settings

#define PART_BOUNDARY "123456789000000000000987654321"
static const char* _STREAM_CONTENT_TYPE = "multipart/x-mixed-replace;boundary=" PART_BOUNDARY;
static const char* _STREAM_BOUNDARY = "\r\n--" PART_BOUNDARY "\r\n";
static const char* _STREAM_PART = "Content-Type: image/jpeg\r\nContent-Length: %u\r\n\r\n";

bool flashState = LOW;
