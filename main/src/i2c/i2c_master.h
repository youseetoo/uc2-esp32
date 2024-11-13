#pragma once
#include "../../cJsonTool.h"
#include "../motor/MotorTypes.h"
#include "cJsonTool.h"
#include "cJSON.h"
namespace i2c_master
{

    // last laser intensity
    static int lastIntensity = 0;
    static bool waitForFirstRunI2CSlave[4] = {false, false, false, false};


    int act(cJSON *doc);
    cJSON *get(cJSON *ob);
    void setup();
    void loop();
    MotorData **getData();
    void startStepper(int i);
    int axis2address(int axis);
    void sendMotorDataI2C(MotorData motorData, uint8_t axis);
    bool isAddressInI2CDevices(byte addressToCheck);

    void parseJsonI2C(cJSON *doc);
};