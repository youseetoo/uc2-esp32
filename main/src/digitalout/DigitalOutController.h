#include <PinConfig.h>
#pragma once
#include "cJSON.h"

namespace DigitalOutController
{
    // T1.3: per-TU namespace statics — not every includer uses every member.
    [[maybe_unused]] static bool isDEBUG = false;

    [[maybe_unused]] static int digitalout_val_1 = 0;
    [[maybe_unused]] static int digitalout_val_2 = 0;
    [[maybe_unused]] static int digitalout_val_3 = 0;

    /// trigger settings
    [[maybe_unused]] static long digitalout_trigger_delay_1_on = 0;
    [[maybe_unused]] static long digitalout_trigger_delay_2_on = 0;
    [[maybe_unused]] static long digitalout_trigger_delay_3_on = 0;
    [[maybe_unused]] static long digitalout_trigger_delay_1_off = 0;
    [[maybe_unused]] static long digitalout_trigger_delay_2_off = 0;
    [[maybe_unused]] static long digitalout_trigger_delay_3_off = 0;

    [[maybe_unused]] static bool is_digital_trigger_1 = false;
    [[maybe_unused]] static bool is_digital_trigger_2 = false;
    [[maybe_unused]] static bool is_digital_trigger_3 = false;
    
    int act(cJSON* ob);
    cJSON*  get(cJSON*  ob);
    void setup();
    void loop();
};