#pragma once
#include "../../cJsonTool.h"
#include "../motor/MotorTypes.h"
#include "../laser/LaserController.h"
#include "../home/HomeMotor.h"
#include "cJsonTool.h"
#include "cJSON.h"
#include <ESP32-TWAI-CAN.hpp>
#ifdef TMC_CONTROLLER
#include "../tmc/TMCController.h"
#endif

enum CAN_REQUESTS
{
    REQUEST_MOTORSTATE = 0,
    REQUEST_HOMESTATE = 1,
    REQUEST_LASER_DATA = 2,
    REQUEST_TMCDATA = 3, 
    REQUEST_OTAUPDATE = 4, 
    REQUEST_REBOOT = 5
};
namespace can_master
{

    // last laser intensity
    static int lastIntensity = 0;
    static bool waitForFirstRunCANSlave[4] = {false, false, false, false};


    int act(cJSON *doc);
    cJSON *get(cJSON *ob);
    void setup();
    void loop();
    void startStepper(MotorData *data, int axis, bool reduced = false);
    void stopStepper(MotorData *data, int axis);
    void startHome(int i);
    int axis2address(int axis);
    void sendMotorDataToCANDriver(MotorData motorData, uint8_t axis, bool reduced);
    void sendHomeDataCAN(HomeData homeData, uint8_t axis);
    bool isAddressInCANDevices(byte addressToCheck);
    void sendLaserDataCAN(LaserData laserData, uint8_t id);
    #ifdef TMC_CONTROLLER
    void sendTMCDataCAN(TMCData tmcData, uint8_t id);
    #endif
    MotorState pullMotorDataCANDriver(int axis);
    HomeState pullHomeStateFromCANDriver(int axis);
    void updateMotorData(int i);    
    long getMotorPosition(int i);
    void setPosition(Stepper s, int pos);
    void setPositionCANDriver(Stepper s, long pos);
    void startOTA(int axis=-1);
    void reboot();

    MotorState getMotorState(int i);
    void parseMotorJsonCAN(cJSON *doc);

    #ifdef DIAL_CONTROLLER
    void pushMotorPosToDial();
    #endif
};