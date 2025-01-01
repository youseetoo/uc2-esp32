#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER
#define DIGITAL_IN_CONTROLLER=1
#define HOME_MOTOR=1
#define MOTOR_CONTROLLER=1
#define I2C_SLAVE_MOTOR=1
#define USE_FASTACCEL=1
#define MESSAGE_CONTROLLER=1
#define TMC_CONTROLLER=1
#define ESP32S3_MODEL_XIAO=1
struct UC2_3_Xiao_Slave : PinConfig
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
     
     const char * pindefName = "UC2_3_I2CSlaveMotorX";
     const unsigned long BAUDRATE = 115200;

     int8_t MOTOR_X_STEP = GPIO_NUM_8;  // D9 -> GPIO8
     int8_t MOTOR_X_DIR = GPIO_NUM_7;   // D8 -> GPIO7
     int8_t MOTOR_ENABLE = GPIO_NUM_9;  // D10 -> GPIO9
     bool MOTOR_ENABLE_INVERTED = true;
     bool MOTOR_AUTOENABLE = false;
     int8_t AccelStepperMotorType = 1;

     // I2c
     const char *I2C_NAME = "MOTX";
     I2CControllerType I2C_CONTROLLER_TYPE = I2CControllerType::mMOTOR;
     int8_t REMOTE_MOTOR_AXIS_ID = 1;   // On the slave we have one motor axis per slave
     int8_t I2C_ADD_SLAVE = I2C_ADD_MOT_X;    // I2C address of the ESP32 if it's a slave ( 0x40;)  
     int8_t I2C_SCL = GPIO_NUM_2; // D1 -> GPIO2 
     int8_t I2C_SDA = GPIO_NUM_3; // D2 -> GPIO3
     
     // I2C configuration (using updated GPIO values)
     int8_t I2C_SCL_ext = GPIO_NUM_6; // D5 -> GPIO6
     int8_t I2C_SDA_ext = GPIO_NUM_5; // D4 -> GPIO5

     // TMC UART 
     int8_t tmc_SW_RX = 44;// GPIO_NUM_44; // D7 -> GPIO44
     int8_t tmc_SW_TX = 43;// GPIO_NUM_43; // D6 -> GPIO43
     int8_t tmc_pin_diag = GPIO_NUM_4; // D3 -> GPIO4
     
     int tmc_microsteps = 16;
     int tmc_rms_current = 500;
     int tmc_stall_value = 100;
     int tmc_sgthrs = 100;
     int tmc_semin = 5;
     int tmc_semax = 2;
     int tmc_sedn = 0b01;
     int tmc_tcoolthrs = 0xFFFFF;
     int tmc_blank_time = 24;
     int tmc_toff = 4;

     // TEmporarily for endstop
     int8_t DIGITAL_IN_1 = GPIO_NUM_1; // D0 -> GPIO1 - > TOUCH
};
  
const UC2_3_Xiao_Slave pinConfig;
