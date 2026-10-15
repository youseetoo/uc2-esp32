#pragma once
// T2.10: canopen_bridge_ps4_usbhost app-local PinConfig (converted from
// main/config/UC2_canopen_bridge_ps4_usbhost/PinConfig.h: Arduino.h -> uc2_hal.h).
// Shadows the shared main/PinConfig.h (laser) for this app's build via -I main/.
// DS4 gamepad over USB-OTG (GPIO19/20, fixed) -> expedited CANopen SDO bridge;
// all actuators REMOTE.
#include "uc2_hal.h"
#include "PinConfigDefault.h"

#undef PSXCONTROLLER

#ifndef ESP32S3_MODEL_XIAO
#define ESP32S3_MODEL_XIAO
#endif

// ── Controller modules enabled on this bridge ───────────────────────────────
// Same five as the PIO env (UC2_canopen_xiao_base + release): the bridge runs
// the CANopen stack (expedited SDO writes, RoutingTable) and serial messaging,
// with the DS4 USB-host provider on top. No actuator controllers.
#ifndef MESSAGE_CONTROLLER
#define MESSAGE_CONTROLLER
#endif
#ifndef CAN_BUS_ENABLED
#define CAN_BUS_ENABLED
#endif
#ifndef CAN_CONTROLLER_CANOPEN
#define CAN_CONTROLLER_CANOPEN
#endif
#ifndef JOYSTICK_USBHOST_PROVIDER
#define JOYSTICK_USBHOST_PROVIDER
#endif

struct UC2_canopen_bridge_ps4_usbhost : PinConfig
{
    /*
    ESP32-S3 XIAO
    I2C: SDA 6, SCL 7
    CAN: RX 14, TX 13
    USB: D- 20, D+ 19
    */
    bool DEBUG_CAN_ISO_TP = 0;

    const char* pindefName = "UC2_canopen_bridge_ps4_usbhost";
    const unsigned long BAUDRATE = 500000;

    // ── CAN — same pinout as the can-gpio-esp board ──────────────────────
    int8_t CAN_TX = GPIO_NUM_13;
    int8_t CAN_RX = GPIO_NUM_14;
    // Node-id 5: the bridge is a "remote" actor; must not collide with the
    // motor (10..19) / laser (20..) / LED (30) node ids on the same bus.
    uint32_t CAN_ID_CURRENT = 5;

    // USB-OTG (DS4 gamepad) — fixed on the S3, no PinConfig field.

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

const UC2_canopen_bridge_ps4_usbhost pinConfig;
