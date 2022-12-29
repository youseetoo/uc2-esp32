#pragma once
#include "ArduinoJson.h"
#include "../wifi/RestApiCallbacks.h"
#include "../../Module.h"

namespace RestApi
{
    void DigitalOut_act();
    void DigitalOut_get();
};

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

    int act(DynamicJsonDocument  ob) override;
    DynamicJsonDocument get(DynamicJsonDocument ob) override;
    void setup() override;
    void loop() override;
};