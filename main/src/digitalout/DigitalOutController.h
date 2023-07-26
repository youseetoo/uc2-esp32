#pragma once
#include "../../Module.h"


class DigitalOutController : public Module
{
private:
    /* data */
public:
    DigitalOutController(/* args */);
    ~DigitalOutController();

    bool isBusy;
    bool DEBUG = false;

    int digitalout_val_1 = 0;
    int digitalout_val_2 = 0;
    int digitalout_val_3 = 0;

    /// trigger settings
    long digitalout_trigger_delay_1_on = 0;
    long digitalout_trigger_delay_2_on = 0;
    long digitalout_trigger_delay_3_on = 0;
    long digitalout_trigger_delay_1_off = 0;
    long digitalout_trigger_delay_2_off = 0;
    long digitalout_trigger_delay_3_off = 0;

    bool is_digital_trigger_1 = false;
    bool is_digital_trigger_2 = false;
    bool is_digital_trigger_3 = false;
    
    int act(DynamicJsonDocument  ob) override;
    DynamicJsonDocument get(DynamicJsonDocument ob) override;
    void setup() override;
    void loop() override;
};