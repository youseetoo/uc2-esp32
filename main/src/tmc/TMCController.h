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
    uint16_t msteps;
    uint16_t rms_current;
    uint16_t stall_value;
    uint16_t sgthrs;
    uint8_t semin;
    uint8_t semax;
    uint8_t sedn;
    uint32_t tcoolthrs;
    uint8_t blank_time;
    uint8_t toff;
};

namespace TMCController
{
    static Preferences preferences;
    int act(cJSON * ob);
    cJSON * get(cJSON *  ob);
    void setup();
    void setTMCCurrent(uint16_t current);
    uint16_t getTMCCurrent();
    void loop();
    void callibrateStallguard(int speed);
    void applyParamsToDriver(const TMCData &p, bool saveToPrefs);
};

