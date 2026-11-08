// T6.6: UC2 CANopen standalone v4 — board pin table, pure C.
//
// Starts from PINCONFIG_DEFAULTS_FIELDS (all pins disabled / safe defaults)
// and overrides only the fields this board wires up. Former C++ derived struct
// `UC2_canopen_standalone_v4 : PinConfig` (lived in PinConfig.h).
//
// UC2 v4 Standalone board in HYBRID mode:
// - Native motors A, X, Y, Z (axes 0-3) via TCA9535
// - CAN bus motors B-G (axes 4-9)
// - Native lasers 0-3
// - CAN bus lasers 4+
#include "pinconfig_defaults.h"

const struct PinConfig pinConfig = {
    PINCONFIG_DEFAULTS_FIELDS
    /* ── board overrides (after the defaults — later designators win) ── */
    .pindefName = "UC2_canopen_standalone_v4",
    .BAUDRATE = 115200,

    /* Native motor drivers (on-board via TCA9535) */
    .MOTOR_A_STEP = GPIO_NUM_15,
    .MOTOR_X_STEP = GPIO_NUM_16,
    .MOTOR_Y_STEP = GPIO_NUM_14,
    .MOTOR_Z_STEP = GPIO_NUM_0,

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

    /* Native laser drivers (PWM channels 1-4) */
    .LASER_1 = GPIO_NUM_12,
    .LASER_2 = GPIO_NUM_4,
    .LASER_3 = GPIO_NUM_2,
    // Laser id 4 is REMOTE on the illumination board
    .LASER_4 = GPIO_NUM_NC,

    /* Native LED array */
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

    /* I2C for TCA9535 port expander */
    .I2C_SCL = GPIO_NUM_22,
    .I2C_SDA = GPIO_NUM_21,
    .I2C_ADD_TCA = 0x27,
    .I2C_INT = GPIO_NUM_27,

    /* SPI (unused but configured) */
    .SPI_MOSI = GPIO_NUM_23,
    .SPI_MISO = GPIO_NUM_19,
    .SPI_SCK = GPIO_NUM_18,
    .SPI_CS = GPIO_NUM_5,

    /* CAN bus configuration */
    .CAN_RX = GPIO_NUM_33,
    .CAN_TX = GPIO_NUM_32,
    .CAN_ID_CURRENT = 1, /* CAN_ID_CENTRAL_NODE */

    /* Routing: 0=LOCAL, 1=REMOTE, 2=OFF */
    .ROUTE_MOTOR = {0, 0, 0, 0},
    .ROUTE_HOME  = {0, 0, 0, 0},
    .ROUTE_TMC   = {0, 0, 0, 0},
    .ROUTE_LASER = {0, 0, 0, 0},
    .ROUTE_GALVO = 1,  // remote

    /* Laser id 4 → REMOTE on illumination board (CAN_ID_LED_0 = 30, sub 0x01) */
    .ROUTE_LASER_4 = 1,
    .CAN_NODE_LASER_4 = 30,
    .CAN_SUBAXIS_LASER_4 = 0,

    /* NeoPixel: route LED array to the illumination board as well */
    .ROUTE_LED = 1,  // REMOTE → illumination board (CAN_ID_LED_0 = 30)
};
