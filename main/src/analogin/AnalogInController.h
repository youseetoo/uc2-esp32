#include <PinConfig.h>
#pragma once
#include "cJSON.h"



namespace AnalogInController
{
    static bool isDEBUG = false;

    static int N_analogin_avg; //no idea if it should be equal to that that one inside PidController.h 

    void setup();
    int act(cJSON* jsonDocument);
    cJSON* get(cJSON* jsonDocument);
};

