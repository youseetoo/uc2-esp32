#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

// ATTENTION: THIS IS ONLY FOR LINTING!
#define CORE_DEBUG_LEVEL
#define ESP32S3_MODEL_XIAO 
#define DCONFIG_TINYUSB_MSC_BUFSIZE 2048
#define CONFIG_TINYUSB_HID_BUFSIZE 64
#define CONFIG_TINYUSB_VIDEO_STREAMING_BUFSIZE 64
#define CONFIG_TINYUSB_VENDOR_RX_BUFSIZE 64
#define CONFIG_TINYUSB_VENDOR_TX_BUFSIZE 64
#define CAN_CONTROLLER
#define CAN_SLAVE_LED
#define LED_CONTROLLER
#define HUB75

//#define DOTSTAR // outcomment if neopixel


struct waveshare_hub75_uc2lasercover : PinConfig
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


    This is a test to work with the UC2_3 board which acts as a I2C slave
     */
     
    const char *pindefName = "seeed_xiao_esp32s3_can_slave_led";
    const unsigned long BAUDRATE = 115200;

    // prints all the ISO TP Stuff - better don't use it to avoid session timeout! 
    bool DEBUG_CAN_ISO_TP = 0;

    // Interlock status
    int8_t digita_in_1= GPIO_NUM_8; // D9 (LO when interlock has power) // INTERLOCK_STATUS
    int8_t digita_in_2 = GPIO_NUM_1; // D0 (LO when interlock tripped, enable pullup) // INTERLOCK_LED

    // CAN communication
    int8_t CAN_TX = A3; 
    int8_t CAN_RX = A4; 
    uint32_t CAN_ID_CURRENT = CAN_ID_LED_0; // Broadcasting address for laser PWM control

    // I2C Configuration (Disabled in this setup)
    int8_t I2C_SCL = -1; // Disabled
    int8_t I2C_SDA = -1; // Disabled

    // Unused Pins (Available on J1101 for future expansion)
    int8_t UNUSED_1 = GPIO_NUM_43; // D6
    int8_t UNUSED_2 = GPIO_NUM_44; // D7
    int8_t UNUSED_3 = GPIO_NUM_7; // D8


    /* 64 × 32 RGB HUB75 panel driven by Protomatter
       (pinout matches the Waveshare ESP32-S3 “LED matrix” base)        */
    static const uint8_t  MATRIX_W   = 64;
    static const uint8_t  MATRIX_H   = 32;
    static const uint8_t  HUB75_BIT_DEPTH = 4;          // 2-6 OK, trade-off vs. refresh
    /*  R1 G1 B1 R2 G2 B2  */
    uint8_t HUB75_RGB_PINS[6]  = { 42, 41, 40, 38, 39, 37 };
    /*  A  B  C  D  E      */
    uint8_t HUB75_ADDR_PINS[5] = { 45, 36, 48, 35, 21 };

    uint8_t HUB75_CLK  =  2;        // CK
    uint8_t HUB75_LAT  = 47;        // LAT
    uint8_t HUB75_OE   = 14;        // OE

    /* NeoPixel/DotStar pin not used – keep a dummy value so legacy
       code that references LED_PIN still compiles.                    */
    uint8_t LED_PIN = 255;

   
};
  
const waveshare_hub75_uc2lasercover pinConfig;
