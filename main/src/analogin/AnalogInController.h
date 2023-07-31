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
    int act(cJSON* jsonDocument) override;
    cJSON* get(cJSON* jsonDocument) override;
    void loop() override;
};

