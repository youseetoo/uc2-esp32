// T4.3: canopen_slave_accelmotor (Xiao ESP32S3) — board pin table, pure C.
//
// Starts from PINCONFIG_DEFAULTS_FIELDS (all pins disabled / safe defaults,
// formerly the C++ base-class defaults) and overrides only the fields this
// board wires up. Former C++ derived struct `UC2_canopen_slave_motor : PinConfig`
// (lived in PinConfig.h; note: the old header carried over the
// "UC2_canopen_slave_motor" pindefName verbatim — kept here for faithfulness).
//
// Single-axis (X) stage-motor CANopen slave: AccelStepper stepper X
// (STEP/DIR/EN GPIO8/7/9), TMC2209 driver over UART (GPIO43/44), linear
// encoder (ENC_X_A/B GPIO5/6, PCNT), hard limit (DIGITAL_IN_1 GPIO1), CAN
// (GPIO2/3). MOTOR_AXIS_COUNT=10 — the axis framework allocates per-axis
// state; only X is wired on this board.
//
// Dropped derived-only fields (no struct field in T4.1 PinConfigDefault.h,
// consumed by no code in the shared uc2_app main.cpp): I2C_NAME "MOTX",
// I2C_SCL_ext/I2C_SDA_ext (== disabled), objectivePositionX0/X1
// (10000/80000), OTA_TIME_FROM_STARTUP (30000 — only read under
// #ifdef OTA_ON_STARTUP, which this app does not define).
#include "pinconfig_defaults.h"

const struct PinConfig pinConfig = {
    PINCONFIG_DEFAULTS_FIELDS
    /* ── board overrides (fields that DIFFER from the defaults; the main
       target compiles with -Wno-override-init, see main/CMakeLists.txt) ── */
    .pindefName = "UC2_canopen_slave_motor",
    .BAUDRATE = 921600,
    .serialTimeout = 100,

    /* Motor step/dir/enable (X axis on-board) */
    .MOTOR_X_STEP = GPIO_NUM_8,
    .MOTOR_X_DIR  = GPIO_NUM_7,
    .MOTOR_ENABLE = GPIO_NUM_9,
    .MOTOR_ENABLE_INVERTED = true,
    .AccelStepperMotorType = 1,

    /* TMC2209 UART (X axis driver) — CoolStep stays off (defaults
       tmc_semin/semax = 0): it throttles current on light load, the
       opposite of a stage axis */
    .tmc_SW_RX = GPIO_NUM_44,
    .tmc_SW_TX = GPIO_NUM_43,
    .tmc_pin_diag = GPIO_NUM_4,
    .tmc_rms_current = 1050,

    /* CAN (slave node — single X axis, CAN_ID_MOT_X = 11; no axis variants
       on this board, so the literal is used rather than a CAN_AXIS_ID define) */
    .CAN_TX = GPIO_NUM_3,
    .CAN_RX = GPIO_NUM_2,
    .CAN_ID_CURRENT = 11, /* CAN_ID_MOT_X */

    /* I2C — this board is the I2C slave (linear encoder) on the laser's bus */
    .I2C_CONTROLLER_TYPE = I2C_CONTROLLER_LINEAR_ENCODER, /* was `= 2` */
    .I2C_ADD_SLAVE = 0x40, /* == I2C_ADD_MOT_X default */

    /* Routing: 0=LOCAL, 1=REMOTE, 2=OFF (-1=infer)
       A=OFF, X=LOCAL, Y=OFF, Z=OFF (only X is on-board) */
    .ROUTE_MOTOR = {2, 0, 2, 2},
    .ROUTE_HOME  = {2, 0, 2, 2},
    .ROUTE_TMC   = {2, 0, 2, 2},
    .ROUTE_LASER = {2, 2, 2, 2},
    .ROUTE_LED   = 2,
    .ROUTE_GALVO = 2,

    /* Endstop (X axis hard limit, active-low via polarity).
       homeEndstopReleaseSteps = 50 == default (back-off off the endstop
       before setting home=0). */
    .DIGITAL_IN_1 = GPIO_NUM_1,
    .hardLimitEnabledX = true,
    .hardLimitPolarityX = true,

    /* Linear encoder (AS5311 interface, X axis) */
    .ENC_X_A = GPIO_NUM_5,
    .ENC_X_B = GPIO_NUM_6,
};
