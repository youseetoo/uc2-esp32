#pragma once
// T2.3: canopen_slave_gpio app-local PinConfig (converted from
// main/config/UC2_canopen_slave_gpio/PinConfig.h: Arduino.h -> uc2_hal.h).
// Shadows the shared main/PinConfig.h (laser) for this app's build via -I main/.
// Per-app PinConfig rework lands in T2.11.
#include "uc2_hal.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

#ifndef ESP32S3_MODEL_XIAO
#define ESP32S3_MODEL_XIAO
#endif

// ── Controller modules enabled on this slave ───────────────────────────────
#ifndef DIGITAL_IN_CONTROLLER
#define DIGITAL_IN_CONTROLLER
#endif
#ifndef DIGITAL_OUT_CONTROLLER
#define DIGITAL_OUT_CONTROLLER
#endif
#ifndef MESSAGE_CONTROLLER
#define MESSAGE_CONTROLLER
#endif
#ifndef CAN_BUS_ENABLED
#define CAN_BUS_ENABLED
#endif
#ifndef CAN_CONTROLLER_CANOPEN
#define CAN_CONTROLLER_CANOPEN
#endif
#ifndef GPIO_CAN_SLAVE_CONTROLLER
#define GPIO_CAN_SLAVE_CONTROLLER  // wires GpioCanSlave::setup/loop into main.cpp
#endif
#ifndef I2C_BRIDGE_CONTROLLER
#define I2C_BRIDGE_CONTROLLER      // generic raw-I2C passthrough (0x2350-0x2354)
#endif

struct UC2_canopen_slave_gpio : PinConfig
{
    /*
     XIAO ESP32S3 pin map for the can-gpio-esp board:

        D0  GPIO_NUM_1   – Remote-controllable output 1   (master writes x2301:01)
        D1  GPIO_NUM_2   – (spare)
        D2  GPIO_NUM_3   – E-stop button input            (active LOW, internal pull-up)
        D3  GPIO_NUM_4   – Remote-controllable output 2   (master writes x2301:02)
        D4  GPIO_NUM_5   – CAN TX
        D5  GPIO_NUM_6   – CAN RX
        D6  GPIO_NUM_43  – (spare) — also UART0 TX (Serial debug)
        D7  GPIO_NUM_44  – (spare) — also UART0 RX
        D8  GPIO_NUM_7   – (spare)
        D9  GPIO_NUM_8   – (spare)
        D10 GPIO_NUM_9   – Collision/resistance sensor ADC (ADC1_CH8)

     Notes:
        * GPIOs 1 and 4 match the user-facing identifiers in the firmware
          (digitaloutid=1 -> DIGITAL_OUT_1; id=2 -> DIGITAL_OUT_2).
        * The collision sensor is read by GpioCanSlave at ~50 Hz with a small
          EWMA filter. The trip threshold is stored in NVS preferences
          (key "gpioCollThr"), seeded from GPIO_COLLISION_THRESHOLD_DEFAULT
          on first boot.
     */

    const char *pindefName = "UC2_canopen_slave_gpio";
    const unsigned long BAUDRATE = 115200;

    // ── CAN bus ──────────────────────────────────────────────────────────
     int8_t CAN_TX =  GPIO_NUM_3;  // D2 in (I2C SDA) CAN Motor Board
     int8_t CAN_RX =  GPIO_NUM_2; // D1 in (I2C SCL)  CAN Motor Board
    uint32_t CAN_ID_CURRENT = CAN_ID_GPIO_0; // default node-id 60

    // ── Remote-controllable digital outputs ──────────────────────────────
    int8_t DIGITAL_OUT_1 = GPIO_NUM_1; // D0 -> remote-GPIO1
    int8_t DIGITAL_OUT_2 = GPIO_NUM_4; // D3 -> remote-GPIO4
    int8_t DIGITAL_OUT_3 = disabled;

    // ── Digital inputs ───────────────────────────────────────────────────
    int8_t DIGITAL_IN_1 =  GPIO_NUM_7; // D8;
    int8_t DIGITAL_IN_2 = disabled;
    int8_t DIGITAL_IN_3 = disabled;

    int8_t GPIO_ESTOP_PIN = disabled; // D2

    // ── Collision sensor (resistance → voltage divider on ADC) ───────────
    int8_t   GPIO_COLLISION_ADC                 = GPIO_NUM_9; // D10
    uint16_t GPIO_COLLISION_THRESHOLD_DEFAULT   = 150;
    uint8_t  GPIO_COLLISION_SENSITIVITY_DEFAULT = 4;
    uint16_t GPIO_ADC_FILTER_ALPHA_X1024        = 128;

    int8_t analogin_PIN_0 = GPIO_NUM_9;

    // ── Routing — everything stays LOCAL on this slave ───────────────────
    int8_t ROUTE_MOTOR[4]  = {2, 2, 2, 2};
    int8_t ROUTE_HOME[4]   = {2, 2, 2, 2};
    int8_t ROUTE_TMC[4]    = {2, 2, 2, 2};
    int8_t ROUTE_LASER[4]  = {2, 2, 2, 2};
    int8_t ROUTE_LASER_4   = 2;
    int8_t ROUTE_LED       = 2;
    int8_t ROUTE_GALVO     = 2;

    // ── I2C — generic sensor passthrough bus (I2cBridge) ─────────────────
    int8_t I2C_SDA = GPIO_NUM_5; // D4
    int8_t I2C_SCL = GPIO_NUM_6; // D5
};

const UC2_canopen_slave_gpio pinConfig;
