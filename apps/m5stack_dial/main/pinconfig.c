// T5.4: M5Stack Dial board pin config (pure-C model, T4.1 approach).
// Overrides only fields that differ from the shared safe defaults (PINCONFIG_DEFAULTS_FIELDS).
// Original PIO PinConfig.h converted from C++ derived struct to C field overrides.

#include <driver/gpio.h>
#include "pinconfig_defaults.h"

const struct PinConfig pinConfig = {
    PINCONFIG_DEFAULTS_FIELDS
    /* ── M5Stack Dial overrides ── */
    .pindefName = "UC2_m5stack_dial",

    /* CAN Bus Pins (via Grove connector - requires CAN transceiver like SN65HVD230) */
    .CAN_TX = GPIO_NUM_2,   /* Grove Yellow wire - TWAI TX */
    .CAN_RX = GPIO_NUM_1,   /* Grove White wire - TWAI RX */

    /* Own CANopen node-id: 62, right after the PTZ bridge (61) and the GPIO slave (60),
       outside the motor (10..19) / laser (20..) / LED (30) ranges. */
    .CAN_ID_CURRENT = 62,

    /* Laser channels 0..3 all live on the illumination node 0x14, OD sub 1..4
       (same map as UC2_canopen_master so the dial addresses the same hardware). */
    .CAN_NODE_LASER = {0x14, 0x14, 0x14, 0x14},

    /* Routing — everything the dial touches is REMOTE (1); the rest OFF (2).
       Indexed by Stepper enum: A=0, X=1, Y=2, Z=3. */
    .ROUTE_MOTOR = {1, 1, 1, 1},
    .ROUTE_HOME  = {2, 2, 2, 2},
    .ROUTE_TMC   = {2, 2, 2, 2},
    .ROUTE_LASER = {1, 1, 1, 1},
    .ROUTE_LED   = 1,  /* LED matrix on CAN_ID_LED_0 (default 30) */

    /* WiFi (optional, can be enabled for OTA updates) */
    .mSSID = "UC2-M5Dial",
    .mPWD = "",
    .mAP = false,
    .mSSIDAP = "UC2-M5Dial",
    .hostname = "uc2-dial",
};
