#pragma once

#include "DS18b20Controller.h"
#include "../laser/LaserController.h"


namespace HeatController
{
    /* data */
    long returnControlValue(float controlTarget, float analoginValue, float Kp, float Ki, float Kd);
    

    void setup();
    void loop();
    int act(cJSON * ob);
    cJSON * get(cJSON * ob);
};
