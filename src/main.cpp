/**************************************************************************
 * 
 * ----  GateMonitor
 * . PIR sensor (AM312) monitors movement, and sends MQTT notification when motion detected
 * . Camera takes photo and uploads to server when triggered (either PIR or via MQTT)
 * - Temperature sensor (DS18B20) reports (outside) temperature via MQTT
 * - Runs web server to allow client to stream video
 * 
 * 
 * MQTT Messages
 * - Subscribed:
 *   - "gate/camera/cmnd" 
 *      -> "photo"                : Take and upload a photo
 *      -> "enable"               : Enable camera and allow PIR to trigger taking photos  (MQTT trigger still possible when disabled)
 *      -> "disable"              : Disable camera actions (PIR still enabled)
 *      -> "settings"             : Report current cam settings and status                           << NOT IMPLEMENTED
 *   - "gate/camera/setsetting" 
 *      -> "<setting>:<value>"    : Update the camera settings with the provided value
 *   - "gate/motion/cmnd" 
 *      -> "enable"               : Enable PIR motion feedback (default)
 *      -> "disable"              : Disable PIR motion feedback
 *      -> "delay:<seconds>"      : Set new delay/debounce between PIR triggers
 *   - "gate/temperature/cmnd" 
 *      -> "reading"              : Report the current temperature value
 *      -> "interval:<seconds>"   : Set the interval between temperature updates (default=30s) (0=disabled)
 *   - "gate/monitor/cmnd" 
 *      -> "restart"              : Trigger restart of ESP32
 *      -> "getstate"             : Report the current state and telemetry values (RSSI, Memory, ..)
 *      -> "getconfig"            : Report the current configuration
 *      -> "interval:<seconds>"   : Set the interval between state updates (default=60s) (0=disabled)
 *      -> "ReportState:<value>"  : Enable/disable reporting full device state    (true/false)
 *      -> "Reportwifi:<value>"   : Enable/disable reporting wifi strength        (true/false)
 * 
 * - Published:
 *   - "gate/motion/state"        -> "yes/no"                   : movement detected at gate
 *   - "gate/temperature/state"   -> "<value>"                  : current temperature value
 *   - "gate/camera/state"        -> "<photo/video settings>"   : photo/video uploaded, list of camera settings
 *   - "gate/monitor/config"      -> "<settings>"               : list of general settings
 *   - "gate/monitor/state"       -> "<parameters>"             : list of telemetry parameters
 *   - "gate/monitor/wifi"        -> "<value>"                  : current WiFi RSSI value           (DISABLED)
 * 
 * Pins:
 * - PIR        -> GPIO 13     : Data wire
 * - DS18B20    -> GPIO 2      : Data wire
 * 
 * ToDo: 
 * - return camera settings as JSON in MQTT msg
 * - OTA (??)
 * 
 * Revision:     
 *   20201223.1     23 December 2020
 *    - loop()          : only valid temperature returned i.e. if not -127 °C.
 *    - MQTT_callback() : also save config to SPIFFS for PIR and Temperature setting changes.
 *    - MQTT_init()     : disconnect MQTT session before each reconnect, set clean session to false on connect.
 *    - reportState()   : changed WiFi % calculation to not exceed 100 
 * 
 **************************************************************************/

#include <Arduino.h>
#include <WiFi.h>
#include <esp_camera.h>
#include <PubSubClient.h>
#include <OneWire.h>
#include <DallasTemperature.h>
#include <soc/rtc_cntl_reg.h>     // disable brownout problems
#include <SPIFFS.h>
#include <ArduinoJson.h>
#include <rom/rtc.h>
#include <esp_http_server.h>
#include <esp_http_client.h>
#include <fb_gfx.h>
#include "configuration.h"
#include "NetworkSettings.h"

httpd_handle_t stream_httpd = NULL;
WiFiClient wifiClient;
PubSubClient mqttClient(wifiClient);
OneWire wireBus(pinOneWire);
DallasTemperature sensorTemp(&wireBus);

volatile long lastMovementDetected = 0;             // Used to debounce PIR
volatile bool motionDetected = false;               // Set in ISR when PIR detected movement
bool actionTakePhoto = false;                       // Set by MQTT when photo must be taken
bool reportStatus = false;                          // Report settings via MQTT
bool requestTemperature = false;                    // Report temperature (once) when set (default: false)
bool runWebServer = false;

struct Config {
  bool PIR_enabled;                                 // Enable/disable photo capture remotely (default: true)
  int PIR_delay;                                    // time to ignore PIR movement interrupts (MQTT)
  int TempInterval;                                 // Interval between Temperature feedback (0 = disabled)
  bool CAM_enabled;                                 // Enable/disable photo capture remotely (default: true)
  bool ReportState;                                 // Enable/disable report device state (MQTT)
  bool ReportWiFi;                                  // Enable/disable report WiFi (MQTT)
  int StateInterval;                                // Interval between State feedback (0 = disabled) 
};
Config config;

struct Settings {
  bool isValid;                                     // Only use camera settings if flag is set, during successful SPIFFS read
  int framesize;
  int quality;
  int brightness;
  int contrast;
  int hmirror;
  int vflip;
};
Settings camSettings;

/**************************************************************************
 * BlinkLED
 * - blink the onboard LED.
 **************************************************************************/
void BlinkLED (int LoopCnt) {

  for (int i=0;i<LoopCnt;i++) {
    // Blink LED enabled, so do it.
    digitalWrite(pinBoardLED, LOW);
    delay(60);
    digitalWrite(pinBoardLED, HIGH);

    if (i<LoopCnt-1) delay(100);
  }
}

/**************************************************************************
 * cam_SaveSettings
 * - Save (some of) the current camera settings to the SPIFFS config file.
 **************************************************************************/
void cam_SaveSettings(bool RemoveOnly) {

  if (camSettings.isValid) {
    if ( SPIFFS.begin(true) ) {
      Serial.println("- SaveSettings: SPIFFS mounted");
      if ( SPIFFS.exists(SETTINGSFILE) ) {
        // First delete config file to ensure it is non-existing.
        SPIFFS.remove(SETTINGSFILE);
      }

      if (!RemoveOnly) {
        File settingsFile = SPIFFS.open(SETTINGSFILE, FILE_WRITE);
        if ( settingsFile ) {
          Serial.println("- SaveSettings: new settings file created");
          StaticJsonDocument<256> jsonDoc;
          // Set the values in the document
          jsonDoc["brightness"] = camSettings.brightness;
          jsonDoc["contrast"] = camSettings.contrast;
          jsonDoc["framesize"] = camSettings.framesize;
          jsonDoc["quality"] = camSettings.quality;
          jsonDoc["hmirror"] = camSettings.hmirror;
          jsonDoc["vflip"] = camSettings.vflip;

          if (serializeJson(jsonDoc, settingsFile) == 0) {
            Serial.println(F("\t---! SaveSettings: Failed to write to file"));
          }
          else { 
            Serial.println(F("\t- SaveSettings: Settings file created"));
          }
          // Close the file
          settingsFile.close();
      
        } else {
          Serial.println("\t---! SaveSettings: Failed to create file");
        }
      }
    } else {
      Serial.println("\t---! SaveSettings: SPIFFS mount failed");
    }
  } else {
    Serial.println("\t-! SaveSettings: not valid settings struct");
  }
}

