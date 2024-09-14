#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
#include "M5Dial.h"
#undef PSXCONTROLLER
#define I2C_SLAVE
struct UC2_M5StackDial : PinConfig
{
     /*
     This is the M5Stack Dial Pin Configuration (only I2C)
     */


     const char * pindefName = "UC2_M5StackDial";
     const unsigned long BAUDRATE = 115200;

     // I2c
     int8_t I2C_SDA = 15; 
     int8_t I2C_SCL = 13; 
     I2CControllerType I2C_CONTROLLER_TYPE = I2CControllerType::mDIAL;
     int8_t I2C_ADD_SLAVE = I2C_ADD_M5_DIAL;    // I2C address of the ESP32 if it's a slave ( 0x40;)  

};
const UC2_M5StackDial pinConfig;