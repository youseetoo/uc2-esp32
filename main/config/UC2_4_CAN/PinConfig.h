//This is for the standalone aka mAIkroscope::mainboard Rev. G, dated 2025-08-15.
/*
New in this Hardware Rev: 
3 flicker-less LED drivers with field-settable current, 
CAN transceiver with FRAME-backbone-compatible 12V+XH port, 
XH 6-pin port to connect directly to LASERBOX, 
XH and SH ports with pinout identical to off-the-shelf endstop boards
*/

#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"

// only for linting
// #define CORE_DEBUG_LEVEL=5
#define LASER_CONTROLLER=1
#define DIGITAL_IN_CONTROLLER=1
#define MESSAGE_CONTROLLER=1
//#define LINEAR_ENCODER_CONTROLLER=1
//#define CAN_BUS_ENABLED=1
//#define DIAL_CONTROLLER=1
#define MOTOR_CONTROLLER=1
#define HOME_MOTOR=1
#define BTHID=1 
#define BLUETOOTH=1	
#define TMC_CONTROLLER=1
#define OBJECTIVE_CONTROLLER=1
#define USE_FASTACCEL=1
struct UC2_4 : PinConfig
{
     /*
     This is the newest electronics where direction/enable are on a seperate port extender
     */
  
     const char * pindefName = "UC2_4";
     const unsigned long BAUDRATE = 115200;

     int8_t MOTOR_A_STEP = GPIO_NUM_15;
     int8_t MOTOR_X_STEP = GPIO_NUM_16;
     int8_t MOTOR_Y_STEP = GPIO_NUM_14;
     int8_t MOTOR_Z_STEP = GPIO_NUM_0;
     
     /*
     //FIXME: No encoder jacks on the hw anymore
     bool ENC_A_encoderDirection = true;  // true = count up, false = count down -> invert polarity
     bool ENC_X_encoderDirection = true; 
     bool ENC_Y_encoderDirection = true; 
     bool ENC_Z_encoderDirection = true; 
     bool ENC_A_motorDirection = true;  // true = count up, false = count down -> invert polarity
     bool ENC_X_motorDirection = true;
     bool ENC_Y_motorDirection = true;
     bool ENC_Z_motorDirection = true;
     */

     bool MOTOR_ENABLE_INVERTED = true;
     bool MOTOR_AUTOENABLE = true;
     int8_t AccelStepperMotorType = 1;

     //The PWM channels 1-3 go to headers, LED drivers, LASERBOX jack.
     //LASER_4 is exclusively on the LASERBOX jack.
     int8_t LASER_1 = GPIO_NUM_12;
     int8_t LASER_2 = GPIO_NUM_4;
     int8_t LASER_3 = GPIO_NUM_2;
     int8_t LASER_4 = GPIO_NUM_17;

     //neopixels. LED_1
     int8_t LED_PIN = GPIO_NUM_13;
     int8_t LED_COUNT = 64;

     // FIXME: Is this redudant?!
     int8_t DIGITAL_IN_1 = disabled;
     int8_t DIGITAL_IN_2 = disabled;
     int8_t DIGITAL_IN_3 = disabled;
     
     //DAC pins go out to pinheaders DAC_1 and DAC_2
     int8_t dac_fake_1 = GPIO_NUM_25; //GPIO_NUM_25;
     int8_t dac_fake_2 = GPIO_NUM_26; //GPIO_NUM_26; 


     // const char * PSX_MAC = "1a:2b:3c:01:01:04";
     // int8_t PSX_CONTROLLER_TYPE = 2; // 1: PS3, 2: PS4

     int8_t JOYSTICK_SPEED_MULTIPLIER = 5;
     int8_t JOYSTICK_MAX_ILLU = 100;
     int8_t JOYSTICK_SPEED_MULTIPLIER_Z = 10;
     
     /*
     FIXME: Not featured in this revision anymore
     // for caliper
     int8_t ENC_X_A = GPIO_NUM_32;
     int8_t ENC_Y_A = GPIO_NUM_34;
     int8_t ENC_Z_A = GPIO_NUM_36;
     int8_t ENC_X_B = GPIO_NUM_33;
     int8_t ENC_Y_B = GPIO_NUM_35;
     int8_t ENC_Z_B = GPIO_NUM_17;
     */

     // I2c
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
     /*
     FIXME: 
     I/O expander, nothing new in this Revision.
     The notation/labels are weird. Not sure if they are correct.
     Step_Enable for all motors on P00
     X_Dir: P01
     Y_Dir: P02
     Z_Dir: P03
     A_Dir: P04
     X_LIMIT: P05
     Y_LIMIT: P06
     Z_LIMIT: P07
     Just broken out to 2.54mm pitch holes: P10-P17
     
     Also connected: 
     int8_t IOEXP_INTERRUPT = GPIO_NUM_27;
     */


     // SPI
     int8_t SPI_MOSI = GPIO_NUM_23; // PICO
     int8_t SPI_MISO = GPIO_NUM_19; // POCI
     int8_t SPI_SCK = GPIO_NUM_18;
     int8_t SPI_CS = GPIO_NUM_5;

     /*
     // WIFI - specific to SEEED microscope
     const char *mSSID = "UC2xSeeed-";
     const char *mPWD =  "";
     bool mAP = true; // false;
     const char *mSSIDAP = "UC2";            
     const char *hostname = "youseetoo";    
     */ 

     /*
     FIXME: Maybe this is application-based?
     // Temperature
     int8_t DS28b20_PIN = GPIO_NUM_25;
     int8_t LASER_0 = GPIO_NUM_26;
     */

     // CAN
     int8_t CAN_RX = GPIO_NUM_33;
     int8_t CAN_TX = GPIO_NUM_32;

     uint32_t CAN_ID_CURRENT = CAN_ID_CENTRAL_NODE;

};
const UC2_4 pinConfig;