#pragma once
#include "cJSON.h"

namespace ObjectiveController
{
    struct ObjectiveData
    {
        long x1 = 0;
        long x2 = 0;
        long lastTarget = 0;
        bool isHomed = false;
        uint8_t currentState = 0; // 0 = undefined, 1 = x1, 2 = x2
    };

    static Stepper sObjective = (Stepper)pinConfig.objectiveMotorAxis;

    int act(cJSON *doc);
    cJSON* get(cJSON *doc);
    void setup();
    void loop();
    void setIsHomed(bool isHomed);

}
