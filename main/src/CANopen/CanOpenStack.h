/**
 * CanOpenStack.h
 *
 * UC2 CANopen-Lite stack — init, teardown, and frame dispatch.
 *
 * This replaces the old CanController for CANopen-based environments.
 * It manages the TWAI driver and a FreeRTOS task that dispatches incoming
 * frames to the appropriate sub-system (PDOEngine, SDOHandler, NMTManager,
 * HeartbeatManager).
 *
 * Usage (master):
 *   CanOpenStack::init(tx_gpio, rx_gpio, ownNodeId, UC2_CAN_500K);
 *   CanOpenStack::sendMotorMove(remoteNodeId, axis, targetPos, speed, cmd, qid);
 *   CanOpenStack::sendLaserSet(remoteNodeId, channel, pwm, qid);
 *   CanOpenStack::loop();  // call from Arduino loop()
 *
 * Usage (slave):
 *   CanOpenStack::init(tx_gpio, rx_gpio, ownNodeId, UC2_CAN_500K);
 *   CanOpenStack::setSdoODCallbacks(readCb, writeCb, ctx);
 *   CanOpenStack::setRPDOCallback(rpdoCb);
 *   CanOpenStack::loop();  // calls HeartbeatManager::tick() etc.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "driver/gpio.h"
#include "driver/twai.h"
#include "ObjectDictionary.h"
#include "SDOHandler.h"
#include "NMTManager.h"

// Supported CAN bitrates
enum UC2_CAN_Bitrate {
    UC2_CAN_250K,
    UC2_CAN_500K,
    UC2_CAN_1M,
};

// Callback type for incoming RPDOs on the slave (dispatched in the CAN task)
// The callback receives all three possible RPDO types; check which is non-null.
typedef void (*UC2_RPDOCallback)(const UC2_RPDO1_MotorPos*,
                                 const UC2_RPDO2_Control*,
                                 const UC2_RPDO3_LEDExtra*);

// Callback type for incoming TPDOs on the master
typedef void (*UC2_TPDOCallback)(uint8_t nodeId,
                                 const UC2_TPDO1_MotorStatus*,
                                 const UC2_TPDO2_NodeStatus*,
                                 const UC2_TPDO3_SensorData*);

namespace CanOpenStack {

// ============================================================================
// Initialization / teardown
// ============================================================================

// Initialize TWAI driver and start background CAN task.
// Must be called once in setup() before any other function.
bool init(gpio_num_t txGpio, gpio_num_t rxGpio,
          uint8_t ownNodeId,
          UC2_CAN_Bitrate bitrate = UC2_CAN_500K,
          uint16_t heartbeatPeriodMs = 1000u);

// Gracefully stop the CAN stack (uninstall TWAI driver)
void deinit();

// Call from Arduino loop() — ticks HeartbeatManager and NMTManager::update()
void loop();

// ============================================================================
// Configuration
// ============================================================================

// Register SDO server callbacks (slave side)
void setSdoODCallbacks(SDOHandler::OD_ReadCb readCb,
                       SDOHandler::OD_WriteCb writeCb,
                       void* ctx);

// Register callback for incoming RPDO frames (slave uses this)
void setRPDOCallback(UC2_RPDOCallback cb);

// Register callback for incoming TPDO frames (master uses this)
void setTPDOCallback(UC2_TPDOCallback cb);

// Register callback for NMT node state changes (master uses this)
void setNodeStatusCallback(NMTManager::NodeStatusCallback cb);

// ============================================================================
// Master-side transmit helpers
// ============================================================================

// Send motor move command to a slave via RPDO1 + RPDO2
// Sends the position first, then the trigger control frame.
bool sendMotorMove(uint8_t nodeId, uint8_t axis,
                   int32_t targetPos, int32_t speed, uint8_t cmd, int16_t qid);

// Send motor stop to a slave via RPDO2
bool sendMotorStop(uint8_t nodeId, uint8_t axis, int16_t qid);

// Send laser set to a slave via RPDO2
bool sendLaserSet(uint8_t nodeId, uint8_t channel, uint16_t pwm, int16_t qid);

// Send LED command to a slave via RPDO2 + RPDO3
bool sendLEDSet(uint8_t nodeId, uint8_t mode,
                uint8_t r, uint8_t g, uint8_t b, uint16_t ledIndex, int16_t qid);

// ============================================================================
// Slave-side transmit helpers
// ============================================================================

// Send motor status TPDO1 (call after every position change or move completion)
bool publishMotorStatus(uint8_t axis, int32_t actualPos,
                        bool isRunning, int16_t qid);

// Send node status TPDO2 (call periodically, e.g. every second)
bool publishNodeStatus(uint8_t caps, uint8_t errReg,
                       int16_t tempX10, int16_t encoderPos);

// Send extended sensor data TPDO3
bool publishSensorData(int32_t encoderPos32, uint16_t adcRaw,
                       uint8_t dinState, uint8_t doutState);

// ============================================================================
// NMT helpers (master)
// ============================================================================

void startAll();
void stopAll();
void resetNode(uint8_t nodeId);

// ============================================================================
// Status / diagnostics
// ============================================================================

uint8_t  ownNodeId();
bool     isBusOk();
uint32_t txCount();
uint32_t rxCount();
uint32_t errorCount();

} // namespace CanOpenStack