/**************************************************************************
 * cam_ReadSettings
 * - Get the camera settings on initialisation.
 **************************************************************************/
static esp_err_t cam_ReadSettings() {
  int res = 0;
  bool readConfigOK = false;

  if (SPIFFS.begin(true)) {
    //Serial.println("- ReadSettings: SPIFFS mounted");
    if ( SPIFFS.exists(SETTINGSFILE) ) {
      File settingsFile = SPIFFS.open(SETTINGSFILE, FILE_READ);

      if ( settingsFile ) {
        // Config file opened ok. Read contents.
        StaticJsonDocument<512> jsonDoc;
        DeserializationError error = deserializeJson(jsonDoc, settingsFile);
        if (error) {
          Serial.print(F("\t---! ReadSettings: Failed to deserialize file. Err: ")); Serial.println(error.c_str());           
        } else {
          camSettings.brightness = jsonDoc["brightness"] | 0;     // brightness (-2 > +2)
          camSettings.contrast = jsonDoc["contrast"] | 0;         // contrast (-2 > +2)
          camSettings.framesize = jsonDoc["framesize"] | 6;       // frame size (0 > 10   default: 6 = VGA(640x480) )
          camSettings.quality = jsonDoc["quality"] | 10;          // quality (10 > 63     default: high )
          camSettings.hmirror = jsonDoc["hmirror"] | 0;           // horizontal mirror (0/1 default: no)
          camSettings.vflip = jsonDoc["vflip"] | 0;               // verical flip (0/1    default: no)
          camSettings.isValid = true;

          readConfigOK = true;
          res = 1;
        }

      } else {
        Serial.println("\t---! ReadSettings: Failed to open existing file for reading.");
      }
      // Close the file
      settingsFile.close();
    } else {
      Serial.println("\t---!  ReadSettings: SPIFFS does not exist");
    }
  } else {
    Serial.println("\t---!  ReadSettings: SPIFFS mount failed");
  }

  if ( !readConfigOK ) {
    // Settings file in SPIFFS not found/loaded. Initialize with defaults.
    camSettings.brightness = 0;             // brightness (-2 > +2)
    camSettings.contrast = 0;               // contrast (-2 > +2)
    camSettings.framesize = 6;              // frame size (0 > 10   default: 6 = VGA(640x480) )
    camSettings.quality = 10;               // quality (10 > 63     default: high )
    camSettings.hmirror = 0;                // horizontal mirror (0/1 default: no)
    camSettings.vflip = 0;                  // verical flip (0/1    default: no)
    camSettings.isValid = true;

    Serial.println("\t- ReadSettings: Unable to read settings. Defaults set. Saving new settings....");

    // Save settings file to SPIFFS.
    cam_SaveSettings(false);

    res = 1;
  }
  return res;  
}

/**************************************************************************
 * cam_ReportSettings
 * - Give feedback on the current settings and status. (TODO: still not implemented)
 **************************************************************************/
void cam_ReportSettings() {
  // TO BE IMPLEMENTED
}

/**************************************************************************
 * cam_UpdateSettings
 * - set camera property based on provided setting and value (format: <setting>:<value>)
 * - setting and value must be delimited by ":"
 * - value must be numeric
 * 
 * settings template taken from: 
 * - https://randomnerdtutorials.com/esp32-cam-video-streaming-face-recognition-arduino-ide/
 * - https://randomnerdtutorials.com/esp32-cam-ov2640-camera-settings/
 * 
 **************************************************************************/
