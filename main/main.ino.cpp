# 1 "/var/folders/4w/k4yhf14j7xsbp2jd85yk555r0000gn/T/tmpr_xn_6kl"
#include <Arduino.h>
# 1 "/Users/bene/Dropbox/Dokumente/Promotion/PROJECTS/uc2-ESP/main/main.ino"
#include "ArduinoJson.h"
#include "esp_log.h"
#include "src/config/ConfigController.h"
#include "src/serial/SerialProcess.h"
#include "ModuleController.h"


#include "soc/soc.h"
#include "soc/rtc_cntl_reg.h"

#define BAUDRATE 115200
void setup();
void loop();
#line 13 "/Users/bene/Dropbox/Dokumente/Promotion/PROJECTS/uc2-ESP/main/main.ino"
void setup()
{

 Serial.begin(BAUDRATE);
 delay(500);
 Serial.setTimeout(50);


 log_i("Start setup");
 WRITE_PERI_REG(RTC_CNTL_BROWN_OUT_REG, 0);


 log_i("Config::setup");
 Config::setup();


 moduleController.setup();

 log_i("End setup");
}

void loop()
{

 serial.loop();

 moduleController.loop();
}