#include <PinConfig.h>
#ifdef DIGITAL_IN_CONTROLLER
#pragma once
#include "cJSON.h"


namespace DigitalInController
{
    static bool isBusy;
    static bool DEBUG = false;

    static int digitalin_val_1 = 0;
    static int digitalin_val_2 = 0;
    static int digitalin_val_3 = 0;

    
    int act(cJSON* jsonDocument);
    cJSON* get(cJSON* jsonDocument);
    void setup();
    void loop();
};
#endif