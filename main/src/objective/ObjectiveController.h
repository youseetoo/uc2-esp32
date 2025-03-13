#pragma once
#include "cJSON.h"

namespace ObjectiveController
{
    struct ObjectiveData
    {
        int32_t x1 = 0;
        int32_t x2 = 0;
        int32_t z1 = 0;
        int32_t z2 = 0;
        int32_t lastTarget = 0;
        bool isHomed = false;
        uint8_t currentState = 0; // 0 = undefined, 1 = x1, 2 = x2
    };

    static Stepper sObjective = (Stepper)pinConfig.objectiveMotorAxis;
    static Stepper sFocus = (Stepper)pinConfig.focusMotorAxis;

    int act(cJSON *doc);
    cJSON* get(cJSON *doc);
    void setup();
    void loop();
    void setIsHomed(bool isHomed);

}
