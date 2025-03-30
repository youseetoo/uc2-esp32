#pragma once
#include "../../cJsonTool.h"
#include "../laser/LaserController.h"
#include "cJsonTool.h"
#include "cJSON.h"
#include <ESP32-TWAI-CAN.hpp>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#include "../can/can_messagetype.h"
#include "../can/iso-tp-twai/CanIsoTp.hpp"
#include "../laser/LaserController.h"
#include "../home/HomeMotor.h"
#include "../motor/FocusMotor.h"
#include "../motor/MotorTypes.h"
#ifdef TMC_CONTROLLER
#include "../tmc/TMCController.h"
#endif
#define CAN_RX_TASK_PRIORITY 5
#define CAN_RX_TASK_STACK 4096
#define CAN_QUEUE_LENGTH 10


typedef struct
{
    uint8_t counter;
    uint8_t counter1;
    uint8_t counter2;
    uint8_t counter3;
    uint8_t counter4;
    uint8_t counter5;
    uint8_t counter6;
    uint8_t counter7;
    uint8_t counter8;
    uint8_t counter9;
    uint8_t counter10;
    uint8_t counter11;

} MessageData;


namespace can_controller
{
    static unsigned long lastSend = 0;
    int act(cJSON *doc);
    cJSON *get(cJSON *ob);
    void setup();

    // general CAN-related functions
    uint8_t axis2id(int axis);
    int receiveCanMessage(uint8_t senderID);
    int sendCanMessage(uint8_t receiverID, const uint8_t *data, size_t size);
    //int sendCanMessage(uint8_t receiverID, const uint8_t *data);
    void setCANAddress(uint8_t address);
    uint8_t getCANAddress();
    void dispatchIsoTpData(pdu_t&);
    
    // motor functions
    int sendMotorDataToCANDriver(MotorData motorData, uint8_t axis, bool reduced = false);
    int startStepper(MotorData *data, int axis, bool reduced);
    void stopStepper(Stepper s);
    void sendMotorStateToMaster();	
    bool isMotorRunning(int axis);
    int sendMotorSpeedToCanDriver(uint8_t axis, int32_t newSpeed);
    int sendMotorSingleValue(uint8_t axis, uint16_t offset, int32_t newVal);

    // home functions
    void sendHomeDataToCANDriver(HomeData homeData, uint8_t axis);
    void sendHomeStateToMaster(HomeState homeState);
    // axis homed array stores the homed state of each axis
    static bool axisHomed[4] = {false, false, false, false};

    // laser functions
    void sendLaserDataToCANDriver(LaserData laserData);

    // TMC 
    #ifdef TMC_CONTROLLER
    void sendTMCDataToCANDriver(TMCData tmcData, int axis);
    #endif




} // namespace can_controller