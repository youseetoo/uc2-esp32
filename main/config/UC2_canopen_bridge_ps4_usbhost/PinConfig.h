// UC2_canopen_bridge_ps4_usbhost — DS4-over-USB-OTG → CANopen bridge node.
//
// Runs on a dedicated Seeed XIAO ESP32-S3. Same CAN pinout as the motor
// slave (CAN_TX=GPIO3, CAN_RX=GPIO2). USB-OTG PHY on the S3 is fixed at
// GPIO19/20 — no pin definition needed here.
//
// NODE_ROLE=2 + ROUTE_*=1 (REMOTE) makes RoutingTable produce REMOTE
// routes for every actuator class, which JoystickRouter then dispatches
// via expedited SDO writes. No MOTOR_/LASER_/LED_CONTROLLER are defined
// here on purpose: this node only *originates* commands; the slaves
// handle them.
#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"

#undef PSXCONTROLLER

#define ESP32S3_MODEL_XIAO
#define MESSAGE_CONTROLLER
#define CAN_BUS_ENABLED
#define CAN_CONTROLLER_CANOPEN
#define JOYSTICK_USBHOST_PROVIDER
// JOYSTICK_USBHOST_PROVIDER comes from build_flags in platformio.ini.

struct UC2_canopen_bridge_ps4_usbhost : PinConfig
{
    bool DEBUG_CAN_ISO_TP = 0;

    const char* pindefName = "UC2_canopen_bridge_ps4_usbhost";
    const unsigned long BAUDRATE = 921600;

    // CAN — same pinout as the motor slave (XIAO ESP32-S3).
    int8_t CAN_TX = GPIO_NUM_13;//GPIO_NUM_3;
    int8_t CAN_RX = GPIO_NUM_14;//GPIO_NUM_2;
    // Pick a node-id distinct from the existing motor (10..) / laser (20..)
    // / LED (30) ranges. The bridge produces traffic, so a low id helps it
    // win arbitration on contended frames.
    uint32_t CAN_ID_CURRENT = 5;

    // Routing overrides — all REMOTE so DeviceRouter-style dispatch (now
    // done locally inside JoystickRouter) emits SDOs at the slaves'
    // default node-ids inherited from PinConfigDefault.h.
    int8_t ROUTE_MOTOR[4] = {1, 1, 1, 1};  // A, X, Y, Z all REMOTE
    int8_t ROUTE_HOME[4]  = {2, 2, 2, 2};  // homing not driven from bridge
    int8_t ROUTE_TMC[4]   = {2, 2, 2, 2};
    int8_t ROUTE_LASER[4] = {1, 2, 2, 2};  // laser ch 0 REMOTE, rest OFF
    int8_t ROUTE_LED      = 1;             // REMOTE
    int8_t ROUTE_GALVO    = 2;             // OFF

    // No actuator pins on this node — everything is disabled.
    int8_t MOTOR_X_STEP = disabled;
    int8_t MOTOR_X_DIR  = disabled;
    int8_t MOTOR_ENABLE = disabled;
    int8_t I2C_SCL      = disabled;
    int8_t I2C_SDA      = disabled;
    int8_t I2C_SCL_ext  = disabled;
    int8_t I2C_SDA_ext  = disabled;

    const uint16_t serialTimeout = 100;
};

const UC2_canopen_bridge_ps4_usbhost pinConfig;
