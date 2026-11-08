// T6.6: UC2 Standalone V4 — board pin table, pure C.
//
// Starts from PINCONFIG_DEFAULTS_FIELDS (all pins disabled / safe defaults)
// and overrides only the fields this board wires up. Former C++ derived struct
// `UC2_4 : PinConfig` (lived in PinConfig.h).
//
// Newest electronics where direction/enable are on a separate port expander
// (TCA9535). UC2_4 = UC2_3 flags MINUS DAC_CONTROLLER.
#include "pinconfig_defaults.h"

const struct PinConfig pinConfig = {
    PINCONFIG_DEFAULTS_FIELDS
    /* ── board overrides (after the defaults — later designators win) ── */
    .pindefName = "UC2_4",
    .BAUDRATE = 115200,

    /* Motor step pins */
    .MOTOR_A_STEP = GPIO_NUM_15,
    .MOTOR_X_STEP = GPIO_NUM_16,
    .MOTOR_Y_STEP = GPIO_NUM_14,
    .MOTOR_Z_STEP = GPIO_NUM_0,

    /* Camera I/O lines from schematic block */
    .CAM_IO0_IN = GPIO_NUM_25,   // "Cam In Line 0"
    .CAM_IO1_OUT = GPIO_NUM_NC,  // "Cam Out Line 1"
    .CAM_IO2_IO = GPIO_NUM_NC,   // "Cam In/Out Line 2"
    .CAMERA_TRIGGER_PIN = GPIO_NUM_25,
    .CAMERA_TRIGGER_INVERTED = false,

    /* Direction pins live on TCA9535 */
    .MOTOR_ENABLE = 0,
    .MOTOR_X_DIR = 1,
    .MOTOR_Y_DIR = 2,
    .MOTOR_Z_DIR = 3,
    .MOTOR_A_DIR = 4,

    /* Motor driver options */
    .MOTOR_ENABLE_INVERTED = true,
    .MOTOR_AUTOENABLE = false,
    .AccelStepperMotorType = 1,

    /* Laser PWM channels (1-3 go to headers, 4 is exclusively on LASERBOX jack) */
    .LASER_1 = GPIO_NUM_12,
    .LASER_2 = GPIO_NUM_4,
    .LASER_3 = GPIO_NUM_2,
    .LASER_4 = GPIO_NUM_17,

    /* NeoPixel LED array */
    .LED_PIN = GPIO_NUM_13,
    .LED_COUNT = 64,

    /* Digital inputs on TCA9535 */
    .DIGITAL_IN_1 = 5,
    .DIGITAL_IN_2 = 6,
    .DIGITAL_IN_3 = 7,

    /* Joystick settings */
    .JOYSTICK_SPEED_MULTIPLIER = 2,
    .JOYSTICK_MAX_ILLU = 255,
    .JOYSTICK_SPEED_MULTIPLIER_Z = 1,

    /* I2C */
    .I2C_SCL = GPIO_NUM_22,
    .I2C_SDA = GPIO_NUM_21,
    .I2C_ADD_TCA = 0x27,
    .I2C_INT = GPIO_NUM_27,

    /* SPI */
    .SPI_MOSI = GPIO_NUM_23,
    .SPI_MISO = GPIO_NUM_19,
    .SPI_SCK = GPIO_NUM_18,
    .SPI_CS = GPIO_NUM_5,

    /* CAN */
    .CAN_RX = GPIO_NUM_33,
    .CAN_TX = GPIO_NUM_32,
    .CAN_ID_CURRENT = CAN_ID_CENTRAL_NODE,
};
