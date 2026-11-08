// T6.6: UC2 Standalone on Wemos D1 R32 — board pin table, pure C.
//
// Starts from PINCONFIG_DEFAULTS_FIELDS (all pins disabled / safe defaults)
// and overrides only the fields this board wires up. Former C++ derived struct
// `UC2_WEMOS : PinConfig` (lived in PinConfig.h).
//
// TCA/DAC/DS18B20 pins are not wired on this board's minimal header set.
#include "pinconfig_defaults.h"

const struct PinConfig pinConfig = {
    PINCONFIG_DEFAULTS_FIELDS
    /* ── board overrides (after the defaults — later designators win) ── */
    .pindefName = "UC2_WEMOS",

    /* Motor drivers (ESP32-WEMOS D1 R32) */
    .MOTOR_A_DIR = GPIO_NUM_23,   // Bridge from Endstop Z to Motor A
    .MOTOR_X_DIR = GPIO_NUM_16,
    .MOTOR_Y_DIR = GPIO_NUM_27,
    .MOTOR_Z_DIR = GPIO_NUM_14,
    .MOTOR_A_STEP = GPIO_NUM_5,   // Bridge from Endstop Y to Motor A
    .MOTOR_X_STEP = GPIO_NUM_26,
    .MOTOR_Y_STEP = GPIO_NUM_25,
    .MOTOR_Z_STEP = GPIO_NUM_17,
    .MOTOR_ENABLE = GPIO_NUM_12,
    .MOTOR_ENABLE_INVERTED = true,

    /* Laser PWM channels */
    .LASER_1 = GPIO_NUM_18,
    .LASER_2 = GPIO_NUM_19,
    .LASER_3 = GPIO_NUM_13,

    /* LED array */
    .LED_PIN = GPIO_NUM_4,
    .LED_COUNT = 64,

    /* PS3 Bluetooth controller */
    .PSX_MAC = "1a:2b:3c:01:01:03",
    .PSX_CONTROLLER_TYPE = 2,

    /* Joystick settings */
    .JOYSTICK_SPEED_MULTIPLIER = 5,
    .JOYSTICK_SPEED_MULTIPLIER_Z = 3,

    /* Additional Digital Input Pins */
    .DIGITAL_IN_1 = GPIO_NUM_13,
    .DIGITAL_IN_2 = GPIO_NUM_5,
    .DIGITAL_IN_3 = GPIO_NUM_23,

    /* WiFi defaults */
    .mSSID = "Blynk",
    .mPWD = "12345678",
    .mAP = false,
};
