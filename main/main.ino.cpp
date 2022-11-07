# 1 "/var/folders/4w/k4yhf14j7xsbp2jd85yk555r0000gn/T/tmpgo1fgl4g"
#include <Arduino.h>
# 1 "/Users/bene/Dropbox/Dokumente/Promotion/PROJECTS/uc2-ESP/main/main.ino"
#include "config.h"
#include "ArduinoJson.h"
#include "esp_log.h"

#include "src/state/State.h"

#if defined IS_PS4 || defined IS_PS3
#include "src/gamepads/ps_3_4_controller.h"
#endif
#include "src/wifi/WifiController.h"
#include "src/config/ConfigController.h"
#include "src/serial/SerialProcess.h"
#include "src/bt/BtController.h"
#include "ModuleController.h"


#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"
void setup();
void loop();
#line 21 "/Users/bene/Dropbox/Dokumente/Promotion/PROJECTS/uc2-ESP/main/main.ino"
void setup()
{

 Serial.begin(BAUDRATE);
 delay(500);
 log_i("Start setup");
 WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);


 state.startMillis = millis();
 log_i("wifi.createJsonDoc()");
 WifiController::createJsonDoc();

 log_i("state.setup");
 state.setup();
 state.printInfo();

 log_i("Config::setup");
 Config::setup();

 WifiController::getJDoc()->clear();


 log_i("wifi.setup");
 WifiController::setup();

 moduleController.setup();

 BtController::setup();

#if defined IS_PS4 || defined IS_PS3
#ifdef DEBUG_GAMEPAD
 ps_c.DEBUG = true;
#endif

#endif
 WifiController::begin();
 log_i("End setup");



 Config::checkifBootWentThrough();
}

void loop()
{

 state.currentMillis = millis();
 serial.loop();

 moduleController.loop();

#if defined IS_PS4 || defined IS_PS3
 ps_c.control();
#endif
}