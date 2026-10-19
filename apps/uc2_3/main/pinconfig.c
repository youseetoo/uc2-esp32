// T4.1: UC2 Standalone V3 (classic ESP32, BT) — board pin table, pure C.
//
// Starts from PINCONFIG_DEFAULTS_FIELDS (all pins disabled / safe defaults,
// formerly the C++ base-class defaults) and overrides only the fields this
// board wires up. Former C++ derived struct `UC2_3 : PinConfig` (lived in
// PinConfig.h). The board-only fields ENC_A_encoderDirection and
// ENC_{A,X,Y,Z}_motorDirection had no readers anywhere (verified by grep
// over components/), so they are dropped; the pin map is preserved in
// PinConfig.h's comment block.
#include "pinconfig_defaults.h"

const struct PinConfig pinConfig = {
    PINCONFIG_DEFAULTS_FIELDS
    /* ── board overrides (after the defaults — later designators win) ── */
    .pindefName = "UC2_3",

    /* Motor step pins (native) */
    .MOTOR_A_STEP = GPIO_NUM_15,
    .MOTOR_X_STEP = GPIO_NUM_16,
    .MOTOR_Y_STEP = GPIO_NUM_14,
    .MOTOR_Z_STEP = GPIO_NUM_0,

    /* Motor enable / direction live on the TCA9535 port extender */
    .MOTOR_ENABLE = 0,
    .MOTOR_X_DIR = 1,
    .MOTOR_Y_DIR = 2,
    .MOTOR_Z_DIR = 3,
    .MOTOR_A_DIR = 4,
    .MOTOR_ENABLE_INVERTED = true,
    .AccelStepperMotorType = 1,

    /* Task sizing (board-tuned) */
    .ANALOGJOYSTICK_TASK_STACKSIZE = 0,
    .HIDCONTROLLER_EVENT_STACK_SIZE = 4 * 2048, /* Don't go below 2048 */
    .MOTOR_TASK_UPDATEWEBSOCKET_STACKSIZE = 0,
    .INTERRUPT_CONTROLLER_TASK_STACKSIZE = 0,
    .TCA_TASK_STACKSIZE = 1024,
    .SCANNER_TASK_STACKSIZE = 0,
    .TEMPERATURE_TASK_STACKSIZE = 0,

    /* Laser / LED */
    .LASER_0 = GPIO_NUM_26,
    .LASER_1 = GPIO_NUM_12,
    .LASER_2 = GPIO_NUM_4,
    .LASER_3 = GPIO_NUM_2,
    .LED_PIN = GPIO_NUM_13,
    .LED_COUNT = 64,

    /* Digital in — these live on the TCA9535 */
    .DIGITAL_IN_1 = 5,
    .DIGITAL_IN_2 = 6,
    .DIGITAL_IN_3 = 7,

    /* Joystick tuning */
    .JOYSTICK_SPEED_MULTIPLIER = 2,
    .JOYSTICK_SPEED_MULTIPLIER_Z = 1,

    /* Caliper / encoder */
    .ENC_X_A = GPIO_NUM_32,
    .ENC_Y_A = GPIO_NUM_34,
    .ENC_Z_A = GPIO_NUM_36,
    .ENC_X_B = GPIO_NUM_33,
    .ENC_Y_B = GPIO_NUM_35,
    .ENC_Z_B = GPIO_NUM_17,

    /* I2C (master + TCA9535 extender) */
    .I2C_SCL = GPIO_NUM_22,
    .I2C_SDA = GPIO_NUM_21,
    .I2C_ADD_TCA = 0x27,
    .I2C_INT = GPIO_NUM_27,

    /* SPI */
    .SPI_MOSI = GPIO_NUM_17,
    .SPI_MISO = GPIO_NUM_19,
    .SPI_SCK = GPIO_NUM_18,
    .SPI_CS = GPIO_NUM_5,

    /* TMC2209 (shares the SPI bus) */
    .tmc_SW_TX = GPIO_NUM_5,  /* SPI_CS */
    .tmc_SW_RX = GPIO_NUM_18, /* SPI_SCK */
    .tmc_pin_diag = GPIO_NUM_19, /* SPI_MISO */
    .tmc_semin = 5,
    .tmc_semax = 2,

    /* WiFi — specific to SEEed microscope */
    .mSSID = "UC2xSeeed-",
    .mPWD = "",

    /* Temperature */
    .DS28b20_PIN = GPIO_NUM_25,
};