static esp_err_t cam_UpdateSettings (const String& newSettingValue) {
  int posDelimiter = -1;
  int val = 0;
  int res = -1;
  int sign = 1;
  bool validSetting = true;
  bool saveSettings = false;

  Serial.print("- cam_UpdateSettings: "); Serial.println(newSettingValue);

  posDelimiter = newSettingValue.indexOf(":");
  if (newSettingValue == "reset") {
    // Remove existing settigs from SPIFFS
    cam_SaveSettings(true);
    // Read default values and create new SPIFFS file.
    cam_ReadSettings();

  } else if ( posDelimiter>0 && posDelimiter < newSettingValue.length() ) {
    char variable[posDelimiter+1];
    newSettingValue.substring(0,posDelimiter).toCharArray(variable,sizeof(variable));

    if (newSettingValue.charAt(posDelimiter+1) == '-') {
      // The provided value is negative (allowed). Keep the sign and increase the offset.
      sign = -1;
      posDelimiter++;
    }
    if (posDelimiter+1 < newSettingValue.length() ) {
      // Validate that the provided value is numeric.
      for (int i=posDelimiter+1; i<newSettingValue.length(); i++) {
        if ( !isdigit(newSettingValue.charAt(i)) ) {
          Serial.println("\t!! NON-NUMERIC settings parameter");
          validSetting = false;
        }
      }
    } else {
      Serial.println("\t!! INVALID setting: no value provided");
      validSetting = false;
    }
  
    if (validSetting) {
      // Extract the setting (variable) and new value (val) from the provided parameter. 
      val = newSettingValue.substring(posDelimiter+1).toInt();
      val = val*sign;

      Serial.print("\t- DO CHANGE: variable="); Serial.print(variable); Serial.print(" val="); Serial.println(val);

      sensor_t * s = esp_camera_sensor_get();

           if (!strcmp(variable, "saturation")) res = s->set_saturation(s, val);
      else if (!strcmp(variable, "gainceiling")) res = s->set_gainceiling(s, (gainceiling_t)val);
      else if (!strcmp(variable, "colorbar")) res = s->set_colorbar(s, val);
      else if (!strcmp(variable, "awb")) res = s->set_whitebal(s, val);
      else if (!strcmp(variable, "agc")) res = s->set_gain_ctrl(s, val);
      else if (!strcmp(variable, "aec")) res = s->set_exposure_ctrl(s, val);
      else if (!strcmp(variable, "awb_gain")) res = s->set_awb_gain(s, val);
      else if (!strcmp(variable, "agc_gain")) res = s->set_agc_gain(s, val);
      else if (!strcmp(variable, "aec_value")) res = s->set_aec_value(s, val);
      else if (!strcmp(variable, "aec2")) res = s->set_aec2(s, val);
      else if (!strcmp(variable, "dcw")) res = s->set_dcw(s, val);
      else if (!strcmp(variable, "bpc")) res = s->set_bpc(s, val);
      else if (!strcmp(variable, "wpc")) res = s->set_wpc(s, val);
      else if (!strcmp(variable, "raw_gma")) res = s->set_raw_gma(s, val);
      else if (!strcmp(variable, "lenc")) res = s->set_lenc(s, val);
      else if (!strcmp(variable, "special_effect")) res = s->set_special_effect(s, val);
      else if (!strcmp(variable, "wb_mode")) res = s->set_wb_mode(s, val);
      else if (!strcmp(variable, "ae_level")) res = s->set_ae_level(s, val);
      // For each of the following settings, also update the settings struct and save to SPIFFS.
      else if (!strcmp(variable, "framesize")) {
        if (s->pixformat == PIXFORMAT_JPEG) { 
          res = s->set_framesize(s, (framesize_t)val);
          camSettings.framesize = (framesize_t)val;
          saveSettings = true;
        }
      }
      else if (!strcmp(variable, "quality")) {
        res = s->set_quality(s, val);
        camSettings.quality = val;
        saveSettings = true;
      }
      else if (!strcmp(variable, "contrast")) {
        res = s->set_contrast(s, val);
        camSettings.contrast = val;
        saveSettings = true;
      }
      else if (!strcmp(variable, "brightness")) {
        res = s->set_brightness(s, val);
        camSettings.brightness = val;
        saveSettings = true;
      }
      else if (!strcmp(variable, "hmirror")) {
        res = s->set_hmirror(s, val);
        camSettings.hmirror = val;
        saveSettings = true;
      }  
      else if (!strcmp(variable, "vflip")) {
        res = s->set_vflip(s, val);
        camSettings.vflip = val;
        saveSettings = true;
      }  
/*
      else if(!strcmp(variable, "face_detect")) {
          detection_enabled = val;
          if(!detection_enabled) {
              recognition_enabled = 0;
          }
      }
      else if(!strcmp(variable, "face_enroll")) is_enrolling = val;
      else if(!strcmp(variable, "face_recognize")) {
          recognition_enabled = val;
          if(recognition_enabled){
              detection_enabled = val;
          }
      }
*/
      else {
        Serial.print("\t!! UNKNOWN/UNSUPPORTED setting: "); Serial.println(newSettingValue);
      }
    }
  } else {
    Serial.print("\t!! INVALID setting format: "); Serial.println(newSettingValue);
  }

  if (saveSettings) {
    // Save the changed camera settings.
    cam_SaveSettings(false);
  }
  
  return res;
}

/**************************************************************************
 * cam_init
 * - Set up and configure the camera
 **************************************************************************/
static esp_err_t cam_init()
{
  int res = ESP_OK;

  camera_config_t config;
  config.ledc_channel = LEDC_CHANNEL_0;
  config.ledc_timer = LEDC_TIMER_0;
  config.pin_d0 = Y2_GPIO_NUM;
  config.pin_d1 = Y3_GPIO_NUM;
  config.pin_d2 = Y4_GPIO_NUM;
  config.pin_d3 = Y5_GPIO_NUM;
  config.pin_d4 = Y6_GPIO_NUM;
  config.pin_d5 = Y7_GPIO_NUM;
  config.pin_d6 = Y8_GPIO_NUM;
  config.pin_d7 = Y9_GPIO_NUM;
  config.pin_xclk = XCLK_GPIO_NUM;
  config.pin_pclk = PCLK_GPIO_NUM;
  config.pin_vsync = VSYNC_GPIO_NUM;
  config.pin_href = HREF_GPIO_NUM;
  config.pin_sscb_sda = SIOD_GPIO_NUM;
  config.pin_sscb_scl = SIOC_GPIO_NUM;
  config.pin_pwdn = PWDN_GPIO_NUM;
  config.pin_reset = RESET_GPIO_NUM;
  config.xclk_freq_hz = 20000000;
  config.pixel_format = PIXFORMAT_JPEG;
  //init with high specs to pre-allocate larger buffers
  if (psramFound()) {
    config.frame_size = FRAMESIZE_UXGA;
    config.jpeg_quality = 10;
    config.fb_count = 2;
  } else {
    config.frame_size = FRAMESIZE_SVGA;
    config.jpeg_quality = 12;
    config.fb_count = 1;
  }

  if (camSettings.isValid) {
    // Initialize the camera with previously stored values. 
    // NOTE this is just a subset of the possible values. The other settings will have default values after each start.
    res = esp_camera_init(&config);
    if (res == ESP_OK) {
      sensor_t * s = esp_camera_sensor_get();

      s->set_framesize(s, (framesize_t)camSettings.framesize);        // framesize (e.g. CIF, VGA, ..) 
      s->set_quality(s, camSettings.quality);                         // set the frame size
      s->set_contrast(s, camSettings.contrast);                       // override the automatic/default contrast
      s->set_brightness(s, camSettings.contrast);                     // override the automatic/default brightness
      s->set_hmirror(s, camSettings.hmirror);                         // flip image horizontally
      res = s->set_vflip(s, camSettings.vflip);                       // flip image vertically

    } else {
      Serial.printf("Camera init failed with error 0x%x!\nRestarting in 10s...", res);
    }
  }

  return res;
}

/**************************************************************************
 * showFileConfig
 * - Show the contents of the SPIFFS config file.
 **************************************************************************/
void showFileConfig() {
  // Open file for reading
  if ( SPIFFS.exists(CONFIGFILE) ) {
    File file = SPIFFS.open(CONFIGFILE, FILE_READ);
    if (file) {
      // Extract each characters by one by one
      while (file.available()) {
        Serial.print((char)file.read());
      }
      Serial.println("\n");

      // Close the file
      file.close();
    } else {  
      Serial.println(F("showConfig: Failed to read SPIFFS file!"));
    } 
  }
}

/**************************************************************************
 * getRestartReason
 * - Get last ESP32 reset reason.
 **************************************************************************/
