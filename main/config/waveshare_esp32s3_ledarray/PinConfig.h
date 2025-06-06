#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"

#undef PSXCONTROLLER

// ATTENTION: THIS IS ONLY FOR LINTING!
#define CORE_DEBUG_LEVEL
#define ESP32S3_MODEL_XIAO 
#define LED_CONTROLLER
#define WAVESHARE_ESP32S3_LEDARRAY
struct waveshare_esp32s3_ledarray : PinConfig
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
     
     const char * pindefName = "waveshare_esp32s3_ledarray";
     const unsigned long BAUDRATE = 115200;

     int8_t LED_PIN = GPIO_NUM_14;
     int8_t LED_COUNT = 64;

     bool dumpHeap = false; 

     const uint16_t serialTimeout = 100;
};
  
const waveshare_esp32s3_ledarray pinConfig;
