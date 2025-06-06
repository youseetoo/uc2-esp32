#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

// ATTENTION: THIS IS ONLY FOR LINTING!
#define CORE_DEBUG_LEVEL
#define ESP32S3_MODEL_XIAO 
#define LASER_CONTROLLER
#define LED_CONTROLLER
#define MESSAGE_CONTROLLER
#define CAN_SLAVE_LASER
#define CAN_SLAVE_LED
#define CAN_CONTROLLER

struct UC2_3_Xiao_Slave_Illumination : PinConfig
{
     /*
     UC2 Illumination Board Pin Mapping:
     D0: GPIO_NUM_1  - Touch electrode 1
     D1: GPIO_NUM_2  - Touch electrode 2
     D2: GPIO_NUM_3  - RGB ring data (NeoPixel)
     D3: GPIO_NUM_4  - White LED PWM
     D4: GPIO_NUM_5  - CAN Tx
     D5: GPIO_NUM_6  - CAN Rx
     D6: GPIO_NUM_43 - Spare (SPI CS)
     D7: GPIO_NUM_44 - Spare
     D8: GPIO_NUM_7  - SPI SCK
     D9: GPIO_NUM_8  - SPI MISO
     D10: GPIO_NUM_9 - SPI MOSI

     This configuration combines laser PWM control and LED NeoPixel control
     for the UC2 illumination board with 4 concentric LED rings.
     */
     
    const char *pindefName = "seeed_xiao_esp32s3_can_slave_illumination";
    const unsigned long BAUDRATE = 115200;

    // Touch sensor inputs
    int8_t TOUCH_1 = GPIO_NUM_1; // D0 - Touch electrode 1
    int8_t TOUCH_2 = GPIO_NUM_2; // D1 - Touch electrode 2

    // LED Configuration for NeoPixel (4 concentric rings)
    uint8_t LED_PIN = GPIO_NUM_3; // D2 - RGB ring data
    
    // For ring LEDs, we set matrix dimensions to get approximately 136 LEDs
    // 12x12 = 144 LEDs (we'll only use the first 136)
    const uint8_t MATRIX_W = 12;
    const uint8_t MATRIX_H = 12;
    
    // Ring definitions for LED indexing (total: 136 LEDs)
    const uint16_t RING_INNER_START = 0;      // Inner ring: 20 LEDs (indices 0-19)
    const uint16_t RING_INNER_COUNT = 20;
    const uint16_t RING_MIDDLE_START = 20;    // Middle ring: 28 LEDs (indices 20-47)
    const uint16_t RING_MIDDLE_COUNT = 28;
    const uint16_t RING_BIGGEST_START = 48;   // Biggest ring: 40 LEDs (indices 48-87)
    const uint16_t RING_BIGGEST_COUNT = 40;
    const uint16_t RING_OUTEST_START = 88;    // Outest ring: 48 LEDs (indices 88-135)
    const uint16_t RING_OUTEST_COUNT = 48;

    // Laser control pin (PWM for high-power white LED)
    int8_t LASER_0 = GPIO_NUM_4; // D3 - White LED PWM (only one channel available)

    // CAN communication
    int8_t CAN_TX = GPIO_NUM_5; // D4 - CAN Tx
    int8_t CAN_RX = GPIO_NUM_6; // D5 - CAN Rx
    
    // The device needs to listen to both laser and LED CAN addresses
    uint32_t CAN_ID_CURRENT = CAN_ID_LED_0; // Primary address for LED control
    uint32_t CAN_ID_SECONDARY = CAN_ID_LASER_0; // Secondary address for laser control

    // I2C Configuration (Disabled in this setup)
    int8_t I2C_SCL = -1; // Disabled
    int8_t I2C_SDA = -1; // Disabled

    // Spare pins available for future expansion (SPI)
    int8_t SPI_CS = GPIO_NUM_43;  // D6 - SPI CS
    int8_t SPI_SCK = GPIO_NUM_7;  // D8 - SPI SCK
    int8_t SPI_MISO = GPIO_NUM_8; // D9 - SPI MISO
    int8_t SPI_MOSI = GPIO_NUM_9; // D10 - SPI MOSI
    int8_t SPARE_1 = GPIO_NUM_44; // D7 - Spare digital I/O

    // Debug flag for CAN communication
    bool DEBUG_CAN_ISO_TP = 0;
};
  
const UC2_3_Xiao_Slave_Illumination pinConfig;