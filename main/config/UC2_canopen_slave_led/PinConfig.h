#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

// ATTENTION: THIS IS ONLY FOR LINTING!
// #define CORE_DEBUG_LEVEL
#define ESP32S3_MODEL_XIAO

// ── Controller modules enabled on this slave ───────────────────────────────
// The illumination board carries both:
//   1) A NeoPixel ring stack (4 concentric rings, 136 LEDs total)
//   2) A single high-power white LED driven by PWM (treated as "laser 0")
// Both are exposed through ONE CANopen node — there is no "secondary CAN
// address" any more. In CANopen the master simply targets different OD
// objects on the same node:
//   • LASER_PWM_VALUE     (0x2100 sub 0x01)  ← controlled remotely by the
//                                              master's logical laser id 4
//   • LED_ARRAY_MODE      (0x2200)
//   • LED_BRIGHTNESS      (0x2201)
//   • LED_UNIFORM_COLOUR  (0x2202)
//   • LED_PIXEL_DATA      (0x2210, domain transfer)
//   • LED_PATTERN_ID/SPEED(0x2220/0x2221)
#define LASER_CONTROLLER
#define LED_CONTROLLER
#define MESSAGE_CONTROLLER
#define CAN_BUS_ENABLED
#define CAN_RECEIVE_LASER          // marks this node as a CAN laser slave (NVS role default)
#define CAN_RECEIVE_LED            // marks this node as a CAN LED slave (NVS role default)
#define CAN_CONTROLLER_CANOPEN     // CANopen stack (instead of ISO-TP)

//#define DOTSTAR // outcomment if neopixel

struct UC2_canopen_slave_led : PinConfig
{
    /*
     UC2 Illumination Board (CANopen slave)
     ──────────────────────────────────────
     Seeed XIAO ESP32S3 pin map (board rev. H):
       D0  GPIO_NUM_1   – I2C SDA
       D1  GPIO_NUM_2   – temperature-sensor_1  (NTC, ADC1_CH1)
       D2  GPIO_NUM_3   – NeoPixel data
       D3  GPIO_NUM_4   – White LED PWM         (exposed as logical laser 0)
       D4  GPIO_NUM_5   – CAN TX
       D5  GPIO_NUM_6   – CAN RX
       D6  GPIO_NUM_43  – I2C SCL
       D7  GPIO_NUM_44  – fan tacho
       D8  GPIO_NUM_7   – temperature-sensor_4  (NTC, ADC1_CH6)
       D9  GPIO_NUM_8   – temperature-sensor_3  (NTC, ADC1_CH7)
       D10 GPIO_NUM_9   – temperature-sensor_2  (NTC, ADC1_CH8)

     Rev. H adds heat-sink monitoring: four NCP15XH103F03RC 10k NTCs, each
     wired 3V3 -> NTC -> node -> 1k -> GND and buffered by an LM324, plus a
     hardware over-temperature cut-off of the NeoPixel +5V supply. All four
     sensor pins are on ADC1. Boards older than rev. H have no thermistors —
     they simply leave thermal_enabled_default off (see below), or it can be
     switched off at runtime with {"task":"/temp_act","enabled":0}.

     Single CANopen node ID (CAN_ID_LED_0, default 30) covers BOTH the
     LED ring and the PWM laser. The master addresses this node through
     the routing table:
       master.ROUTE_LASER_4 = REMOTE   → CAN_NODE_LASER_4 = 30, sub-axis 0
       master.ROUTE_LED     = REMOTE   → CAN_ID_LED_0     = 30
    */

    const char *pindefName = "UC2_canopen_slave_led";
    const unsigned long BAUDRATE = 115200;

    // prints all the ISO TP Stuff - leave off in CANopen mode
    bool DEBUG_CAN_ISO_TP = 0;

    // ── Heat-sink thermistors (rev. H) ────────────────────────────────────
    // Numbered as on the schematic: temperature-sensor_1..4.
    int8_t thermal_PIN_0 = GPIO_NUM_2; // D1  – temperature-sensor_1
    int8_t thermal_PIN_1 = GPIO_NUM_9; // D10 – temperature-sensor_2
    int8_t thermal_PIN_2 = GPIO_NUM_8; // D9  – temperature-sensor_3
    int8_t thermal_PIN_3 = GPIO_NUM_7; // D8  – temperature-sensor_4
    bool thermal_enabled_default = true;

    // ── NeoPixel ring stack ───────────────────────────────────────────────
    uint8_t LED_PIN = GPIO_NUM_3; // D2 – RGB ring data

    // 12x12 matrix bounds (we drive the first 136 LEDs)
    const uint8_t MATRIX_W = 12;
    const uint8_t MATRIX_H = 12;

    // Ring definitions (total: 136 LEDs)
    const uint16_t RING_INNER_START   = 0;
    const uint16_t RING_INNER_COUNT   = 20;
    const uint16_t RING_MIDDLE_START  = 20;
    const uint16_t RING_MIDDLE_COUNT  = 28;
    const uint16_t RING_BIGGEST_START = 48;
    const uint16_t RING_BIGGEST_COUNT = 40;
    const uint16_t RING_OUTEST_START  = 88;
    const uint16_t RING_OUTEST_COUNT  = 48;

