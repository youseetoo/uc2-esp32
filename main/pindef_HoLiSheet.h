#pragma once
// initial pin definitions in "pindef_HoLiSheet.h"

// ESP32-WEMOS D1 R32
const int PIN_DEF_MOTOR_DIR_A = 0;
const int PIN_DEF_MOTOR_DIR_X = 0; 
const int PIN_DEF_MOTOR_DIR_Y = 0;
const int PIN_DEF_MOTOR_DIR_Z = GPIO_NUM_14;
const int PIN_DEF_MOTOR_STP_A = 0;
const int PIN_DEF_MOTOR_STP_X = 0;
const int PIN_DEF_MOTOR_STP_Y = 0;
const int PIN_DEF_MOTOR_STP_Z = GPIO_NUM_17;
const int PIN_DEF_MOTOR_EN_A = GPIO_NUM_12;
const int PIN_DEF_MOTOR_EN_X = GPIO_NUM_12;
const int PIN_DEF_MOTOR_EN_Y = GPIO_NUM_12;
const int PIN_DEF_MOTOR_EN_Z = GPIO_NUM_12;
const bool PIN_DEF_MOTOR_EN_A_INVERTED = true;
const bool PIN_DEF_MOTOR_EN_X_INVERTED = true;
const bool PIN_DEF_MOTOR_EN_Y_INVERTED = true;
const bool PIN_DEF_MOTOR_EN_Z_INVERTED = true;

const int PIN_DEF_LASER_1 = GPIO_NUM_18;
const int PIN_DEF_LASER_2 = GPIO_NUM_19;
const int PIN_DEF_LASER_3 = 0; //GPIO_NUM_21 

const int PIN_DEF_LED = GPIO_NUM_4;
const int PIN_DEF_LED_NUM = 64;

const int PIN_DEF_END_X = GPIO_NUM_13;
const int PIN_DEF_END_Y = GPIO_NUM_5;
const int PIN_DEF_END_Z = GPIO_NUM_23; 

const String PIN_PS4_MAC_DEF = "1a:2b:3c:01:01:01";
const int PIN_PS4_ENUM_DEF = 2;



