#pragma once
#include "Arduino.h"
#include "cJsonTool.h"
#include "JsonKeys.h"
#include "Wire.h"


namespace i2c_controller
{
    void setup();
    void loop();
    int act(cJSON * ob);
    cJSON * get(cJSON *  ob);
    void sendJsonString(String jsonString, uint8_t slave_addr);

    void receiveEvent(int numBytes);
};