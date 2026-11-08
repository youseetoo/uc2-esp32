// T6.6: xiao_odmr (XIAO ESP32-S3) — board pin table, pure C.
//
// Starts from PINCONFIG_DEFAULTS_FIELDS (all pins disabled / safe defaults)
// and overrides only the fields this board wires up. Former C++ derived struct
// `UC2_ESP32S3_XIAO : PinConfig` (lived in PinConfig.h).
#include "pinconfig_defaults.h"

const struct PinConfig pinConfig = {
    PINCONFIG_DEFAULTS_FIELDS
    /* ── board overrides (after the defaults — later designators win) ── */
    .pindefName = "UC2_esp32s3_xiao",
    .BAUDRATE = 500000,

    /* Motor step pins */
    .MOTOR_A_STEP = GPIO_NUM_15,
    .MOTOR_X_STEP = GPIO_NUM_16,
    .MOTOR_Y_STEP = GPIO_NUM_15,
    .MOTOR_Z_STEP = GPIO_NUM_0,

    /* Motor driver options */
    .MOTOR_ENABLE_INVERTED = true,
    .MOTOR_AUTOENABLE = false,
    .AccelStepperMotorType = 1,

    /* Laser PWM channels */
    .LASER_1 = GPIO_NUM_12,
    .LASER_2 = GPIO_NUM_4,
    .LASER_3 = GPIO_NUM_2,

    /* LED array (internally connected to the LED array on the Waveshare board) */
    .LED_PIN = GPIO_NUM_14,
    .LED_COUNT = 64,

    /* Joystick settings */
    .JOYSTICK_SPEED_MULTIPLIER = 30,
    .JOYSTICK_MAX_ILLU = 100,
    .JOYSTICK_SPEED_MULTIPLIER_Z = 30,

    /* I2C */
    .I2C_SCL = GPIO_NUM_7,
    .I2C_SDA = GPIO_NUM_6,
    .I2C_ADD_TCA = 0x27,
    .I2C_ADD_SLAVE = 0x08,

    /* SPI */
    .SPI_MOSI = GPIO_NUM_17,
    .SPI_MISO = GPIO_NUM_19,
    .SPI_SCK = GPIO_NUM_18,
    .SPI_CS = GPIO_NUM_5,
};
