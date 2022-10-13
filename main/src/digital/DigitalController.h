#include "../../config.h"
#pragma once
#include "ArduinoJson.h"
#include "../wifi/RestApiCallbacks.h"
#include "DigitalPins.h"
#include "../../Module.h"

namespace RestApi
{
    void Digital_act();
    void Digital_get();
    void Digital_set();
};

class DigitalController : public Module
{
private:
    /* data */
public:
    DigitalController(/* args */);
    ~DigitalController();

    bool isBusy;
    bool DEBUG = false;
    DigitalPins pins;

    int digital_val_1 = 0;
    int digital_val_2 = 0;
    int digital_val_3 = 0;

    void act() override;
    void set() override;
    void get() override;
    void setup() override;
    void loop() override;
};