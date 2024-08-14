#include <PinConfig.h>
#pragma once
#include "cJSON.h"
#include "DAC_Module.h"


namespace DacController
{
    static DAC_Module *dacm;

    static bool DEBUG = false;

    // DAC-specific parameters
    #ifndef ESP32S3_MODEL_XIAO
        static dac_channel_t dac_channel = DAC_CHANNEL_1;
    #endif

    static bool fake_galvo_running = false;
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
