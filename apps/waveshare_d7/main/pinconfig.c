// T6.6: waveshare_d7 (Waveshare ESP32-S3 7" screen board) — board pin table, pure C.
//
// Starts from PINCONFIG_DEFAULTS_FIELDS (all pins disabled / safe defaults)
// and overrides only the fields this board wires up. Former C++ derived struct
// `waveshare_esp32s3_7inchScreen : PinConfig` (lived in PinConfig.h).
//
// Despite the T5.4 "M5GFX" grouping, this is a CAN MOTOR node: the config was
// copied from the XIAO can-slave-motor board (single X motor + TMC UART +
// CAN_ID_MOT_X). No display pins, no M5GFX/M5Unified.
#include "pinconfig_defaults.h"

const struct PinConfig pinConfig = {
    PINCONFIG_DEFAULTS_FIELDS
    /* ── board overrides (after the defaults — later designators win) ── */
    .pindefName = "UC2_canopen_slave_motor",
    .BAUDRATE = 921600,
    .serialTimeout = 100,

    /* Motor step/dir/enable (X axis on-board) */
    .MOTOR_X_STEP = GPIO_NUM_8,
    .MOTOR_X_DIR  = GPIO_NUM_7,
    .MOTOR_ENABLE = GPIO_NUM_9,
    .MOTOR_ENABLE_INVERTED = true,
    .AccelStepperMotorType = 1,

    /* TMC2209 UART (X axis driver) */
    .tmc_SW_RX = GPIO_NUM_44,
    .tmc_SW_TX = GPIO_NUM_43,
    .tmc_pin_diag = GPIO_NUM_4,
    .tmc_rms_current = 1050,

    /* CAN (slave node — single X axis, CAN_ID_MOT_X = 11) */
    .CAN_TX = GPIO_NUM_3,
    .CAN_RX = GPIO_NUM_2,
    .CAN_ID_CURRENT = 11, /* CAN_ID_MOT_X */

    /* I2C — this board is the I2C slave (linear encoder) on the laser's bus */
    .I2C_CONTROLLER_TYPE = I2C_CONTROLLER_LINEAR_ENCODER,
    .I2C_ADD_SLAVE = 0x40, /* == I2C_ADD_MOT_X default */

    /* Routing: 0=LOCAL, 1=REMOTE, 2=OFF (-1=infer)
       A=OFF, X=LOCAL, Y=OFF, Z=OFF (only X is on-board) */
    .ROUTE_MOTOR = {2, 0, 2, 2},
    .ROUTE_HOME  = {2, 0, 2, 2},
    .ROUTE_TMC   = {2, 0, 2, 2},
    .ROUTE_LASER = {2, 2, 2, 2},
    .ROUTE_LED   = 2,
    .ROUTE_GALVO = 2,

    /* Endstop (X axis hard limit, active-low via polarity) */
    .DIGITAL_IN_1 = GPIO_NUM_1,
    .hardLimitEnabledX = true,
    .hardLimitPolarityX = true,

    /* Linear encoder (AS5311 interface, X axis) */
    .ENC_X_A = GPIO_NUM_5,
    .ENC_X_B = GPIO_NUM_6,
};
