#include <PinConfig.h>
#pragma once
#include "cJSON.h"

namespace DigitalOutController
{
    static bool isDEBUG = false;

    static int digitalout_val_1 = 0;
    static int digitalout_val_2 = 0;
    static int digitalout_val_3 = 0;

    /// trigger settings
    static long digitalout_trigger_delay_1_on = 0;
    static long digitalout_trigger_delay_2_on = 0;
    static long digitalout_trigger_delay_3_on = 0;
    static long digitalout_trigger_delay_1_off = 0;
    static long digitalout_trigger_delay_2_off = 0;
    static long digitalout_trigger_delay_3_off = 0;

    static bool is_digital_trigger_1 = false;
    static bool is_digital_trigger_2 = false;
    static bool is_digital_trigger_3 = false;
    
    int act(cJSON* ob);
    cJSON*  get(cJSON*  ob);
    void setup();
    void loop();
};