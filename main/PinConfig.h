#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER
struct UC2_ESP32S3_XIAO_LEDRING : PinConfig
{
     /*
     D0: 1
     D1: 3
     D2: 3
     D3: 4
     D4: 5
     D5: 6
     D6: 43
     D7: 44
     D8: 7
     D9: 8
     D10: 9

     # XIAO

     {"task":"/ledarr_act", "qid":1, "led":{"LEDArrMode":1, "led_array":[{"id":0, "r":255, "g":255, "b":255}]}}
     {"task":"/ledarr_act", "qid":1, "led":{"LEDArrMode":1, "led_array":[{"id":0, "r":0, "g":0, "b":255}]}}

     {"task": "/laser_act", "LASERid":1, "LASERval": 500, "LASERdespeckle": 500,  "LASERdespecklePeriod": 100}
     {"task": "/laser_act", "LASERid":2, "LASERval": 0}
     {"task": "/laser_act", "LASERid":1, "LASERval": 1024}
     {"task": "/laser_act", "LASERid":1, "LASERval": 0}
     {"task": "/laser_act", "LASERid":2, "LASERval": 1024}
     */
     const char *pindefName = "UC2_ESP32S3_XIAO_LEDRING";
     const unsigned long BAUDRATE = 115200;

     uint8_t I2C_CONTROLLER_TYPE = I2CControllerType::mLASER;
     uint8_t I2C_ADD_SLAVE = I2C_ADD_LEX_PWM1; // I2C address of the ESP32 if it's a slave

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

};
const UC2_ESP32S3_XIAO_LEDRING pinConfig;
