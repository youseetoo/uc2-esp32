#pragma once
#include "../../Module.h"
#include "../motor/FocusMotor.h"
class DigitalInController : public Module
{
private:
    /* data */
public:
    DigitalInController(/* args */);
    ~DigitalInController();

    bool isBusy;
    bool DEBUG = false;

    int digitalin_val_1 = 0;
    int digitalin_val_2 = 0;
    int digitalin_val_3 = 0;

    
    int act(cJSON* jsonDocument) override;
    cJSON* get(cJSON* jsonDocument) override;
    void setup() override;
    void loop() override;
};