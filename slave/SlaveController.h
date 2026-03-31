/**
 * SlaveController.h
 *
 * UC2 CAN Slave — device state and Object Dictionary adapter.
 *
 * This module holds the runtime state of all devices on the slave node
 * (motor axes, laser channels, LED).  It exposes two callbacks
 * (sdoRead / sdoWrite) that the SDO server forwards OD requests to,
 * and an RPDO handler that processes incoming PDO frames from the master.
 *
 * The application fw (slave/main.cpp) wires everything together:
 *   SlaveController::init(caps, motorCount, laserCount, hasLED);
 *   CanOpenStack::setSdoODCallbacks(SlaveController::sdoRead,
 *                                   SlaveController::sdoWrite, nullptr);
 *   CanOpenStack::setRPDOCallback(SlaveController::onRPDO);
 *   CanOpenStack::init(txPin, rxPin, nodeId);
 *   loop() → SlaveController::loop();   // publish TPDOs on state change
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "CANopen/ObjectDictionary.h"

// Maximum number of motor axes / laser channels on one slave node
#define SLAVE_MAX_MOTORS  4
#define SLAVE_MAX_LASERS  4

namespace SlaveController {

// ============================================================================
// Initialization
// ============================================================================

// Call before CanOpenStack::init().
// caps     — OR of UC2_CAP_* bits describing what this slave provides
// nodeId   — own CAN node-ID (needed to publish TPDOs correctly)
// motors   — number of motor axes (max SLAVE_MAX_MOTORS)
// lasers   — number of laser channels (max SLAVE_MAX_LASERS)
// hasLED   — true if a local LED array is connected
void init(uint8_t caps, uint8_t nodeId,
          uint8_t motors, uint8_t lasers, bool hasLED);

// ============================================================================
// Main-loop work
// ============================================================================

// Call every loop iteration.  Publishes TPDO1 when a motor finishes moving,
// and TPDO2 once per second.
void loop();

// ============================================================================
// Callbacks registered with CanOpenStack
// ============================================================================

// SDO read callback — invoked when master reads a UC2 OD entry from us.
// Returns true + fills buf[0..3] and sets *lenOut on success.
bool sdoRead(uint16_t index, uint8_t subIndex,
             uint8_t* buf, uint8_t* lenOut, void* ctx);

// SDO write callback — invoked when master writes a UC2 OD entry to us.
// data points to 1–4 bytes as sent by the master.
bool sdoWrite(uint16_t index, uint8_t subIndex,
              const uint8_t* data, uint8_t dataLen, void* ctx);

// RPDO callback — invoked for every incoming RPDO frame addressed to us.
void onRPDO(const UC2_RPDO1_MotorPos* rp1,
            const UC2_RPDO2_Control*  rp2,
            const UC2_RPDO3_LEDExtra* rp3);

// ============================================================================
// State accessors (read-only, safe to call from any context)
// ============================================================================

struct MotorState {
    int32_t  targetPos;
    int32_t  speed;
    uint8_t  cmd;            // last UC2_MOTOR_CMD_* received
    int32_t  actualPos;      // updated by ISR / periodic read
    bool     isRunning;
    int16_t  qid;
};

struct LaserState {
    uint16_t pwm;            // 0-1023
};

struct LEDState {
    uint8_t  mode;
    uint8_t  r, g, b;
    uint16_t ledIndex;
};

const MotorState* getMotor(uint8_t axis);
const LaserState* getLaser(uint8_t channel);
const LEDState*   getLED();

uint8_t getCaps();
uint8_t getNodeId();
uint8_t getErrorReg();

} // namespace SlaveController
