// T4.4 4h: canopen_slave_gpio (Xiao ESP32S3 can-gpio-esp board) — board pin
// table, pure C.
//
// Starts from PINCONFIG_DEFAULTS_FIELDS (all pins disabled / safe defaults,
// formerly the C++ base-class defaults) and overrides only the fields this
// board wires up. Former C++ derived struct `UC2_canopen_slave_gpio : PinConfig`
// (lived in PinConfig.h).
//
// CANopen GPIO slave: remote-controllable digital outputs (master writes
// x2301:01/02), E-stop button input (active LOW — disabled on this board),
// collision/resistance sensor ADC (50 Hz, EWMA + adaptive collision
// detection), generic I2C passthrough bus (I2cBridge), CAN (GPIO2/3, default
// node-id 60 = CAN_ID_GPIO_0).
#include "pinconfig_defaults.h"

const struct PinConfig pinConfig = {
    PINCONFIG_DEFAULTS_FIELDS
    /* ── board overrides (after the defaults — only fields that DIFFER
       from the defaults; GCC 15 C23 -Werror=override-init forbids
       re-initializing a field) ── */
    .pindefName = "UC2_canopen_slave_gpio",

    /* CAN bus */
    .CAN_TX = GPIO_NUM_3,   // D4 in (I2C SDA) CAN Motor Board
    .CAN_RX = GPIO_NUM_2,   // D1 in (I2C SCL) CAN Motor Board
    .CAN_ID_CURRENT = 60,   // = CAN_ID_GPIO_0, default node-id

    /* Remote-controllable digital outputs */
    .DIGITAL_OUT_1 = GPIO_NUM_1, // D0 -> remote-GPIO1
    .DIGITAL_OUT_2 = GPIO_NUM_4, // D3 -> remote-GPIO4

    /* Digital inputs */
    .DIGITAL_IN_1 = GPIO_NUM_7,  // D8

    /* Collision sensor (resistance → voltage divider on ADC) */
    .GPIO_COLLISION_ADC = GPIO_NUM_9, // D10
    .analogin_PIN_0 = GPIO_NUM_9,

    /* Routing — everything stays LOCAL on this slave (2 = OFF) */
    .ROUTE_MOTOR   = {2, 2, 2, 2},
    .ROUTE_HOME    = {2, 2, 2, 2},
    .ROUTE_TMC     = {2, 2, 2, 2},
    .ROUTE_LASER   = {2, 2, 2, 2},
    .ROUTE_LASER_4 = 2,
    .ROUTE_LED     = 2,
    .ROUTE_GALVO   = 2,

    /* I2C — generic sensor passthrough bus (I2cBridge) */
    .I2C_SDA = GPIO_NUM_5, // D4
    .I2C_SCL = GPIO_NUM_6, // D5
};