void getRestartReason(char* Reason, int rsnLength) {

  int rreason = (int) rtc_get_reset_reason( (RESET_REASON) 0);

  switch (rreason)
  {
    case 1 : strncpy(Reason, "POWERON_RESET", rsnLength);break;             // - 1,  Vbat power on reset
    case 3 : strncpy(Reason, "SW_RESET", rsnLength);break;                  // - 3,  Software reset digital core
    case 4 : strncpy(Reason, "OWDT_RESET", rsnLength);break;                // - 4,  Legacy watch dog reset digital core
    case 5 : strncpy(Reason, "DEEPSLEEP_RESET", rsnLength);break;           // - 5,  Deep Sleep reset digital core
    case 6 : strncpy(Reason, "SDIO_RESET", rsnLength);break;                // - 6,  Reset by SLC module, reset digital core
    case 7 : strncpy(Reason, "TG0WDT_SYS_RESET", rsnLength);break;          // - 7,  Timer Group0 Watch dog reset digital core
    case 8 : strncpy(Reason, "TG1WDT_SYS_RESET", rsnLength);break;          // - 8,  Timer Group1 Watch dog reset digital core
    case 9 : strncpy(Reason, "RTCWDT_SYS_RESET", rsnLength);break;          // - 9,  RTC Watch dog Reset digital core
    case 10 : strncpy(Reason, "INTRUSION_RESET", rsnLength);break;          // - 10, Instrusion tested to reset CPU
    case 11 : strncpy(Reason, "TGWDT_CPU_RESET", rsnLength);break;          // - 11, Time Group reset CPU
    case 12 : strncpy(Reason, "SW_CPU_RESET", rsnLength);break;             // - 12, Software reset CPU
    case 13 : strncpy(Reason, "RTCWDT_CPU_RESET", rsnLength);break;         // - 13, RTC Watch dog Reset CPU
    case 14 : strncpy(Reason, "EXT_CPU_RESET", rsnLength);break;            // - 14, for APP CPU, reseted by PRO CPU
    case 15 : strncpy(Reason, "RTCWDT_BROWN_OUT_RESET", rsnLength);break;   // - 15, Reset when the vdd voltage is not stable
    case 16 : strncpy(Reason, "RTCWDT_RTC_RESET", rsnLength);break;         // - 16, RTC Watch dog reset digital core and rtc module
    default : strncpy(Reason, "NO_MEAN", rsnLength);                        // - undefined (not covered in this function)
  }
}

/**************************************************************************
 * RSSItoPrecentage
 * - Convert the RSSI dBi value to equavalent percentage.
 **************************************************************************/
int RSSItoPrecentage(int valRSSI) {
  int result = 0;

  result = (WiFi.RSSI()+100)*2;
  if (result>100) {
    result = 99;
  } else if (result < 0) {
    result = 0;
  }
  return result;
}

/**************************************************************************
 * reportState
 * - Feedback the current app state and telemetry values.
 **************************************************************************/
void reportState() {
  const int LEN = 30;
  char startReason[LEN];
  char UpTime[LEN];
  long espTemperature = temperatureRead();
  long UptimeSeconds = esp_timer_get_time()/1000/1000;
  String ipAddress = WiFi.localIP().toString();

  getRestartReason(startReason, LEN);
  sprintf(UpTime, "%01.0fd%01.0f:%02.0f:%02.0f", floor(UptimeSeconds/86400.0), floor(fmod((UptimeSeconds/3600.0),24.0)), floor(fmod(UptimeSeconds,3600.0)/60.0), fmod(UptimeSeconds,60.0));

  StaticJsonDocument<640> doc;
  // Set the values in the document
  doc["IP Address"] = ipAddress;                                  // device IP address
  doc["RSSI (dBm)"] = WiFi.RSSI();                                // dBm value (negative)
  doc["wifi"] = RSSItoPrecentage( WiFi.RSSI() );                  // dBm value converted to signal strength percentage
  doc["Core Temperature (°C)"] = espTemperature;                  // ESP core temperature
  doc["Uptime"] = UpTime;                                         // day.hours:minutes:seconds since last boot
  doc["Start Reason"] = startReason;                              // reason for last restart
  doc["Free Heap Memory"] = esp_get_free_heap_size();
  doc["Min Free Heap"] = esp_get_minimum_free_heap_size();

/*
  esp_chip_info_t espInfo;
  esp_chip_info(&espInfo);
  doc["ESP Core Count"] = espInfo.cores;
  doc["ESP Model"] = espInfo.model;
  doc["Revision"] = espInfo.revision;
  doc["IDFversion"] = esp_get_idf_version();
*/

  char buffer[640];
  serializeJson(doc, buffer);
  mqttClient.publish(MQTT_PUB_STATE, buffer);
}

/**************************************************************************
 * reportWiFi
 * - Feedback the current WiFi RSSI value.
 **************************************************************************/
void reportWiFi() {

  //mqttClient.publish( MQTT_PUB_WIFI, String( (WiFi.RSSI()+100)*2 ).c_str() );
  mqttClient.publish( MQTT_PUB_WIFI, String( RSSItoPrecentage( WiFi.RSSI() ) ).c_str() );

/*
  StaticJsonDocument<124> doc;
  // Set the values in the document
  doc["RSSI (dBm)"] = WiFi.RSSI();                                // dBm value (negative)
  doc["wifi"] = (WiFi.RSSI()+100)*2;                              // dBm value converted to signal strength percentage

  char buffer[124];
  serializeJson(doc, buffer);
  mqttClient.publish(MQTT_PUB_WIFI, buffer);
*/
}

/**************************************************************************
 * reportConfig
 * - Feedback the general settings (that is currently in memory).
 **************************************************************************/
void reportConfig() {

  StaticJsonDocument<256> configDoc;
  // Set the values in the document
  configDoc["CAM_enabled"] = config.CAM_enabled;
  configDoc["PIR_enabled"] = config.PIR_enabled;
  configDoc["PIR_delay"] = config.PIR_delay;
  configDoc["ReportState"] = config.ReportState;
  configDoc["ReportWiFi"] = config.ReportWiFi;
  configDoc["TempInterval"] = config.TempInterval;
  configDoc["StateInterval"] = config.StateInterval;

  char buffer[256];
  serializeJson(configDoc, buffer);
  mqttClient.publish(MQTT_PUB_CONFIG, buffer);

}

/**************************************************************************
 * saveConfig
 * - Save the current configuration to the SPIFFS config file.
 **************************************************************************/
