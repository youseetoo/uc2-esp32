#pragma once
#include "../../cJsonTool.h"
#include "../motor/MotorTypes.h"

namespace i2c_master
{
    
    void setup();
    void loop();
    MotorData **getData();
    void startStepper(int i);
};