    const uint16_t LED_COUNT = RING_OUTEST_START + RING_OUTEST_COUNT; // 136

    // ── Segment definitions for directional illumination (DPC etc.) ──────
    // LED 0 is on the RIGHT segment of the inner ring. Quadrant order
    // (clockwise from LED 0): right → bottom → left → top.
    static constexpr uint16_t SEGMENT_INNER_RIGHT_START  = 0;
    static constexpr uint16_t SEGMENT_INNER_BOTTOM_START = 5;
    static constexpr uint16_t SEGMENT_INNER_LEFT_START   = 10;
    static constexpr uint16_t SEGMENT_INNER_TOP_START    = 15;
    static constexpr uint16_t SEGMENT_INNER_COUNT        = 5;

    static constexpr uint16_t SEGMENT_MIDDLE_RIGHT_START  = 0;
    static constexpr uint16_t SEGMENT_MIDDLE_BOTTOM_START = 7;
    static constexpr uint16_t SEGMENT_MIDDLE_LEFT_START   = 14;
    static constexpr uint16_t SEGMENT_MIDDLE_TOP_START    = 21;
    static constexpr uint16_t SEGMENT_MIDDLE_COUNT        = 7;

    static constexpr uint16_t SEGMENT_BIGGEST_RIGHT_START  = 0;
    static constexpr uint16_t SEGMENT_BIGGEST_BOTTOM_START = 10;
    static constexpr uint16_t SEGMENT_BIGGEST_LEFT_START   = 20;
    static constexpr uint16_t SEGMENT_BIGGEST_TOP_START    = 30;
    static constexpr uint16_t SEGMENT_BIGGEST_COUNT        = 10;

    static constexpr uint16_t SEGMENT_OUTEST_RIGHT_START  = 0;
    static constexpr uint16_t SEGMENT_OUTEST_BOTTOM_START = 12;
    static constexpr uint16_t SEGMENT_OUTEST_LEFT_START   = 24;
    static constexpr uint16_t SEGMENT_OUTEST_TOP_START    = 36;
    static constexpr uint16_t SEGMENT_OUTEST_COUNT        = 12;

    // ── White LED PWM (single channel, exposed as logical laser 0) ────────
    // The CANopen OD only carries laser channels 0-3 per node, so the slave's
    // sole PWM laser MUST live at channel 0 (OD sub 0x01). The master maps
    // its logical laser id 4 to {node = CAN_ID_LED_0, sub 0x01} via
    // ROUTE_LASER_4 / CAN_NODE_LASER_4 / CAN_SUBAXIS_LASER_4.
    int8_t LASER_0 = GPIO_NUM_4; // D3 – white LED PWM
    bool   testLaserPinOnBoot = true;

    // PWM configuration for the white LED driver:
    // - Minimum 10 kHz required to avoid flicker at low dimming levels
    // - ESP32-S3 LEDC uses a 40 MHz clock source; 10 kHz + 11-bit is the
    //   highest resolution that keeps div_param >= 256 (-> integer >= 1).
    int LASER_PWM_FREQUENCY = 10000; // Hz
    int LASER_PWM_RESOLUTION = 11;   // bits (2048 steps)

    // ── CAN bus ───────────────────────────────────────────────────────────
    int8_t CAN_TX = GPIO_NUM_5; // D4
    int8_t CAN_RX = GPIO_NUM_6; // D5

    // Single CANopen node ID. The master writes BOTH the laser PWM (OD
    // 0x2100:01) and the LED array (OD 0x2200/0x2202/0x2210/...) to this
    // single node — no secondary CAN ID is needed under CANopen.
    uint32_t CAN_ID_CURRENT = CAN_ID_LED_0; // default 30

    // ── Route overrides ──────────────────────────────────────────────────
    // Make the routing table explicit so the inferred behaviour cannot
    // accidentally turn one of the locally driven peripherals OFF on a
    // CAN_SLAVE node.
    int8_t ROUTE_LED      = 0;            // LED ring: LOCAL
    int8_t ROUTE_LASER[4] = {0, 2, 2, 2}; // ch0 LOCAL (PWM), ch1..3 OFF
    int8_t ROUTE_LASER_4  = 2;            // no extra laser on this board

    // ── I2C ───────────────────────────────────────────────────────────────
    // Routed on rev. H (SDA = D0/GPIO1, SCL = D6/GPIO43) but left disabled
    // until something on the bus is actually driven from firmware.
    int8_t I2C_SCL = -1;
    int8_t I2C_SDA = -1;

    // ── Fan ───────────────────────────────────────────────────────────────
    // Tacho feedback from the fan socket. The speed itself is set by the
    // board's software-controlled voltage output, which is not driven yet.
    int8_t FAN_TACHO_PIN = GPIO_NUM_44; // D7
};

const UC2_canopen_slave_led pinConfig;
