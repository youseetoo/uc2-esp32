// T6.6: UC2 CANopen bridge PS4 USB-host (XIAO ESP32-S3) — board pin table, pure C.
//
// Starts from PINCONFIG_DEFAULTS_FIELDS (all pins disabled / safe defaults)
// and overrides only the fields this board wires up. Former C++ derived struct
// `UC2_canopen_bridge_ps4_usbhost : PinConfig` (lived in PinConfig.h).
#include "pinconfig_defaults.h"

const struct PinConfig pinConfig = {
    PINCONFIG_DEFAULTS_FIELDS
    /* ── board overrides (after the defaults — later designators win) ── */
    .pindefName = "UC2_canopen_bridge_ps4_usbhost",
    .BAUDRATE = 500000,

    /* CAN — same pinout as the can-gpio-esp board */
    .CAN_TX = GPIO_NUM_13,
    .CAN_RX = GPIO_NUM_14,
    /* Node-id 5: the bridge is a "remote" actor; must not collide with the
       motor (10..19) / laser (20..) / LED (30) node ids on the same bus. */
    .CAN_ID_CURRENT = 5,

    /* USB-OTG (DS4 gamepad) — fixed on the S3, no PinConfig field. */

    /* Routing — all actuators REMOTE (dispatch via SDO) */
    /* Indexed by Stepper enum: A=0, X=1, Y=2, Z=3. */
    .ROUTE_MOTOR = {1, 1, 1, 1},
    .ROUTE_HOME  = {1, 1, 1, 1},
    .ROUTE_TMC   = {1, 1, 1, 1},
    .ROUTE_LASER = {1, 1, 1, 1},
    .ROUTE_LED   = 1,
    .ROUTE_GALVO = 1,

    /* No actuator pins on this node. */
    .MOTOR_X_STEP = GPIO_NUM_NC,
    .MOTOR_X_DIR  = GPIO_NUM_NC,
    .MOTOR_ENABLE = GPIO_NUM_NC,
    .I2C_SCL      = GPIO_NUM_NC,
    .I2C_SDA      = GPIO_NUM_NC,
};
