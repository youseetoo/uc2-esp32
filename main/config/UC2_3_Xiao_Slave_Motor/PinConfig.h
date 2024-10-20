#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER
struct UC2_ESP32S3_XIAO_LEDSERVO : PinConfig
{
     /*
     D0: 1
     D1: 2
     D2: 3
     D3: 4
     D4: 5
     D5: 6
     D6: 43
     D7: 44
     D8: 7
     D9: 8
     D10: 9

     #define STALL_VALUE     100  // StallGuard sensitivity [0..255]
     #define EN_PIN           D10   // PA4_A1_D1 (Pin 2) Enable pin for motor driver
     #define DIR_PIN          D8   // PA02_A0_D0 (Pin 1) Direction pin
     #define STEP_PIN         D9   // PA10_A2_D2 (Pin 3) Step pin
     #define SW_RX            D7   // PB09_D7_RX (Pin 8) UART RX pin for TMC2209
     #define SW_TX            D6   // PB08_A6_TX (Pin 7) UART TX pin for TMC2209
     #define SERIAL_PORT Serial1  // UART Serial port for TMC2209

    This is a test to work with the UC2_3 board which acts as a I2C slave
     */
     
     const char * pindefName = "UC2_3_I2CSlaveMotorX";
     const unsigned long BAUDRATE = 115200;

     int8_t MOTOR_A_STEP = GPIO_NUM_8;  // D9 -> GPIO8
     int8_t MOTOR_A_DIR = GPIO_NUM_7;   // D8 -> GPIO7
     int8_t MOTOR_ENABLE = GPIO_NUM_9;  // D10 -> GPIO9
     bool MOTOR_ENABLE_INVERTED = true;
     bool MOTOR_AUTOENABLE = true;
     int8_t AccelStepperMotorType = 1;

     // I2c
     const char *I2C_NAME = "MOTX";
     I2CControllerType I2C_CONTROLLER_TYPE = I2CControllerType::mMOTOR;
     int8_t I2C_MOTOR_AXIS = 0;   // On the slave we have one motor axis per slave
     int8_t I2C_ADD_SLAVE = I2C_ADD_MOT_X;    // I2C address of the ESP32 if it's a slave ( 0x40;)  
     int8_t I2C_SCL = GPIO_NUM_2; // D1 -> GPIO2 
     int8_t I2C_SDA = GPIO_NUM_3; // D2 -> GPIO3
     int8_t I2C_ADD_TCA = 0x27;
     gpio_num_t I2C_INT = GPIO_NUM_27;

     // I2C configuration (using updated GPIO values)
     int8_t I2C_SCL_ext = GPIO_NUM_6; // D5 -> GPIO6
     int8_t I2C_SDA_ext = GPIO_NUM_5; // D4 -> GPIO5
};
const UC2_ESP32S3_XIAO_LEDSERVO pinConfig;
