#pragma once
// T2.5: canopen_slave_galvo app-local PinConfig (converted from
// main/config/UC2_canopen_slave_galvo/PinConfig.h: Arduino.h -> uc2_hal.h).
// Shadows the shared main/PinConfig.h (laser) for this app's build via
// UC2_APP_MAIN_DIR (uc2_modinc.cmake prepends it). Per-app PinConfig rework
// lands in T2.11.
//
// Galvo scanner board (XIAO ESP32-S3): MCP4822 dual 12-bit SPI DAC (X/Y mirror
// positioning) + RMT 3-trigger system (pixel/line/frame) + laser-enable GPIO.
//
// The galvo pins are uint8_t to match the base PinConfigDefault: RoutingTable
// (hasLocalGalvoPin) compares `pinConfig.galvo_cs != 0xFF`, which is always
// true for a signed int8_t and trips -Werror=type-limits.
#include "uc2_hal.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

#ifndef ESP32S3_MODEL_XIAO
#define ESP32S3_MODEL_XIAO
#endif
#ifndef GALVO_CONTROLLER
#define GALVO_CONTROLLER
#endif
#ifndef CAN_RECEIVE_GALVO
#define CAN_RECEIVE_GALVO  // marks this node as a CAN galvo slave (NVS role default)
#endif
#ifndef CAN_BUS_ENABLED
#define CAN_BUS_ENABLED
#endif
#ifndef CAN_CONTROLLER_CANOPEN
#define CAN_CONTROLLER_CANOPEN  // CANopen stack (instead of ISO-TP)
#endif

struct UC2_canopen_slave_galvo : PinConfig
{
    /*
     XIAO ESP32-S3 pin map (galvo scanner board):
       D0  GPIO_NUM_1   – I2C SDA
       D1  GPIO_NUM_2   – galvo trigger: PIXEL
       D2  GPIO_NUM_3   – galvo trigger: LINE
       D3  GPIO_NUM_4   – galvo trigger: FRAME
       D4  GPIO_NUM_5   – CAN TX
       D5  GPIO_NUM_6   – galvo LDAC (DAC latch)
       D6  GPIO_NUM_43  – galvo laser enable
       D7  GPIO_NUM_44  – CAN RX
       D8  GPIO_NUM_7   – galvo SPI SCK
       D9  GPIO_NUM_8   – galvo SPI CS
       D10 GPIO_NUM_9   – galvo SPI SDI (MOSI)

     The MCP4822 is driven by a dedicated SPI bus (20 MHz) for deterministic
     mirror positioning; the three RMT trigger channels (pixel/line/frame) gate
     the acquisition camera. CAN_ID_CURRENT = CAN_ID_GALVO_0 is the single
     CANopen node id for this slave.
     */
    const char *pindefName = "UC2_canopen_slave_galvo";
    const unsigned long BAUDRATE = 115200;

    // CAN communication
    int8_t CAN_TX = GPIO_NUM_5;   // D4
    int8_t CAN_RX = GPIO_NUM_44;  // D7

    // MCP4822 dual DAC over SPI (channel A = X, channel B = Y).
    // uint8_t to match base PinConfigDefault (see hasLocalGalvoPin / 0xFF).
    uint8_t galvo_miso = -1;             // not used (write-only SPI)
    uint8_t galvo_sck  = GPIO_NUM_7;     // D8
    uint8_t galvo_sdi  = GPIO_NUM_9;     // D10 (MOSI)
    uint8_t galvo_cs   = GPIO_NUM_8;     // D9
    uint8_t galvo_ldac = GPIO_NUM_6;     // D5 (latch)

    // Laser enable + RMT 3-trigger system
    uint8_t galvo_laser      = GPIO_NUM_43; // D6
    uint8_t galvo_trig_pixel = GPIO_NUM_2;  // D1
    uint8_t galvo_trig_line  = GPIO_NUM_3;  // D2
    uint8_t galvo_trig_frame = GPIO_NUM_4;  // D3

    uint32_t CAN_ID_CURRENT = CAN_ID_GALVO_0;
    int8_t   ROUTE_GALVO    = 0;  // 0=LOCAL, 1=REMOTE, 2=OFF
};

const UC2_canopen_slave_galvo pinConfig;
