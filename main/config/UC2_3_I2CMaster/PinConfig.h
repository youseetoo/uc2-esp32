#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"

/*
//#define WIFI
//#define BLUETOOTH
//#define BTHID
#define MOTOR_CONTROLLER
#define LASER_CONTROLLER
#define LED_CONTROLLER
#define DIGITAL_IN_CONTROLLER
#define USE_TCA9535
#define ENCODER_CONTROLLER
#define LINEAR_ENCODER_CONTROLLER
#define HOME_DRIVE
#define MESSAGE_CONTROLLER
#define USE_I2C
#define DAC_CONTROLLER
#define HEAT_CONTROLLER
#define DS18B20_CONTROLLER
#define USE_I2C
#define USE_I2C_MOTOR
*/
struct UC2_3_I2CMaster : PinConfig
{
     /*
     This is the newest electronics where direction/enable are on a seperate port extender
     */
  
     const char * pindefName = "UC2_3_I2C_Master";
     const unsigned long BAUDRATE = 115200;

     int8_t MOTOR_A_STEP = GPIO_NUM_15;
     int8_t MOTOR_X_STEP = GPIO_NUM_16;
     int8_t MOTOR_Y_STEP = GPIO_NUM_14;
     int8_t MOTOR_Z_STEP = GPIO_NUM_0;
     bool isDualAxisZ = true;
     
     bool ENC_A_encoderDirection = true;  // true = count up, false = count down -> invert polarity
     bool ENC_X_encoderDirection = true; 
     bool ENC_Y_encoderDirection = true; 
     bool ENC_Z_encoderDirection = true; 
     bool ENC_A_motorDirection = true;  // true = count up, false = count down -> invert polarity
     bool ENC_X_motorDirection = true;
     bool ENC_Y_motorDirection = true;
     bool ENC_Z_motorDirection = true;

     bool MOTOR_ENABLE_INVERTED = true;
     bool MOTOR_AUTOENABLE = true;
     int8_t AccelStepperMotorType = 1;

     int8_t LASER_1 = GPIO_NUM_12;
     int8_t LASER_2 = GPIO_NUM_4;
     int8_t LASER_3 = GPIO_NUM_2;

     int8_t LED_PIN = GPIO_NUM_13;
     int8_t LED_COUNT = 64;

     // FIXME: Is this redudant?!
     int8_t PIN_DEF_END_X = disabled;
     int8_t PIN_DEF_END_Y = disabled;
     int8_t PIN_DEF_END_Z = disabled;
     int8_t DIGITAL_IN_1 = PIN_DEF_END_X;
     int8_t DIGITAL_IN_2 = PIN_DEF_END_Y;
     int8_t DIGITAL_IN_3 = PIN_DEF_END_Z;

     int8_t dac_fake_1 = disabled; //GPIO_NUM_25; // RESET-ABORT just toggles between 1 and 0
     int8_t dac_fake_2 = disabled; //GPIO_NUM_26; // Coolant


     // const char * PSX_MAC = "1a:2b:3c:01:01:04";
     // int8_t PSX_CONTROLLER_TYPE = 2; // 1: PS3, 2: PS4

     int8_t JOYSTICK_SPEED_MULTIPLIER = 2;
     int8_t JOYSTICK_MAX_ILLU = 100;
     int8_t JOYSTICK_SPEED_MULTIPLIER_Z = 10;
     
     // for caliper
     int8_t ENC_X_A = GPIO_NUM_32;
     int8_t ENC_Y_A = GPIO_NUM_34;
     int8_t ENC_Z_A = GPIO_NUM_36;
     int8_t ENC_X_B = GPIO_NUM_33;
     int8_t ENC_Y_B = GPIO_NUM_35;
     int8_t ENC_Z_B = GPIO_NUM_17;

     // I2c
     bool IS_I2C_MASTER = true;
     bool IS_I2C_SLAVE = false;
     int8_t I2C_SCL = GPIO_NUM_22;      // This is the poart that connects to all other slaves
     int8_t I2C_SDA = GPIO_NUM_21;

     // Auxilarry I2C devices
     int8_t I2C_ADD_TCA = 0x27; // this is the port extender on the PCB that controls the direction of the motors

     int8_t I2C_ADD_MOT_X = 0x40;
     int8_t I2C_ADD_MOT_Y = 0x41;
     int8_t I2C_ADD_MOT_Z = 0x42;
     int8_t I2C_ADD_MOT_A = 0x43;
     int8_t I2C_ADD_LEX_MAT = 0x50;
     int8_t I2C_ADD_LEX_PWM1 = 0x51;
     int8_t I2C_ADD_LEX_PWM2 = 0x52;
     int8_t I2C_ADD_LEX_PWM3 = 0x53;
     int8_t I2C_ADD_LEX_PWM4 = 0x54;

     int8_t IC2_ADD_MASTER = 0x10; // I2C address of the ESP32 if it's a master
     int8_t I2C_ADD_SLAVE = 0x40;    // I2C address of the ESP32 if it's a slave
     int8_t I2C_ADD_REMOTE_DEVICE = 0x40; //  #define SLAVE_ADDRESS 0x40 // I2C address of the ESP32


     // SPI
     int8_t SPI_MOSI = GPIO_NUM_17; // GPIO_NUM_23;
     int8_t SPI_MISO = GPIO_NUM_19;
     int8_t SPI_SCK = GPIO_NUM_18;
     int8_t SPI_CS = GPIO_NUM_5;

     // WIFI - specific to SEEED microscope
     const char *mSSID = "UC2xSeeed-";
     const char *mPWD =  "";
     bool mAP = true; // false;
     const char *mSSIDAP = "UC2";            
     const char *hostname = "youseetoo";     

     // Temperature
     int8_t DS28b20_PIN = GPIO_NUM_25;
     int8_t LASER_0 = GPIO_NUM_26;
};
const UC2_3_I2CMaster pinConfig;