void saveConfig() {

  if (SPIFFS.begin(true)) {
    //Serial.println("SaveConfig: SPIFFS mounted");
    if ( SPIFFS.exists(CONFIGFILE) ) {
      // First delete config file to ensure it is non-existing.
      SPIFFS.remove(CONFIGFILE);
    }

    File configFile = SPIFFS.open(CONFIGFILE, FILE_WRITE);
    if ( configFile ) {
      //Serial.println("SaveConfig: new config file created");
      StaticJsonDocument<256> configDoc;
      // Set the values in the document
      configDoc["CAM_enabled"] = config.CAM_enabled;
      configDoc["PIR_enabled"] = config.PIR_enabled;
      configDoc["PIR_delay"] = config.PIR_delay;
      configDoc["TempInterval"] = config.TempInterval;
      configDoc["ReportState"] = config.ReportState;
      configDoc["ReportWiFi"] = config.ReportWiFi;
      configDoc["StateInterval"] = config.StateInterval;

      if (serializeJson(configDoc, configFile) == 0) {
        Serial.println(F("\t---! Failed to write to file"));
      }
      // Close the file
      configFile.close();
  
    } else {
      Serial.println("\t---! SaveConfig: Failed to create file");
    }
  } else {
    Serial.println("\t---! SaveConfig: SPIFFS mount failed");
  }
}

/**************************************************************************
 * cam_StreamHandler
 * - 
 **************************************************************************/
static esp_err_t cam_StreamHandler(httpd_req_t *req) {
  camera_fb_t * fb = NULL;
  esp_err_t res = ESP_OK;
  size_t _jpg_buf_len = 0;
  uint8_t * _jpg_buf = NULL;
  char * part_buf[64];

  Serial.println("Camera StreamHandler started");

  res = httpd_resp_set_type(req, _STREAM_CONTENT_TYPE);
  if (res != ESP_OK) {
    return res;
  }
  
  // loop until web server is stopped (MQTT video toggle) ...
//  while (runWebServer) {

  while (true) {
    fb = esp_camera_fb_get();
    if (!fb) {
      Serial.println("\t---! SH: Camera capture failed");
      res = ESP_FAIL;
    } else {
      if (fb->width > 400) {
        if (fb->format != PIXFORMAT_JPEG) {
          bool jpeg_converted = frame2jpg(fb, 80, &_jpg_buf, &_jpg_buf_len);
          esp_camera_fb_return(fb);
          fb = NULL;
          if (!jpeg_converted) {
            Serial.println("\t---! SH: JPEG compression failed");
            res = ESP_FAIL;
          }
        } else {
          _jpg_buf_len = fb->len;
          _jpg_buf = fb->buf;
        }
      }
    }
    if (res == ESP_OK) {
      size_t hlen = snprintf((char *)part_buf, 64, _STREAM_PART, _jpg_buf_len);
      res = httpd_resp_send_chunk(req, (const char *)part_buf, hlen);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, (const char *)_jpg_buf, _jpg_buf_len);
    }
    if (res == ESP_OK) {
      res = httpd_resp_send_chunk(req, _STREAM_BOUNDARY, strlen(_STREAM_BOUNDARY));
    }
    if (fb) {
      esp_camera_fb_return(fb);
      fb = NULL;
      _jpg_buf = NULL;
    } else if (_jpg_buf) {
      free(_jpg_buf);
      _jpg_buf = NULL;
    }
    if (res != ESP_OK) {
      break;
    }
    //Serial.printf("MJPG: %uB\n",(uint32_t)(_jpg_buf_len));
  }

  Serial.println("- SH: StreamHandler stopped");
  return res;
}

/**************************************************************************
 * cam_StopStartHTTPServer
 * - Start the web server for video
 **************************************************************************/
void cam_StopStartHTTPServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

Serial.print("-- cam_StopStartHTTPServer: stream is null - "); Serial.println( (stream_httpd == NULL) );

  if (runWebServer) {
    Serial.println("- Cam StartServer start");

    httpd_uri_t index_uri = {
      .uri       = "/",
      .method    = HTTP_GET,
      .handler   = cam_StreamHandler,
      .user_ctx  = NULL
    };
    //Serial.printf("Starting web server on port: '%d'\n", config.server_port);
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
      httpd_register_uri_handler(stream_httpd, &index_uri);
    }
  } else {
    Serial.println("- Cam StartServer stop");
    if (stream_httpd != NULL) {
      httpd_stop(stream_httpd);
    }
  }
  Serial.println("Cam StartServer done");
}

/**************************************************************************
 * cam_StopStartHTTPServer
 * - Start the web server for video
 **************************************************************************/
void cam_StartHTTPServer() {
  httpd_config_t config = HTTPD_DEFAULT_CONFIG();
  config.server_port = 80;

Serial.print("-- cam_StartHTTPServer: stream is null - "); Serial.println( (stream_httpd == NULL) );

  if (stream_httpd == NULL) {
    Serial.println("- Cam StartServer start");

    httpd_uri_t index_uri = {
      .uri       = "/",
      .method    = HTTP_GET,
      .handler   = cam_StreamHandler,
      .user_ctx  = NULL
    };
    //Serial.printf("Starting web server on port: '%d'\n", config.server_port);
    if (httpd_start(&stream_httpd, &config) == ESP_OK) {
      httpd_register_uri_handler(stream_httpd, &index_uri);
    }
  } else {
    Serial.println("- Cam StartServer already running");
  }
  Serial.println("Cam StartServer done");
}

/**************************************************************************
 * readConfig
 * - Get the settings on initialisation.
 **************************************************************************/
static esp_err_t readConfig() {
  int res = 0;
  bool readConfigOK = false;

  if (SPIFFS.begin(true)) {
    //Serial.println("ReadConfig: SPIFFS mounted");
    if ( SPIFFS.exists(CONFIGFILE) ) {
      File configFile = SPIFFS.open(CONFIGFILE, FILE_READ);

      if ( configFile ) {
        // Config file opened ok. Read contents.
        StaticJsonDocument<256> configDoc;
        DeserializationError error = deserializeJson(configDoc, configFile);
        if (error) {
          Serial.print(F("\t---! Failed to deserialize file. Err: ")); Serial.println(error.c_str());           
        } else {
          config.CAM_enabled = configDoc["CAM_enabled"] | true;           // Enable taking photos and upload.
          config.PIR_enabled = configDoc["PIR_enabled"] | true;           // Enable motion detection.
          config.PIR_delay = configDoc["PIR_delay"] | 20000;              // Wait 20 seconds before reporting new motion event.
          config.TempInterval = configDoc["TempInterval"] | 60000;        // Upload temperature once per minute. 
          config.ReportState = configDoc["ReportState"] | true;           // Upload device state.
          config.ReportWiFi = configDoc["ReportWiFi"] | false;            // Upload WiFi value. (default: false) 
          config.StateInterval = configDoc["StateInterval"] | 60000;      // Upload state values once per minute.

          readConfigOK = true;
          res = 1;
        }

      } else {
        Serial.println("\t---! Failed to open existing file for reading.");
      }
      // Close the file
      configFile.close();
    } else {
      Serial.println("\t---!  ReadConfig: SPIFFS does not exist");
    }
  } else {
    Serial.println("\t---!  ReadConfig: SPIFFS mount failed");
  }

  if ( !readConfigOK ) {
    // Configuration in SPIFFS not found/loaded. Initialize with defaults and save to new SPIFFS file.
    config.CAM_enabled = true;          // Enable taking photos and upload.
    config.PIR_enabled = true;          // Enable motion detection.
    config.PIR_delay = 20000;           // Wait 20 seconds before reporting new motion event.
    config.TempInterval = 60000;        // Upload temperature once per minute.
    config.ReportState =  true;         // Upload device state.
    config.ReportWiFi = false;          // Upload WiFi value.  (default: false)
    config.StateInterval = 60000;       // Upload state values once per minute.

    Serial.println("\t- Unable to read config. Defaults set. Saving new config....");

    // Save config to SPIFFS.
    saveConfig();

    res = 1;
  }
  return res;  
}

