// T6.6: UC2 CANopen slave galvo (XIAO ESP32-S3) — board pin table, pure C.
//
// Starts from PINCONFIG_DEFAULTS_FIELDS (all pins disabled / safe defaults)
// and overrides only the fields this board wires up. Former C++ derived struct
// `UC2_canopen_slave_galvo : PinConfig` (lived in PinConfig.h).
//
// MCP4822 dual 12-bit SPI DAC (X/Y mirror positioning) + RMT 3-trigger system
// (pixel/line/frame) + laser-enable GPIO.
#include "pinconfig_defaults.h"

const struct PinConfig pinConfig = {
    PINCONFIG_DEFAULTS_FIELDS
    /* ── board overrides (after the defaults — later designators win) ── */
    .pindefName = "UC2_canopen_slave_galvo",
    .BAUDRATE = 115200,

    /* CAN communication */
    .CAN_TX = GPIO_NUM_5,    // D4
    .CAN_RX = GPIO_NUM_44,   // D7
    .CAN_ID_CURRENT = 40,  /* CAN_ID_GALVO_0 — struct FIELD in PinConfigDefault,
                              unusable as a C initializer (T4.3); value from
                              pinconfig_defaults.h */

    /* MCP4822 dual DAC over SPI (channel A = X, channel B = Y)
       uint8_t to match base PinConfigDefault (see hasLocalGalvoPin / 0xFF) */
    .galvo_miso = GPIO_NUM_NC,   // not used (write-only SPI)
    .galvo_sck  = GPIO_NUM_7,    // D8
    .galvo_sdi  = GPIO_NUM_9,    // D10 (MOSI)
    .galvo_cs   = GPIO_NUM_8,    // D9
    .galvo_ldac = GPIO_NUM_6,    // D5 (latch)

    /* Laser enable + RMT 3-trigger system */
    .galvo_laser      = GPIO_NUM_43, // D6
    .galvo_trig_pixel = GPIO_NUM_2,  // D1
    .galvo_trig_line  = GPIO_NUM_3,  // D2
    .galvo_trig_frame = GPIO_NUM_4,  // D3

    /* Routing: 0=LOCAL, 1=REMOTE, 2=OFF */
    .ROUTE_MOTOR = {2, 2, 2, 2},
    .ROUTE_HOME  = {2, 2, 2, 2},
    .ROUTE_TMC   = {2, 2, 2, 2},
    .ROUTE_LASER = {2, 2, 2, 2},
    .ROUTE_LED   = 2,
    .ROUTE_GALVO = 0,  // LOCAL (this is the galvo slave)
};
