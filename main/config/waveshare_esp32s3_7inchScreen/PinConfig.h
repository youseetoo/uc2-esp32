#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"

#undef MOTOR_AXIS_COUNT
// redfine
#define MOTOR_AXIS_COUNT 10
#undef PSXCONTROLLER

// #define CORE_DEBUG_LEVEL
#define ESP32S3_MODEL_XIAO 
#define MESSAGE_CONTROLLER
#define CAN_RECEIVE_MOTOR
#define CAN_BUS_ENABLED
#define MOTOR_CONTROLLER
#define USE_FASTACCEL
//#define OTA_ON_STARTUP


struct waveshare_esp32s3_7inchScreen : PinConfig
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
     
     bool DEBUG_CAN_ISO_TP = 1; // 1 = debug CAN communication, 0 = no debug

     const char * pindefName = "seeed_xiao_esp32s3_can_slave_motor";
     const unsigned long BAUDRATE = 115200;

     int8_t MOTOR_X_STEP = 0;  // D9 -> GPIO8
     int8_t MOTOR_X_DIR = 0;   // D8 -> GPIO7
     int8_t MOTOR_ENABLE = 0;  // D10 -> GPIO9
     bool MOTOR_ENABLE_INVERTED = true;
     bool MOTOR_AUTOENABLE = false;
     int8_t AccelStepperMotorType = 1;

     // I2c - as slave
     const char *I2C_NAME = "MOTX";
     int8_t I2C_ADD_SLAVE = I2C_ADD_MOT_X;    // I2C address of the ESP32 if it's a slave ( 0x40;)  
     int8_t I2C_SCL = disabled; // GPIO_NUM_2; // D1 -> GPIO2 
     int8_t I2C_SDA = disabled; // GPIO_NUM_3; // D2 -> GPIO3
     
     // I2C  - as controller 
     int8_t I2C_SCL_ext = disabled; // D5 -> GPIO5
     int8_t I2C_SDA_ext = disabled; // D4 -> GPIO4

     // TMC UART 
     int8_t tmc_SW_RX = disabled;// GPIO_NUM_44; // D7 -> GPIO44
     int8_t tmc_SW_TX = disabled;// GPIO_NUM_43; // D6 -> GPIO43
     int8_t tmc_pin_diag = disabled; // D3 -> GPIO4
     
     int tmc_microsteps = 16;
     int tmc_rms_current = 850;
     int tmc_stall_value = 100;
     int tmc_sgthrs = 100;
     int tmc_semin = 5;
     int tmc_semax = 2;
     int tmc_sedn = 0b01;
     int tmc_tcoolthrs = 0xFFFFF;
     int tmc_blank_time = 24;
     int tmc_toff = 4;

     // CAN
     int8_t CAN_TX = GPIO_NUM_20; // GPIO_NUM_3;  // D2 in (I2C SDA) CAN Motor Board
     int8_t CAN_RX = GPIO_NUM_19; //GPIO_NUM_2; // D1 in (I2C SCL)  CAN Motor Board
     uint32_t CAN_ID_CURRENT = CAN_ID_MOT_X;
     

     // Endstops should be the same for all - depending on the motor
     uint8_t DIGITAL_IN_1 = GPIO_NUM_1; // D0 -> GPIO1 - > TOUCH
     uint32_t objectivePositionX0 = 10000;
     uint32_t objectivePositionX1 = 80000;

     // OTA settings
     const uint32_t OTA_TIME_FROM_STARTUP = 30000; // 30 seconds

     const uint16_t serialTimeout = 100;
     const bool dumpHeap = true;

};
  
const waveshare_esp32s3_7inchScreen pinConfig;
