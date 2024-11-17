#pragma once
#include "../../cJsonTool.h"
#include "../motor/MotorTypes.h"
#include "../laser/LaserController.h"
#include "../home/HomeMotor.h"
#include "../tmc/TMCController.h"
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
    void startStepper(int i, bool reduced);
    void stopStepper(int i);
    void startHome(int i);
    int axis2address(int axis);
    void sendMotorDataToI2CDriver(MotorData motorData, uint8_t axis, bool reduced);
    void sendHomeDataI2C(HomeData homeData, uint8_t axis);
    bool isAddressInI2CDevices(byte addressToCheck);
    void sendLaserDataI2C(LaserData laserData, uint8_t id);
    void sendTMCDataI2C(TMCData tmcData, uint8_t id);
    MotorState pullMotorDataI2C(int axis);

    void parseHomeJsonI2C(cJSON *doc);
    void parseMotorJsonI2C(cJSON *doc);
    #ifdef DIAL_CONTROLLER
    void pushMotorPosToDial();
    #endif
};