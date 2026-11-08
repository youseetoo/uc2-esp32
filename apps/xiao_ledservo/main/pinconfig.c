// T6.6: xiao_ledservo — board pin table, pure C.
//
// Starts from PINCONFIG_DEFAULTS_FIELDS (all pins disabled / safe defaults)
// and overrides only the fields this board wires up. Former C++ derived struct
// `UC2_ESP32S3_XIAO_LEDSERVO : PinConfig` (lived in PinConfig.h).
#include "pinconfig_defaults.h"

const struct PinConfig pinConfig = {
    PINCONFIG_DEFAULTS_FIELDS
    /* ── board overrides (after the defaults — later designators win) ── */
    .pindefName = "UC2_esp32s3_xiao_ledservo",
    .BAUDRATE = 115200,

    /* Motor driver options */
    .MOTOR_ENABLE_INVERTED = true,
    .MOTOR_AUTOENABLE = false,
    .AccelStepperMotorType = 1,

    /* Laser control pins */
    .LASER_0 = 44,           // D7 (Servo PWM 1)
    .LASER_1 = GPIO_NUM_4,   // D3 (Motor 1/2)
    .LASER_2 = GPIO_NUM_3,   // D2 (Motor 1/1)
    .LASER_3 = GPIO_NUM_7,   // D8 (Servo PWM 2)

    /* Digital outputs */
    .DIGITAL_OUT_1 = GPIO_NUM_8,  // D9 (Motor 2/2)
    .DIGITAL_OUT_2 = GPIO_NUM_9,  // D10 (Motor 2/1)
    .DIGITAL_OUT_3 = GPIO_NUM_NC,

    /* LED */
    .LED_PIN = 43,           // D6 Neopixel
    .LED_COUNT = 1,

    /* Joystick settings */
    .JOYSTICK_SPEED_MULTIPLIER = 30,
    .JOYSTICK_MAX_ILLU = 100,
    .JOYSTICK_SPEED_MULTIPLIER_Z = 30,

    /* I2C */
    .I2C_SCL = GPIO_NUM_6,   // D5
    .I2C_SDA = GPIO_NUM_5,   // D4
};
