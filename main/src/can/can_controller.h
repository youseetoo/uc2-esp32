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
#include "../can/can_messagetype.h"

#define CAN_RX_TASK_PRIORITY 7
#define CAN_RX_TASK_STACK 4096
#define CAN_QUEUE_LENGTH 10


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
    uint8_t sourceID;
    uint8_t destID;
    uint8_t messageType;
    uint8_t data[256];
    size_t dataSize;
};




namespace can_controller
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
    bool sendSegmentedDataCAN(uint32_t canID, void *data, size_t dataSize);

    static QueueHandle_t messageQueue;
    static MultiFrameBuffer multiFrameBuffers[16];
    void sendMotorStateToMaster();
    int address2axis(int address);
    uint8_t getSelfCANID();
    uint32_t constructExtendedCANID(uint8_t destID, uint8_t messageType, uint8_t flags);
    void decodeExtendedCANID(uint32_t id, uint8_t &sourceID, uint8_t &destID, uint8_t &messageType, uint8_t &flags);

    bool sendHeartbeatCAN();
    static uint32_t canLastHeartbeat = 0;
    static uint16_t canHeartbeatPeriod = 1000;              // Variable to keep track of number of devices found



} // namespace can_controller