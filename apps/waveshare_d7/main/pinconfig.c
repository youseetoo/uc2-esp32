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
    .pindefName = "seeed_xiao_esp32s3_can_slave_motor",
    .BAUDRATE = 115200,

    /* Motor driver */
    .MOTOR_X_STEP = 0,   // D9 -> GPIO8
    .MOTOR_X_DIR = 0,    // D8 -> GPIO7
    .MOTOR_ENABLE = 0,   // D10 -> GPIO9
    .MOTOR_ENABLE_INVERTED = true,
    .MOTOR_AUTOENABLE = false,
    .AccelStepperMotorType = 1,

    /* I2C — as slave */
    .I2C_ADD_SLAVE = I2C_ADD_MOT_X,  // I2C address of the ESP32 if it's a slave
    .I2C_SCL = GPIO_NUM_NC,
    .I2C_SDA = GPIO_NUM_NC,

    /* I2C — as controller (unused on this board) */
    .I2C_SCL_ext = GPIO_NUM_NC,
    .I2C_SDA_ext = GPIO_NUM_NC,

    /* TMC UART (unused on this board) */
    .tmc_SW_RX = GPIO_NUM_NC,
    .tmc_SW_TX = GPIO_NUM_NC,
    .tmc_pin_diag = GPIO_NUM_NC,

    /* TMC driver settings */
    .tmc_microsteps = 16,
    .tmc_rms_current = 850,
    .tmc_stall_value = 100,
    .tmc_sgthrs = 100,
    .tmc_semin = 5,
    .tmc_semax = 2,
    .tmc_sedn = 1,
    .tmc_tcoolthrs = 0xFFFFF,
    .tmc_blank_time = 24,
    .tmc_toff = 4,

    /* CAN */
    .CAN_TX = GPIO_NUM_20,
    .CAN_RX = GPIO_NUM_19,
    .CAN_ID_CURRENT = CAN_ID_MOT_X,

    /* Endstops */
    .DIGITAL_IN_1 = GPIO_NUM_1,

    /* Objective positions */
    .objectivePositionX0 = 10000,
    .objectivePositionX1 = 80000,

    /* OTA settings */
    .OTA_TIME_FROM_STARTUP = 30000,  // 30 seconds

    /* Debug */
    .dumpHeap = true,
};