/**************************************************************************
 * WiFi_init
 * - Connect to local SSID
 **************************************************************************/
bool WiFi_init()
{
  int connAttempts = 0;
  Serial.print("\r\nConnecting to: "); Serial.println(ssid);
  WiFi.begin(ssid, password);
  while (WiFi.status() != WL_CONNECTED ) {
    delay(500);
    Serial.print(".");
    if (connAttempts > 20) return false;
    connAttempts++;
  }
  Serial.println(" OK");
  return true;
}

/**************************************************************************
 *  MQTT_init
 *  - Connect/reconnect to WiFi
 *  - Subscribe to MQTT topics
***************************************************************************/
void MQTT_init() {

  // Loop until we're reconnected
//  while (!client.connected() && i<wifiMaxRetry) {
  mqttClient.disconnect();
  if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
    Serial.print("Attempting MQTT connection... ");
    // Attempt to connect (cleanSession = false : inform broker to not start new session when reconnect)
    if (mqttClient.connect("ESP32Cam", MQTT_user, MQTT_pwd,NULL,0,false,NULL,false)) {
      Serial.print("connected. "); Serial.print(" WiFi="); Serial.println(WiFi.RSSI());
      // Subscribe
      mqttClient.subscribe(MQTT_SUB_CAMCOMMAND);
      mqttClient.subscribe(MQTT_SUB_MOTION);
      mqttClient.subscribe(MQTT_SUB_CAMSETTING);
      mqttClient.subscribe(MQTT_SUB_TEMP);
      mqttClient.subscribe(MQTT_SUB_MONITOR);
      Serial.println("Subscribed to MQTT");
    } else {
      Serial.print("failed! rc="); Serial.print(mqttClient.state());
      Serial.print(" RSSI="); Serial.print(WiFi.RSSI()); Serial.print(" IP="); Serial.println(WiFi.localIP());
      // Wait before retrying
      delay(1000);
    }
  }
}

/**************************************************************************
 *  MQTT_recon
 *  - Reconnect MQTT client (if WiFi is connected)
***************************************************************************/
void MQTT_recon() {
  while (!mqttClient.connected()) {
    // Attempt to connect
    if (!mqttClient.connect("ESP32Cam")) {
      delay(1000);
    }
  }
}

/**************************************************************************
 *  MQTT_callback
 *  - Handle received MQTT messages
 **************************************************************************/
