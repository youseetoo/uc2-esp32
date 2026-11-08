// T6.6: UC2 Standalone V2 — board pin table, pure C.
//
// Starts from PINCONFIG_DEFAULTS_FIELDS (all pins disabled / safe defaults)
// and overrides only the fields this board wires up. Former C++ derived struct
// `UC2_2 : PinConfig` (lived in PinConfig.h).
//
// No TCA9535 on this board.
#include "pinconfig_defaults.h"

const struct PinConfig pinConfig = {
    PINCONFIG_DEFAULTS_FIELDS
    /* ── board overrides (after the defaults — later designators win) ── */
    .pindefName = "UC2_2",

    /* Motor drivers */
    .MOTOR_A_DIR = GPIO_NUM_21,
    .MOTOR_X_DIR = GPIO_NUM_33,
    .MOTOR_Y_DIR = GPIO_NUM_16,
    .MOTOR_Z_DIR = GPIO_NUM_14,
    .MOTOR_A_STEP = GPIO_NUM_22,
    .MOTOR_X_STEP = GPIO_NUM_2,
    .MOTOR_Y_STEP = GPIO_NUM_27,
    .MOTOR_Z_STEP = GPIO_NUM_12,
    .MOTOR_ENABLE = GPIO_NUM_13,
    .MOTOR_ENABLE_INVERTED = true,
    .MOTOR_AUTOENABLE = false,

    /* Laser PWM channels */
    .LASER_1 = GPIO_NUM_17,
    .LASER_2 = GPIO_NUM_4,
    .LASER_3 = GPIO_NUM_15,

    /* LED array */
    .LED_PIN = GPIO_NUM_32,
    .LED_COUNT = 64,

    /* Digital inputs */
    .DIGITAL_IN_1 = GPIO_NUM_34,
    .DIGITAL_IN_2 = GPIO_NUM_39,
    .DIGITAL_IN_3 = GPIO_NUM_NC,

    /* PS4 Bluetooth controller */
    .PSX_MAC = "1a:2b:3c:01:01:04",
    .PSX_CONTROLLER_TYPE = 2,

    /* Joystick settings */
    .JOYSTICK_SPEED_MULTIPLIER = 30,
    .JOYSTICK_MAX_ILLU = 100,
    .JOYSTICK_SPEED_MULTIPLIER_Z = 30,
};
