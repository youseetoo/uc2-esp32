#pragma once
// T2.1: xiao_ledring app-local PinConfig (converted from
// main/config/seeed_xiao_esp32s3_ledring/PinConfig.h: Arduino.h -> uc2_hal.h).
// Shadows the shared main/PinConfig.h (laser) for this app's build via -I main/.
// Per-app PinConfig rework lands in T2.11.
#include "uc2_hal.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

// #ifndef guards: the IDF build may already define board/feature flags via
// CMAKE_CXX_FLAGS (-DFOO=1 vs an empty #define FOO would trip -Werror).
#ifndef ESP32S3_MODEL_XIAO
#define ESP32S3_MODEL_XIAO
#endif
#ifndef LASER_CONTROLLER
#define LASER_CONTROLLER
#endif
#ifndef DIGITAL_IN_CONTROLLER
#define DIGITAL_IN_CONTROLLER
#endif
#ifndef I2C_SLAVE_LASER
#define I2C_SLAVE_LASER
#endif
#ifndef MESSAGE_CONTROLLER
#define MESSAGE_CONTROLLER
#endif

struct UC2_3_Xiao_Slave_Laser : PinConfig
{
    const char *pindefName = "UC2_3_I2CSlaveLaser";
    const unsigned long BAUDRATE = 115200;

    // Laser control pins (PWM for lasers)
    int8_t LASER_1 = GPIO_NUM_2; // D1 (signal_1, Laser 0)
    int8_t LASER_2 = GPIO_NUM_3; // D2 (signal_2, Laser 1)
    int8_t LASER_3 = GPIO_NUM_5; // D4 (signal_3, Laser 2)
    int8_t LASER_4 = GPIO_NUM_6; // D5 (signal_4, Laser 3)

    // Interlock status
    int8_t digita_in_1 = GPIO_NUM_8; // D9 (LO when interlock has power)
    int8_t digita_in_2 = GPIO_NUM_1; // D0 (LO when interlock tripped, enable pullup)

    // CAN communication
    int8_t CAN_TX = GPIO_NUM_4; // D3 (CAN-SEND)
    int8_t CAN_RX = GPIO_NUM_9; // D10 (CAN-RECV)
    uint32_t CAN_ID_CURRENT = CAN_ID_LASER_0; // Broadcasting address for laser PWM control

    // I2C Configuration (Disabled in this setup)
    int8_t I2C_SCL = -1; // Disabled
    int8_t I2C_SDA = -1; // Disabled

    // Unused Pins (Available on J1101 for future expansion)
    int8_t UNUSED_1 = GPIO_NUM_43; // D6
    int8_t UNUSED_2 = GPIO_NUM_44; // D7
    int8_t UNUSED_3 = GPIO_NUM_7;  // D8
};

const UC2_3_Xiao_Slave_Laser pinConfig;