void MQTT_callback (char* topic, byte* message, unsigned int length) {
  String msgValue;
  bool configChanged = false;

  Serial.print("MQTT Message arrived on topic: ");
  Serial.print(topic);
  Serial.print(". Message: ");
  for (int i = 0; i < length; i++) {
    Serial.print((char)message[i]);
    msgValue += (char)message[i];
  }
  Serial.println();

  // Process the received MQTT topics.. 

// * - "gate/camera/cmnd" 
// *      -> "photo"              : take and upload a photo
// *      -> "enable"             : enable camera and allow PIR to trigger taking photos (MQTT trigger still possible when disabled)
// *      -> "disable"            : disable camera actions (PIR still enabled)
// *      -> "settings"           : report current cam settings and status                           << NOT IMPLEMENTED
  if (String(topic) == MQTT_SUB_CAMCOMMAND) 
  { 
    if (msgValue == "photo") {
      Serial.println("\t- MQTT Take and upload photo");
      actionTakePhoto = true;
    } else if (msgValue == "video") {                                     // now web server runs all the time. no stop/start needed.
      //      actionTakeVideo = true;
      if ( !runWebServer ) {
        Serial.println("\t- MQTT Video - start");
        runWebServer = true;
        cam_StopStartHTTPServer();                                        // Start running the Web Server
      } else {
        Serial.println("\t- MQTT Video - stop");
        runWebServer = false;
        cam_StopStartHTTPServer();                                        // Start running the Web Server
      }
    } else if (msgValue == "enable") {
      Serial.println("\t- MQTT enable camera");
      configChanged = (config.CAM_enabled != true);
      config.CAM_enabled = true;                                          // Enable Camera
    }  else if (msgValue == "disable") {
      Serial.println("\t- MQTT disable camera");
      configChanged = (config.CAM_enabled != false);
      config.CAM_enabled = false;                                         // Disable Camera
    } else if (msgValue == "settings") {
      Serial.println("\t- MQTT return current camera settings");
      cam_ReportSettings();
    } else {
      Serial.print(" UNKNOWN CAMERA action ("); Serial.print(msgValue); Serial.println(")");
    }
  }

// * - "gate/camera/setsetting" 
// *      -> "<setting>:<value>"  : Update the camera settings with the provided value
  else if (String(topic) == MQTT_SUB_CAMSETTING ) 
  { 
    Serial.println("\t- MQTT update camera setting");
    cam_UpdateSettings(msgValue);
  }

// * - "gate/motion/cmnd" 
// *      -> "enable"             : enable PIR motion feedback (default)
// *      -> "disable"            : disable PIR motion feedback
// *      -> "delay:<seconds>"    : set new delay/debounce between PIR triggers
  else if (String(topic) == MQTT_SUB_MOTION ) 
  { 
    if (msgValue == "disable") {
      Serial.println("\t- MQTT disable PIR");
      configChanged = (config.PIR_enabled != false);
      config.PIR_enabled = false;                                     // Disable PIR sensor
    } else if (msgValue == "enable") {
      Serial.println("\t- MQTT enable PIR");
      motionDetected = false;                                         // Start clean, don't report on any previous triggers
      configChanged = (config.PIR_enabled != true);
      config.PIR_enabled = true;                                      // Enable PIR sensor
    } else if (msgValue.substring(0,5) == "delay") {
      Serial.print("\t- MQTT set PIR debounce delay");
      int valSplit = msgValue.indexOf(":"); 
      if (valSplit>0 && valSplit<msgValue.length() ) {
        // Seems like a valid parameter
        configChanged = (config.PIR_delay != msgValue.substring(valSplit+1).toInt()*1000 );
        config.PIR_delay = msgValue.substring(valSplit+1).toInt()*1000;
        Serial.println(" - "); Serial.println(config.PIR_delay);
      } else {
        Serial.println(" >>> INVALID !!");
      }
    }
    else {
      Serial.print(" UNKNOWN MOTION action ("); Serial.print(msgValue); Serial.println(")");
    }
  }

// * - "gate/temperature/cmnd" 
// *      -> "reading"            : report the current temperature value
// *      -> "interval:<seconds>" : set the interval between temperature updates (0=disabled)
  else if (String(topic) == MQTT_SUB_TEMP ) 
  { 
    if (msgValue == "reading") {
      Serial.println("\t- MQTT request Temperature value");
      requestTemperature = true;                                                  // Feed back the Temperature reading on next loop
    } else if (msgValue.substring(0,8) == "interval") {
      Serial.print("\t- MQTT set Temperature interval ");
      int valSplit = msgValue.indexOf(":"); 
      if (valSplit>0 && valSplit<msgValue.length() ) {
        // Seems like a valid parameter
        configChanged = (config.TempInterval != msgValue.substring(valSplit+1).toInt()*1000 );
        config.TempInterval = msgValue.substring(valSplit+1).toInt()*1000;        // Set the Temperature feedback interval period
        Serial.print(" NewVal="); Serial.println(config.TempInterval);
      } else {
        Serial.println(" >>> INVALID !!");
      }
    }
  }

// * - "gate/monitor/cmnd" 
// *      -> "restart"                  : trigger restart of ESP32
// *      -> "getstate"                 : report the current state and telemetry values (RSSI, Memory, ..)
// *      -> "getconfig"                : report the current state and telemetry values (RSSI, Memory, ..)
// *      -> "interval:<seconds>"       : set the interval between state updates (default=30s) (0=disabled)
// *      -> "ReportState:<true/false>" : Enable/disable reporting full device state
// *      -> "Reportwifi:<true/false>"  : Enable/disable reporting wifi strength
  else if (String(topic) == MQTT_SUB_MONITOR ) 
  { 
    if (msgValue == "restart") {
      Serial.println("\t- MQTT -- RESTART ESP32");
      BlinkLED(3);                                                        // Visual indication during debugging
      delay(100);
      esp_restart();                                                      // RESTART ESP32 !!!!!
    } else if (msgValue == "getstate") {
      Serial.println("\t- MQTT request State and Telemetry values");
      BlinkLED(1);
      reportState();                                                      // Feedback current telemetry values (once)
    } else if (msgValue == "getconfig") {
      Serial.println("\t- MQTT request Configuration values");
      BlinkLED(1);
      reportConfig();                                                     // Feedback current configuration (once)
    } else if (msgValue.substring(0,8) == "interval") {
      Serial.print("\t- MQTT set State interval ");
      int valSplit = msgValue.indexOf(":"); 
      if (valSplit>0 && valSplit < msgValue.length() ) {
        // Seems like a valid parameter
        configChanged = (config.StateInterval != msgValue.substring(valSplit+1).toInt()*1000 );
        config.StateInterval = msgValue.substring(valSplit+1).toInt()*1000;        // Set State feedback interval (in milliseconds!)
        Serial.print(" NewVal="); Serial.println(config.StateInterval);
      } else {
        Serial.println(" >>> INVALID !!");
      }
    } else if (msgValue.substring(0,11) == "ReportState") {
      Serial.print("\t- MQTT set ReportState ");

      int valSplit = msgValue.indexOf(":"); 
      if (valSplit > 0 && valSplit < msgValue.length() ) {
        // Seems like a valid parameter
        if (msgValue.substring(valSplit+1) == "true") {
          configChanged = (config.ReportState != true );
          config.ReportState = true;                                                // Enable reporting state
        } else if (msgValue.substring(valSplit+1) == "false") {
          configChanged = (config.ReportState != false );
          config.ReportState = false;                                               // Disable reporting state
        }
        Serial.println(config.ReportState);
      }  
    } else if (msgValue.substring(0,10) == "ReportWiFi") {
      Serial.print("\t- MQTT set ReportWiFi ");

      int valSplit = msgValue.indexOf(":"); 
      if (valSplit > 0 && valSplit < msgValue.length() ) {
        // Seems like a valid parameter
        if (msgValue.substring(valSplit+1) == "true") {
          configChanged = (config.ReportWiFi != true );
          config.ReportWiFi = true;                                                 // Enable reporting WiFi
        } else if (msgValue.substring(valSplit+1) == "false") {
          configChanged = (config.ReportWiFi != false );
          config.ReportWiFi = false;                                                // Disable reporting WiFi
        } 
        Serial.println(config.ReportWiFi);
      }  
    }
  }
  if (configChanged) {
    // The configuration changed. Update the local SPIFFS config file with the new value.
    saveConfig();
    reportConfig();     // feedback of new configuration settings
  }
}

/**************************************************************************
 * isrDetectMovement
 * - interrupt routine called when the PIR detected movement.
 **************************************************************************/
static void IRAM_ATTR isrDetectMovement(void * arg) {
  //Serial.println("MOTION DETECTED!!!");
  if ( millis()-lastMovementDetected > config.PIR_delay ) {
    motionDetected = true;
    lastMovementDetected = millis();
  }
}

/**************************************************************************
 * _http_event_handler
 * - manages HTTP events.
 **************************************************************************/
esp_err_t _http_event_handler(esp_http_client_event_t *evt)
{
  switch (evt->event_id) {
    case HTTP_EVENT_ERROR:
      Serial.println("HTTP_EVENT_ERROR");
      break;
    case HTTP_EVENT_ON_CONNECTED:
//      Serial.println("HTTP_EVENT_ON_CONNECTED");
      break;
    case HTTP_EVENT_HEADER_SENT:
//      Serial.println("HTTP_EVENT_HEADER_SENT");
      break;
    case HTTP_EVENT_ON_HEADER:
//      Serial.println();
//      Serial.printf("HTTP_EVENT_ON_HEADER, key=%s, value=%s", evt->header_key, evt->header_value);
      break;
    case HTTP_EVENT_ON_DATA:
//      Serial.println();
//      Serial.printf("HTTP_EVENT_ON_DATA, len=%d", evt->data_len);
      if (!esp_http_client_is_chunked_response(evt->client)) {
        // Write out data
        printf("%.*s", evt->data_len, (char*)evt->data);
      }
      break;
    case HTTP_EVENT_ON_FINISH:
//      Serial.println("");
      Serial.println("HTTP_EVENT_ON_FINISH");
      break;
    case HTTP_EVENT_DISCONNECTED:
      Serial.println("HTTP_EVENT_DISCONNECTED");
      break;
  }
  return ESP_OK;
}

