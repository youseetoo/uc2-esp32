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
#include "../can/OtaTypes.h"
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

#ifdef LED_CONTROLLER
#include "../led/LedController.h"
#endif

#ifdef GALVO_CONTROLLER
#include "../scanner/GalvoController.h"
#endif

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
    extern uint8_t device_can_id;  // Device CAN address, accessible externally
    
    int act(cJSON *doc);
    cJSON *get(cJSON *ob);
    void setup();
    void loop();

    // debug state
    extern bool debugState;
    
    // ISO-TP separation time (ms) - adaptive based on debug mode
    // Debug mode: 30ms (to handle log_i overhead)
    // Production mode: 2-5ms (fast transfer)
    extern uint8_t separationTimeMin;

    // general CAN-related functions
    uint8_t axis2id(int axis);
    int receiveCanMessage(uint8_t senderID);
    int receiveCanMessage(uint8_t *rxIDs, uint8_t numIDs);
    int sendCanMessage(uint8_t receiverID, const uint8_t *data, size_t size);
    int sendIsoTpData(uint8_t receiverID, const uint8_t *data, size_t size);  // ISO-TP transmission
    int sendTypedCanMessage(uint8_t receiverID, CANMessageTypeID msgType, const uint8_t *payload, size_t payloadSize);
    //int sendCanMessage(uint8_t receiverID, const uint8_t *data);
    void setCANAddress(uint8_t address);
    uint8_t getCANAddress();
    void dispatchIsoTpData(pdu_t&);
    // motor functions
    int sendMotorDataToCANDriver(MotorData motorData, uint8_t axis, int reduced = 0);
    MotorSettings extractMotorSettings(const MotorData& motorData);
    int sendMotorSettingsToCANDriver(MotorSettings motorSettings, uint8_t axis);
    void resetMotorSettingsFlag(uint8_t axis); // Reset flag to force settings resend
    void resetAllMotorSettingsFlags(); // Reset all flags
    int startStepper(MotorData *data, int axis, int reduced);
    void stopStepper(Stepper s);
    void sendMotorStateToMaster();	
    bool isMotorRunning(int axis);
    int sendMotorSpeedToCanDriver(uint8_t axis, int32_t newSpeed);
    int sendEncoderBasedMotionToCanDriver(uint8_t axis, bool encoderBasedMotion);
    int sendMotorSingleValue(uint8_t axis, uint16_t offset, int32_t newVal);
    int sendCANRestartByID(uint8_t canID);

    // scan functions
    cJSON* scanCanDevices();
    bool isCANDeviceOnline(uint8_t canId);
    void addCANDeviceToAvailableList(uint8_t canId);
    void removeCANDeviceFromAvailableList(uint8_t canId);

    // home functions
    void sendHomeDataToCANDriver(HomeData homeData, uint8_t axis);
    int sendSoftLimitsToCANDriver(int32_t minPos, int32_t maxPos, bool enabled, uint8_t axis);
    void sendHomeStateToMaster(HomeState homeState);
    void sendStopHomeToCANDriver(uint8_t axis);
    // axis homed array stores the homed state of each axis
    static bool axisHomed[4] = {false, false, false, false};

    // laser functions
    void sendLaserDataToCANDriver(LaserData laserData);

    // galvo functions
    #ifdef GALVO_CONTROLLER
    void sendGalvoDataToCANDriver(GalvoData galvoData);
    void sendGalvoStateToMaster(GalvoData galvoData);
    #endif

    // TMC 
    #ifdef TMC_CONTROLLER
    void sendTMCDataToCANDriver(TMCData tmcData, int axis);
    #endif

    // LED 
    #ifdef LED_CONTROLLER
    int sendLedCommandToCANDriver(LedCommand cmd, uint8_t targetID);
    #endif

    // OTA functions
    int sendOtaStartCommandToSlave(uint8_t slaveID, const char* ssid, const char* password, uint32_t timeout_ms = 300000);
    void handleOtaCommand(OtaWifiCredentials* otaCreds);
    void sendOtaAck(uint8_t status);
    void handleOtaLoop(); // Non-blocking OTA handler to be called in main loop
    
    // CAN-based OTA (firmware over CAN via ISO-TP)
    cJSON* actCanOta(cJSON* doc);
    cJSON* actCanOtaStream(cJSON* doc);
    
} // namespace can_controller