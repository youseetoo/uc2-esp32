#include <PinConfig.h>
#ifdef DAC_CONTROLLER
#pragma once
#include "cJSON.h"
#include "DAC_Module.h"


namespace DacController
{
    static DAC_Module *dacm;

    static bool DEBUG = false;

    // DAC-specific parameters
    static dac_channel_t dac_channel = DAC_CHANNEL_1;

    static uint32_t clk_div = 0;
    static uint32_t scale = 0;
    static uint32_t invert = 2;
    static uint32_t phase = 0;

    static uint32_t frequency = 1000;

    static bool dac_is_running = false;

    void setup();

    int act(cJSON* jsonDocument);
    void drive_galvo(void *parameter);
};
#endif
