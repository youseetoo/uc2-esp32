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
#include "../tmc/TMCController.h"
#define CAN_RX_TASK_PRIORITY 5
#define CAN_RX_TASK_STACK 4096
#define CAN_QUEUE_LENGTH 10


typedef struct
{
    uint32_t counter;
    uint32_t counter1;
    uint32_t counter2;
    uint32_t counter3;
    uint32_t counter4;
    uint32_t counter5;
    uint32_t counter6;
    uint32_t counter7;
    uint32_t counter8;
    uint32_t counter9;
    uint32_t counter10;
    uint32_t counter11;

} MessageData;


namespace can_controller
{

    // allocate memory for the generic data pointer 
    static uint8_t *genericDataPtr = nullptr;

    static unsigned long lastSend = 0;
    int act(cJSON *doc);
    cJSON *get(cJSON *ob);
    void setup();
    void loop();

    // general CAN-related functions
    uint32_t axis2id(int axis);
    int receiveCanMessage(uint32_t senderID, uint8_t *data);
    int sendCanMessage(uint32_t receiverID, const uint8_t *data);
    void setCANAddress(uint32_t address);
    uint32_t getCANAddress();
    void dispatchIsoTpData(pdu_t&);
    
    // motor functions
    int sendMotorDataToCANDriver(MotorData motorData, uint8_t axis, bool reduced = false);
    int startStepper(MotorData *data, int axis, bool reduced);
    void stopStepper(Stepper s);
    void sendMotorStateToMaster();	
    bool isMotorRunning(int axis);

    // home functions
    void sendHomeDataToCANDriver(HomeData homeData, uint8_t axis);
    void sendHomeStateToMaster(HomeState homeState);

    // laser functions
    void sendLaserDataToCANDriver(LaserData laserData);

    // TMC 
    void sendTMCDataToCANDriver(TMCData tmcData, int axis);



} // namespace can_controller