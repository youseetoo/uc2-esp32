#pragma once
// T2.8: canopen_bridge_ptz app-local PinConfig (converted from
// main/config/UC2_canopen_bridge_ptz/PinConfig.h: Arduino.h -> uc2_hal.h).
// Shadows the shared main/PinConfig.h (laser) for this app's build via -I main/.
// RS-485 Pelco-D PTZ keyboard (UART1) -> CANopen SDO bridge; all actuators REMOTE.
#include "uc2_hal.h"
#include "PinConfigDefault.h"

#undef PSXCONTROLLER

#ifndef ESP32S3_MODEL_XIAO
#define ESP32S3_MODEL_XIAO
#endif

// ── Controller modules enabled on this bridge ───────────────────────────────
#ifndef MESSAGE_CONTROLLER
#define MESSAGE_CONTROLLER
#endif
#ifndef CAN_BUS_ENABLED
#define CAN_BUS_ENABLED
#endif
#ifndef CAN_CONTROLLER_CANOPEN
#define CAN_CONTROLLER_CANOPEN
#endif
#ifndef PTZ_KEYBOARD_CONTROLLER
#define PTZ_KEYBOARD_CONTROLLER
#endif

struct UC2_canopen_bridge_ptz : PinConfig
{
    bool DEBUG_CAN_ISO_TP = 0;

    const char* pindefName = "UC2_canopen_bridge_ptz";
    const unsigned long BAUDRATE = 115200;

    // ── CAN — same pinout as the can-gpio-esp board ──────────────────────
    int8_t CAN_TX = GPIO_NUM_3;
    int8_t CAN_RX = GPIO_NUM_2;
    // Node-id 61: right after the GPIO slave (60), outside the motor
    // (10..19) / laser (20..) / LED (30) / galvo (40) ranges.
    uint32_t CAN_ID_CURRENT = 61;

    // ── RS-485 keyboard link (UART1) ─────────────────────────────────────
    int8_t  PTZ_RS485_RX = GPIO_NUM_5;  // D4 (SDA) <- adapter RO
    int8_t  PTZ_RS485_TX = GPIO_NUM_6;  // D5 (SCL) -> adapter DI (listen-only, spare)
    int32_t PTZ_BAUDRATE = 9600;         // keyboard menu: 96+ACK
    uint8_t PTZ_CAMERA_ADDRESS = 0;      // 0 = accept any Pelco camera address
    uint8_t PTZ_DEBUG_DEFAULT = 2;       // 0 quiet / 1 frames / 2 + raw hex
    uint32_t PTZ_MOTION_TIMEOUT_MS = 2000;

    // Speed scaling (final speed = (pelcoSpeed/63) * 2000 * multiplier, steps/s).
    int8_t PTZ_SPEED_MULTIPLIER_XY = 50;
    int8_t PTZ_SPEED_MULTIPLIER_Z  = 10; // zoom rocker
    int8_t PTZ_SPEED_MULTIPLIER_A  = 5;  // NEAR/FAR keys -> aux axis

    // ── Routing — all actuators REMOTE (dispatch via SDO) ────────────────
    // Indexed by Stepper enum: A=0, X=1, Y=2, Z=3.
    int8_t ROUTE_MOTOR[4] = {1, 1, 1, 1};
    int8_t ROUTE_HOME[4]  = {2, 2, 2, 2};
    int8_t ROUTE_TMC[4]   = {2, 2, 2, 2};
    int8_t ROUTE_LASER[4] = {2, 2, 2, 2};
    int8_t ROUTE_LED      = 2;
    int8_t ROUTE_GALVO    = 2;

    // No actuator pins on this node.
    int8_t MOTOR_X_STEP = disabled;
    int8_t MOTOR_X_DIR  = disabled;
    int8_t MOTOR_ENABLE = disabled;
    int8_t I2C_SCL      = disabled;
    int8_t I2C_SDA      = disabled;
    int8_t I2C_SCL_ext  = disabled;
    int8_t I2C_SDA_ext  = disabled;

    const uint16_t serialTimeout = 100;
};

const UC2_canopen_bridge_ptz pinConfig;
