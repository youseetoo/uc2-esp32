#include <PinConfig.h>
#pragma once
#include "cJSON.h"
#include "Arduino.h"
#include <TMCStepper.h>
#include <Wire.h>
#include "Arduino.h"
#include "../../JsonKeys.h"
#include "cJsonTool.h"
#include "PinConfig.h"
#include "Preferences.h"
#include "../motor/FocusMotor.h"
#include "esp_task_wdt.h"

#define DRIVER_ADDRESS 0b00
#define R_SENSE 0.2f
namespace TMCController
{
    static Preferences preferences;
    void LASER_despeckle(int LASERdespeckle, int LASERid, int LASERperiod);
    int act(cJSON * ob);
    cJSON * get(cJSON *  ob);
    void setup();
    void loop();
    void callibrateStallguard(int speed);

};

