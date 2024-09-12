#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER
#define M5DIAL

struct UC2_M5StackDial : PinConfig
{
     /*
     This is the M5Stack Dial Pin Configuration (only I2C)
     */


     const char * pindefName = "UC2_M5StackDial";
     const unsigned long BAUDRATE = 115200;

     // I2c
     int8_t I2C_SCL = GPIO_NUM_12; 
     int8_t I2C_SDA = GPIO_NUM_13;
     bool IS_I2C_SLAVE = true;
     I2CControllerType I2C_CONTROLLER_TYPE = I2CControllerType::mDIAL;
     int8_t I2C_ADD_SLAVE = I2C_ADD_M5_DIAL;    // I2C address of the ESP32 if it's a slave ( 0x40;)  

};
const UC2_M5StackDial pinConfig;