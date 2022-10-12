#include "../../config.h"
#ifdef IS_DIGITAL
#pragma once
#include "ArduinoJson.h"
#include "../../pinstruct.h"
#include "../wifi/RestApiCallbacks.h"

namespace RestApi
{
    void Digital_act();
    void Digital_get();
    void Digital_set();
};

class DigitalController
{
private:
    /* data */
public:
    DigitalController(/* args */);
    ~DigitalController();

    bool isBusy;
    bool DEBUG = false;
    PINDEF * pins;

    int digital_val_1 = 0;
    int digital_val_2 = 0;
    int digital_val_3 = 0;

    void act(DynamicJsonDocument * jsonDocument);
    void set(DynamicJsonDocument * jsonDocument);
    void get(DynamicJsonDocument * jsonDocument);
    void setup(PINDEF * pins);
};
extern DigitalController digital;
#endif