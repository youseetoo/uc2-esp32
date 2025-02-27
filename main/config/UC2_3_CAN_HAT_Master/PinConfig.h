#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"

#define CORE_DEBUG_LEVEL=5
#define LASER_CONTROLLER=1
#define DIGITAL_IN_CONTROLLER=1
#define MESSAGE_CONTROLLER=1
#define ENCODER_CONTROLLER=1
#define LINEAR_ENCODER_CONTROLLER=1
#define CAN_CONTROLLER=1
#define DIAL_CONTROLLER=1
#define MOTOR_CONTROLLER=1
#define HOME_MOTOR=1
#define BTHID=1 
#define BLUETOOTH=1	
#define TMC_CONTROLLER=1
#define OBJECTIVE_CONTROLLER=1

#define MOTOR_AXIS_COUNT 10   
struct UC2_3_CAN_HAT_Master : PinConfig
{
     /*
     This is the newest electronics where direction/enable are on a seperate port extender
     */
  
     const char * pindefName = "UC2_3_CAN_HAT_Master";
     const unsigned long BAUDRATE = 115200;

     int8_t MOTOR_A_STEP = GPIO_NUM_0;
     int8_t MOTOR_X_STEP = GPIO_NUM_0;
     int8_t MOTOR_Y_STEP = GPIO_NUM_0;
     int8_t MOTOR_Z_STEP = GPIO_NUM_0;

     bool isDualAxisZ = false;
     
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

     int8_t LASER_1 = disabled;
     int8_t LASER_2 = disabled;
     int8_t LASER_3 = disabled;

     int8_t LED_PIN = GPIO_NUM_19;
     int8_t LED_COUNT = 1;

     // FIXME: Is this redudant?!
     int8_t DIGITAL_IN_1 = disabled;
     int8_t DIGITAL_IN_2 = disabled;
     int8_t DIGITAL_IN_3 = disabled;
     
     int8_t dac_fake_1 = disabled; //GPIO_NUM_25; // RESET-ABORT just toggles between 1 and 0
     int8_t dac_fake_2 = disabled; //GPIO_NUM_26; // Coolant

     // const char * PSX_MAC = "1a:2b:3c:01:01:04";
     // int8_t PSX_CONTROLLER_TYPE = 2; // 1: PS3, 2: PS4
     int8_t JOYSTICK_SPEED_MULTIPLIER = 5;
     int8_t JOYSTICK_MAX_ILLU = 100;
     int8_t JOYSTICK_SPEED_MULTIPLIER_Z = 10;
     
     // for caliper
     int8_t ENC_X_A = disabled;
     int8_t ENC_Y_A = disabled;
     int8_t ENC_Z_A = disabled;
     int8_t ENC_X_B = disabled;
     int8_t ENC_Y_B = disabled;
     int8_t ENC_Z_B = disabled;

     // I2c
     int8_t I2C_SCL = GPIO_NUM_22;      // This is the poart that connects to all other slaves
     int8_t I2C_SDA = GPIO_NUM_21;



     // WIFI - specific to SEEED microscope
     const char *mSSID = "UC2x-CAN-HAT";
     const char *mPWD =  "";
     bool mAP = true; // false;
     const char *mSSIDAP = "UC2";            
     const char *hostname = "youseetoo";     

     
     // CAN
     int8_t CAN_TX = GPIO_NUM_17;
     int8_t CAN_RX = GPIO_NUM_18;

     uint32_t CAN_ID_CURRENT = CAN_ID_CENTRAL_NODE;

     // Objective
     uint8_t objectiveMotorAxis = 0; // 0=A, 1=X, 2=Y, 3=Z
     uint32_t objectivePositionX1 = 5000;
     uint32_t objectivePositionX2 = 35000;
     int8_t objectiveHomeDirection = -1;
     int8_t objectiveHomeEndStopPolarity = 0;
			
};
const UC2_3_CAN_HAT_Master pinConfig;