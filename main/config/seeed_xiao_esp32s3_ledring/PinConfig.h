#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

// ATTENTION: THIS IS ONLY FOR LINTING!
#define CORE_DEBUG_LEVEL
#define ESP32S3_MODEL_XIAO 
#define LASER_CONTROLLER
#define MESSAGE_CONTROLLER
struct UC2_3_Xiao_Slave_Laser : PinConfig
{
    /*
    XIAO Pin to GPIO Mapping:
    D0: GPIO_01
    D1: GPIO_02
    D2: GPIO_03
    D3: GPIO_04
    D4: GPIO_05
    D5: GPIO_06
    D6: GPIO_43 (Unused, available on J1101)
    D7: GPIO_44 (Unused, available on J1101)
    D8: GPIO_07 (Unused, available on J1101)
    D9: GPIO_08 (Interlock power detection)
    D10: GPIO_09 (CAN RX)
    */

    const char *pindefName = "UC2_3_I2CSlaveLaser";
    const unsigned long BAUDRATE = 115200;

    // Laser control pins (PWM for lasers)
    int8_t LASER_1 = GPIO_NUM_2; // D1 (signal_1, Laser 0)
    int8_t LASER_2 = GPIO_NUM_3; // D2 (signal_2, Laser 1)
    int8_t LASER_3 = GPIO_NUM_5; // D4 (signal_3, Laser 2)
    int8_t LASER_4 = GPIO_NUM_6; // D5 (signal_4, Laser 3)

    // Interlock status
    int8_t digita_in_1= GPIO_NUM_8; // D9 (LO when interlock has power) // INTERLOCK_STATUS
    int8_t digita_in_2 = GPIO_NUM_1; // D0 (LO when interlock tripped, enable pullup) // INTERLOCK_LED

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
    int8_t UNUSED_3 = GPIO_NUM_7; // D8
};

const UC2_3_Xiao_Slave_Laser pinConfig;
