#pragma once
#include "../../cJsonTool.h"
#include "../motor/MotorTypes.h"
#include "cJsonTool.h"
#include "cJSON.h"
namespace i2c_master
{
    int act(cJSON *doc);
    cJSON *get(cJSON *ob);
    void setup();
    void loop();
    MotorData **getData();
    void startStepper(int i);
};