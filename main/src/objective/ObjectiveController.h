#pragma once
#include "cJSON.h"

namespace ObjectiveController
{
    struct ObjectiveData
    {
        int32_t x0 = 0;
        int32_t x1 = 0;
        int32_t z0 = 0;
        int32_t z1 = 0;
        int32_t lastTarget = 0;
        bool isHomed = false;
        uint8_t currentState = 2; // 2 = undefined, 0 = x0, 1 = x1
    };

    static Stepper sObjective = (Stepper)pinConfig.objectiveMotorAxis;
    static Stepper sFocus = (Stepper)pinConfig.focusMotorAxis;

    int act(cJSON *doc);
    cJSON* get(cJSON *doc);
    void setup();
    void loop();
    void setIsHomed(bool isHomed);
    void share_changed_event(int pressed);

}
