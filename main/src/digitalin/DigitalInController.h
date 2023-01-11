#pragma once
#include "ArduinoJson.h"
#include "../wifi/RestApiCallbacks.h"
#include "../../Module.h"

namespace RestApi
{
    void DigitalIn_act();
    void DigitalIn_get();
};

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

    
    int act(DynamicJsonDocument jsonDocument) override;
    DynamicJsonDocument get(DynamicJsonDocument jsonDocument) override;
    void setup() override;
    void loop() override;
};