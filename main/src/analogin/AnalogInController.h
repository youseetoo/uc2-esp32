#pragma once
#include "ArduinoJson.h"
#include "../wifi/WifiController.h"
#include "../../Module.h"
#include "AnalogInPins.h"

class AnalogInController : public Module
{
private:
    /* data */
public:
    AnalogInController(/* args */);
    ~AnalogInController();
    bool DEBUG = false;
    AnalogInPins pins;

    int N_analogin_avg; //no idea if it should be equal to that that one inside PidController.h 

    void setup() override;
    int act(DynamicJsonDocument jsonDocument) override;
    int set(DynamicJsonDocument jsonDocument) override;
    DynamicJsonDocument get(DynamicJsonDocument jsonDocument) override;
    void loop() override;
};

