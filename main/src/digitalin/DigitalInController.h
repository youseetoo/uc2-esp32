#include <PinConfig.h>
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
    bool getDigitalVal(int digitalinid);
    void setup();
    void loop();
};