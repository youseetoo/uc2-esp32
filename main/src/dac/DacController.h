#pragma once

#include "DAC_Module.h"
#include "../../Module.h"

class DacController : public Module
{
private:
    DAC_Module *dacm;

public:
    DacController();
    ~DacController();
    bool DEBUG = false;

    // DAC-specific parameters
    dac_channel_t dac_channel = DAC_CHANNEL_1;

    uint32_t clk_div = 0;
    uint32_t scale = 0;
    uint32_t invert = 2;
    uint32_t phase = 0;

    uint32_t frequency = 1000;

    boolean dac_is_running = false;

    void setup() override;

    int act(DynamicJsonDocument jsonDocument) override;
    DynamicJsonDocument get(DynamicJsonDocument jsonDocument) override;
    void loop() override;
    static void drive_galvo(void *parameter);
};