/**************************************************************************
 * take_send_photo
 * - takes a photo
 * - uploads photo to server
 **************************************************************************/
static esp_err_t take_send_photo()
{
  Serial.println("\t- Taking picture...");
  camera_fb_t * fb = NULL;

  fb = esp_camera_fb_get();
  if (!fb) {
    Serial.println("\t- Camera capture failed!");
    return ESP_FAIL;
  }

  // Photo taken successfully. Now upload to server.
  esp_http_client_handle_t http_client;
  
  esp_http_client_config_t config_client = {0};
  config_client.url = upload_url;
  config_client.event_handler = _http_event_handler;
  config_client.method = HTTP_METHOD_POST;

  http_client = esp_http_client_init(&config_client);

  esp_http_client_set_post_field(http_client, (const char *)fb->buf, fb->len);

  esp_http_client_set_header(http_client, "Content-Type", "image/jpg");

  esp_err_t err = esp_http_client_perform(http_client);
//  if (err == ESP_OK) {
//    Serial.print("esp_http_client_get_status_code: ");
//    Serial.println(esp_http_client_get_status_code(http_client));
//  }

  esp_http_client_cleanup(http_client);

  esp_camera_fb_return(fb);

  return err;
}

/**************************************************************************
 * setup
 * - Set WiFi and MQTT connections
 * - Set up camera
 * - Set up PIR and interrupts
 * - Set up One-Wire Temperature sensor
 **************************************************************************/
void setup()
{
  int res = 0;

  Serial.begin(115200);
  Serial.setDebugOutput(true);

  WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0); // disable brownout detector

  if ( WiFi_init() ) { // Connected to WiFi
    Serial.print("WiFi connected.\n - RSSI: "); Serial.print(WiFi.RSSI()); Serial.print(" Local IP: "); Serial.println(WiFi.localIP() ); 
  } else {
    Serial.println("WiFi connection failed! \nRestarting in 10s...");
    delay(10000);
    ESP.restart();
  }
  mqttClient.setServer(MQTT_server, 1883); 
  mqttClient.setCallback(MQTT_callback);         // local function to call when MQTT msg received
  MQTT_init();
  if ( mqttClient.connected() ) {
    // Report the startup event for monitoring of crashes and restarts.
    reportState();
  }

  // Read general configuration from SPIFFS config file.
  if ( !readConfig() ) {
    Serial.println("Reading config file failed!");
    delay(10000);
    //ESP.restart();
  }

  // Read camera settings from SPIFFS config file.
  if ( !cam_ReadSettings() ) {
    Serial.println("Reading cam settings file failed!");
    //delay(10000);
    //ESP.restart();
  }

  if ( cam_init() != ESP_OK ) {
    // Something went wrong while setting up the camera. Restart and try again.
    delay(20000);
    ESP.restart();
  }

  pinMode(pinFlashLED, OUTPUT);
  digitalWrite(pinFlashLED, flashState);
  pinMode(pinBoardLED, OUTPUT);
  
  // PIR Motion Sensor and interrupts (rising edge).
  res = gpio_isr_handler_add(GPIO_NUM_13, &isrDetectMovement, (void *) 13);  
  if (res != ESP_OK) {
    Serial.printf("PIR - interrupt handler add failed (err=0x%x) \r\n", res); 
  }
  res = gpio_set_intr_type(GPIO_NUM_13, GPIO_INTR_POSEDGE);
  if (res != ESP_OK) {
    Serial.printf("PIR - set interrupt type failed (err=0x%x) \r\n", res);
  }

  // Set up the OneWire bus with the Temperature sensor
  sensorTemp.begin();

  //runWebServer = true;
  cam_StartHTTPServer();

  // Show board detail
  esp_chip_info_t espInfo;
  esp_chip_info(&espInfo);
  Serial.print("\t- Nr of cores: "); Serial.println(espInfo.cores);
  Serial.print("\t- ESP Model: "); Serial.println( espInfo.model );
  Serial.print("\t- Revision: "); Serial.println( espInfo.revision );
  Serial.print("\t- IDF version: "); Serial.println( esp_get_idf_version() );

  // Blink the LED once (note "opposite" notation, LOW = on for onboard LED)
  BlinkLED(2);

  Serial.println("Setup done.");

}

/**************************************************************************
 * loop
 **************************************************************************/
void loop() {
  static long lastTmpReport = 0;
  static long lastStateReport = 0;

  if (motionDetected) {
    // Motion was detected.
    if (config.PIR_enabled) {
      Serial.println("Loop - Motion Detected"); 
      mqttClient.publish(MQTT_PUB_MOTION, "on");
      actionTakePhoto = true;
    }
    motionDetected = false;
  }

  if (actionTakePhoto) {
    //if (config.CAM_enabled && !runWebServer) {
    if (config.CAM_enabled ) {
      // Take a photo and upload
      Serial.println("Loop - Take and upload photo");
      if ( take_send_photo() == ESP_OK ) {
        mqttClient.publish(MQTT_PUB_CAMERA, "photo");
      }
    }
    actionTakePhoto = false;
  } 

  if ( ((millis()-lastTmpReport>config.TempInterval) && (config.TempInterval>1000)) || requestTemperature ) {
    // Get and upload the temperature. (interval 0 = disabled).
    sensorTemp.requestTemperatures();
    float curTemp = sensorTemp.getTempCByIndex(0);
    Serial.print("Temperature: "); Serial.println(curTemp);
    if (curTemp != -127) {
      // This is a valid reading, upload it to the server.
      mqttClient.publish(MQTT_PUB_TEMP, String(curTemp).c_str() );
    }
    requestTemperature = false;
    lastTmpReport = millis();
  }

  // Feedback ESP32 State and/or WiFi parameters (interval 0 = disabled) 
  if ( ((millis()-lastStateReport > config.StateInterval) && (config.StateInterval>1000)) ) {
    if (config.ReportState) reportState();
    if (config.ReportWiFi) reportWiFi();
    lastStateReport = millis();
  }

  delay(100);

  // Keep MQTT connection alive.
  if (WiFi.status() == WL_CONNECTED && !mqttClient.connected()) {
    MQTT_init();
  }
  mqttClient.loop();

}

