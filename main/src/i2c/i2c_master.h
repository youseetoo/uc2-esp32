#pragma once
#include "../../cJsonTool.h"
#include "../motor/MotorTypes.h"
#include "../laser/LaserController.h"
#include "../home/HomeMotor.h"
#include "../tmc/TMCController.h"
#include "cJsonTool.h"
#include "cJSON.h"

enum I2C_REQUESTS
{
    REQUEST_MOTORSTATE = 0,
    REQUEST_HOMESTATE = 1,
    REQUEST_LASER_DATA = 2,
    REQUEST_TMCDATA = 3
};
namespace i2c_master
{

    // last laser intensity
    static int lastIntensity = 0;
    static bool waitForFirstRunI2CSlave[4] = {false, false, false, false};


    int act(cJSON *doc);
    cJSON *get(cJSON *ob);
    void setup();
    void loop();
    void startStepper(MotorData *data, int axis, bool reduced = false);
    void stopStepper(MotorData *data, int axis);
    void startHome(int i);
    int axis2address(int axis);
    void sendMotorDataToI2CDriver(MotorData motorData, uint8_t axis, bool reduced);
    void sendHomeDataI2C(HomeData homeData, uint8_t axis);
    bool isAddressInI2CDevices(byte addressToCheck);
    void sendLaserDataI2C(LaserData laserData, uint8_t id);
    void sendTMCDataI2C(TMCData tmcData, uint8_t id);
    MotorState pullMotorDataI2CDriver(int axis);
    HomeState pullHomeStateFromI2CDriver(int axis);
    void updateMotorData(int i);    
    long getMotorPosition(int i);
    void setPosition(Stepper s, int pos);
    void setPositionI2CDriver(Stepper s, long pos);

    MotorState getMotorState(int i);
    void parseMotorJsonI2C(cJSON *doc);

    #ifdef DIAL_CONTROLLER
    void pushMotorPosToDial();
    #endif
};