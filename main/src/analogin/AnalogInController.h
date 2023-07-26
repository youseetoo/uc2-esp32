#pragma once
#include "../../Module.h"

class AnalogInController : public Module
{
private:
    /* data */
public:
    AnalogInController(/* args */);
    ~AnalogInController();
    bool DEBUG = false;

    int N_analogin_avg; //no idea if it should be equal to that that one inside PidController.h 

    void setup() override;
    int act(DynamicJsonDocument jsonDocument) override;
    DynamicJsonDocument get(DynamicJsonDocument jsonDocument) override;
    void loop() override;
};

