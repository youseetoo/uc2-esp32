#include "../../config.h"
#if defined IS_DAC || defined IS_DAC_FAKE
#pragma once

#include "ArduinoJson.h"
#include "DAC_Module.h"
#include "../wifi/WifiController.h"
#include "DacPins.h"
#include "../../Module.h"

namespace RestApi
{
    void Dac_act();
    void Dac_get();
    void Dac_set();
};

class DacController : public Module
{
private:
#ifdef IS_DAC
    DAC_Module *dacm;
#endif

public:
    DacController();
    ~DacController();
    bool DEBUG = false;
    DacPins pins;

    // DAC-specific parameters
    dac_channel_t dac_channel = DAC_CHANNEL_1;

    uint32_t clk_div = 0;
    uint32_t scale = 0;
    uint32_t invert = 2;
    uint32_t phase = 0;

    uint32_t frequency = 1000;

    boolean dac_is_running = false;

    void setup() override;

    void act() override;
    void set() override;
    void get() override;
    void loop() override;
    static void drive_galvo(void *parameter);
};
#endif
