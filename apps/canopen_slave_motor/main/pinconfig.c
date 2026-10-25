// T4.3: canopen_slave_motor (Xiao ESP32S3) — board pin table, pure C.
//
// Starts from PINCONFIG_DEFAULTS_FIELDS (all pins disabled / safe defaults,
// formerly the C++ base-class defaults) and overrides only the fields this
// board wires up. Former C++ derived struct `UC2_canopen_slave_motor : PinConfig`
// (lived in PinConfig.h).
//
// Multi-axis (A/X/Y/Z) stage-motor CANopen slave: FAS (FastAccelStepper, RMT
// backend) stepper X (STEP/DIR/EN GPIO8/7/9), TMC over UART (GPIO43/44), linear
// encoder (ENC_X_A/B GPIO40/41, PCNT), hard limit (DIGITAL_IN_1 GPIO1), CAN
// (GPIO2/3). MOTOR_AXIS_COUNT=10 — the axis framework allocates per-axis
// state; only X is wired on this board.
#include "pinconfig_defaults.h"

const struct PinConfig pinConfig = {
    PINCONFIG_DEFAULTS_FIELDS
    /* ── board overrides (after the defaults — only fields that DIFFER
       from the defaults; GCC 15 C23 -Werror=override-init forbids
       re-initializing a field) ── */
    .pindefName = "UC2_canopen_slave_motor",
    .BAUDRATE = 921600,

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
    .tmc_rms_current = 1400,
    .tmc_stall_value = 50,
    .tmc_sgthrs = 5,
    .tmc_sedn = 0,

    /* CAN (slave node — default X axis (id 11); UC2_CAN_AXIS env overrides
       via -DCAN_AXIS_ID=<numeric CAN id> in CMakeLists; values match
       pinconfig_defaults.h CAN_ID_MOT_A..Z = 10..13) */
    .CAN_TX = GPIO_NUM_3,
    .CAN_RX = GPIO_NUM_2,
    .CAN_ID_CURRENT = CAN_AXIS_ID,

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
    .ENC_X_A = GPIO_NUM_40,
    .ENC_X_B = GPIO_NUM_41,

    /* I2C slave address (for I2C dial — not used on this board but kept
       for the axis framework's I2C dispatch path) */
    .I2C_ADD_SLAVE = 0x40, /* == I2C_ADD_MOT_X default */

    /* Serial timeout (host->slave command framing) */
    .serialTimeout = 100,
};
