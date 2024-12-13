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
#include <ESP32-TWAI-CAN.hpp>
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/queue.h"

#define CAN_RX_TASK_PRIORITY 5
#define CAN_RX_TASK_STACK 4096
#define CAN_QUEUE_LENGTH 10

#define NODE_ID_MASK 0xF0      // Upper 4 bits for Node ID
#define MESSAGE_TYPE_MASK 0x0F // Lower 4 bits for Message Type

// struct for CAN messages
struct CAN_MESSAGE_TYPE
{
    uint8_t MOTOR_ACT = 0;
    uint8_t MOTOR_GET = 1;
    uint8_t HOME_SET = 2;
    uint8_t HOME_GET = 3;
    uint8_t TMC_SET = 4;
    uint8_t TMC_GET = 5;
    uint8_t MOTOR_STATE = 6;
};

#define GET_NODE_ID(id)       ((id & NODE_ID_MASK) >> 4)
#define GET_MESSAGE_TYPE(id)  (id & MESSAGE_TYPE_MASK)
#define CREATE_CAN_ID(node, type) ((node << 4) | type)

struct MultiFrameBuffer
{
    uint8_t buffer[256];
    size_t totalSize;
    size_t receivedSize;
    uint8_t currentFrame;
    bool complete;
};

struct CANMessage
{
    uint8_t nodeID;
    uint8_t messageType;
    uint8_t data[256];
    size_t dataSize;
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
    MotorState pullMotorDataCANDriver(int axis);
    HomeState pullHomeStateFromCANDriver(int axis);
    void updateMotorData(int i);
    long getMotorPosition(int i);
    void setPosition(Stepper s, int pos);
    void setPositionCANDriver(Stepper s, long pos);

    MotorState getMotorState(int i);
    void parseMotorJsonCAN(cJSON *doc);
    void CANListenerTask(void *param);
    void processCANMessage(const CANMessage &message);
    bool sendSegmentedDataCAN(uint16_t msgID, uint8_t messageType, void *data, size_t dataSize);
    QueueHandle_t messageQueue;
    MultiFrameBuffer multiFrameBuffers[16];
    void sendMotorStateToMaster();

} // namespace can_master