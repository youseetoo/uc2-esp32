#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

#define CORE_DEBUG_LEVEL
#define ESP32S3_MODEL_XIAO 
#define LASER_CONTROLLER
#define DIGITAL_IN_CONTROLLER
#define MESSAGE_CONTROLLER
#define CAN_SLAVE_MOTOR
#define CAN_CONTROLLER
#define MOTOR_CONTROLLER
#define HOME_MOTOR
#define DIGITAL_IN_CONTROLLER
#define USE_FASTACCEL
#define TMC_CONTROLLER

struct UC2_3_Xiao_Slave_Laser : PinConfig
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
     #define I2C_SCL_ext      D5   // PB07_A5_D5 (Pin 6) I2C SCL pin
     #define I2C_SDA_ext      D4   // PB06_A4_D4 (Pin 5) I2C SDA pin
     #define I2C_SCL_int     D2   // PA14_A3_D3 (Pin 4) I2C SCL pin
     #define I2C_SDA_int     D3   // PA13_A10_D10 (Pin 9) I2C SDA pin

    This is a test to work with the UC2_3 board which acts as a I2C slave
     */
     
     const char * pindefName = "UC2_3_I2CSlaveLaser";
     const unsigned long BAUDRATE = 115200;

     // Laser control pins (using updated GPIO values)
     int8_t LASER_1 = GPIO_NUM_3; // D2
     int8_t LASER_2 = GPIO_NUM_4; // D3 => Motor 1/1
     
     int8_t DIGITAL_IN_1 = GPIO_NUM_1; // D0 // Touch 1 
     int8_t DIGITAL_IN_2 = GPIO_NUM_3; // D1 // Touch 2
     
     // I2C configuration (using updated GPIO values)
     int8_t I2C_SCL = GPIO_NUM_6; // D5 -> GPIO6
     int8_t I2C_SDA = GPIO_NUM_5; // D4 -> GPIO5

     int8_t CAN_TX = 5;
     int8_t CAN_RX = 44;

     // CAN
     int8_t CAN_TX = GPIO_NUM_7;  // D8 -> GPIO7
     int8_t CAN_RX = 44; // D7 -> GPIO44

     uint32_t CAN_ID_CURRENT = CAN_ID_LASER_0; // TODO: This is a "broadcasting" address, where multiple lasers (PWM) are connected to one device


};
  
const UC2_3_Xiao_Slave_Laser pinConfig;
