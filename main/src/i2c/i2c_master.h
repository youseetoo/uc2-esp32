#pragma once
#include "../../cJsonTool.h"
#include "../motor/MotorTypes.h"
#include "cJsonTool.h"
#include "cJSON.h"
namespace i2c_master
{

    // last laser intensity
    static int lastIntensity = 0;


    int act(cJSON *doc);
    cJSON *get(cJSON *ob);
    void setup();
    void loop();
    MotorData **getData();
    void startStepper(int i);

    void parseJsonI2C(cJSON *doc);
};