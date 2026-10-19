// T4.1: canopen_slave_laser (Xiao ESP32S3) — board pin table, pure C.
//
// Starts from PINCONFIG_DEFAULTS_FIELDS (all pins disabled / safe defaults,
// formerly the C++ base-class defaults) and overrides only the fields this
// board wires up. Former C++ derived struct `UC2_canopen_slave_laser : PinConfig`
// (lived in PinConfig.h) — the board-only fields digita_in_1/2 and
// UNUSED_1/2/3 had no readers anywhere (verified by grep over components/),
// so they are dropped here; the pin map is preserved in PinConfig.h's
// comment block.
#include "pinconfig_defaults.h"

const struct PinConfig pinConfig = {
    PINCONFIG_DEFAULTS_FIELDS
    /* ── board overrides (after the defaults — only fields that DIFFER
       from the defaults; GCC 15 C23 -Werror=override-init forbids
       re-initializing a field) ── */
    .pindefName = "UC2_canopen_slave_laser",

    /* Laser control pins (PWM for lasers) */
    .LASER_0 = GPIO_NUM_2, /* D1 (signal_1, Laser 0) */
    .LASER_1 = GPIO_NUM_3, /* D2 (signal_2, Laser 1) */
    .LASER_2 = GPIO_NUM_5, /* D4 (signal_3, Laser 2) */
    .LASER_3 = GPIO_NUM_6, /* D5 (signal_4, Laser 3) */

    /* CAN communication */
    .CAN_TX = GPIO_NUM_4, /* D3 (CAN-SEND) */
    .CAN_RX = GPIO_NUM_9, /* D10 (CAN-RECV) */
    .CAN_ID_CURRENT = 20, /* CAN_ID_LASER_0 — broadcasting address for laser PWM control */

    /* I2C: disabled in this setup (defaults already -1) */
};
