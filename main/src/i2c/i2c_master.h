#pragma once
#include "../../cJsonTool.h"
#include "../motor/MotorTypes.h"
#include "../laser/LaserController.h"
#include "../home/HomeMotor.h"
#include "cJsonTool.h"
#include "cJSON.h"
#ifdef TMC_CONTROLLER
#include "../tmc/TMCController.h"
#endif

enum I2C_REQUESTS
{
    REQUEST_MOTORSTATE = 0,
    REQUEST_HOMESTATE = 1,
    REQUEST_LASER_DATA = 2,
    REQUEST_TMCDATA = 3, 
    REQUEST_OTAUPDATE = 4, 
    REQUEST_REBOOT = 5
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
    void startStepper(MotorData *data, int axis, int reduced = false);
    void stopStepper(MotorData *data, int axis);
    int axis2address(int axis);
    void sendMotorDataToI2CDriver(MotorData motorData, uint8_t axis, bool reduced);
    void sendHomeDataI2C(HomeData homeData, uint8_t axis);
    bool isAddressInI2CDevices(byte addressToCheck);
    void sendLaserDataI2C(LaserData laserData, uint8_t id);
    #ifdef TMC_CONTROLLER
    void sendTMCDataI2C(TMCData tmcData, uint8_t id);
    #endif
    MotorState pullMotorDataReducedDriver(int axis);
    HomeState pullHomeStateFromI2CDriver(int axis);
    void updateMotorData(int i);    
    uint32_t getMotorPosition(int i);
    void setPosition(Stepper s, int pos);
    void setPositionI2CDriver(Stepper s, uint32_t pos);
    void startOTA(int axis=-1);
    void reboot();

    MotorState getMotorState(int i);
    void parseMotorJsonI2C(cJSON *doc);

    #ifdef DIAL_CONTROLLER
    void pushMotorPosToDial();
    #endif
};