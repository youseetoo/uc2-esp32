#include "../../config.h"
#pragma once
#include "ArduinoJson.h"
#include "../wifi/RestApiCallbacks.h"
#include "DigitalOutPins.h"
#include "../../Module.h"

namespace RestApi
{
    void DigitalOut_act();
    void DigitalOut_get();
    void DigitalOut_set();
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
    DigitalOutPins pins;

    int digitalout_val_1 = 0;
    int digitalout_val_2 = 0;
    int digitalout_val_3 = 0;

    void act(JsonObject  ob) override;
    void set(JsonObject ob) override;
    void get(JsonObject ob) override;
    void setup() override;
    void loop() override;
};