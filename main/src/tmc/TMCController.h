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

struct TMCData
{
    int msteps;
    int rms_current;
    int stall_value;
    int sgthrs;
    int semin;
    int semax;
    int sedn;
    int tcoolthrs;
    int blank_time;
    int toff;
};
namespace TMCController
{
    static Preferences preferences;
    int act(cJSON * ob);
    cJSON * get(cJSON *  ob);
    void setup();
    void setTMCCurrent(int current);
    void loop();
    void callibrateStallguard(int speed);
    void setTMCData(TMCData tmcData);
};

