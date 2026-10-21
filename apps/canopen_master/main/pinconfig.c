// T4.1: UC2 CANopen master (HAT v2, classic ESP32) — board pin table, pure C.
//
// Starts from PINCONFIG_DEFAULTS_FIELDS (all pins disabled / safe defaults,
// formerly the C++ base-class defaults) and overrides only the fields this
// board wires up. Former C++ derived struct `UC2_canopen_master : PinConfig`
// (lived in PinConfig.h).
#include "pinconfig_defaults.h"

const struct PinConfig pinConfig = {
    PINCONFIG_DEFAULTS_FIELDS
    /* ── board overrides (after the defaults — later designators win) ── */
    .pindefName = "UC2_canopen_master",
    .BAUDRATE = 921600,

    /* I2C (shared with 5V I2C header; solder-jumpers JP101/JP102 can bridge
       Pi<->ESP) */
    .I2C_SCL = GPIO_NUM_22,
    .I2C_SDA = GPIO_NUM_21,

    /* Power / safety integration */
    .BUSPOWER_OFF_PIN = GPIO_NUM_4,   /* HIGH = CAN bus power OFF */
    .pinEmergencyExit = GPIO_NUM_34,  /* E-STOP sense (external pull-up) */

    /* Remote GPIO/E-stop/collision slave (UC2_canopen_slave_gpio) —
       TPDO2 sniffer node id. 60 == the CAN_ID_GPIO_0 default. */
    .MASTER_GPIO_SLAVE_NODE_ID = 60,
    /* PTZ keyboard bridge (UC2_canopen_bridge_ptz). */
    .MASTER_PTZ_NODE_ID = 61,

    /* CAN (ESP32 TWAI on-board; Pi-side MCP2515 is separate) */
    .CAN_TX = GPIO_NUM_17,
    .CAN_RX = GPIO_NUM_18,
    .CAN_ID_CURRENT = 1, /* == CAN_ID_CENTRAL_NODE default */

    /* Routing overrides: 0=LOCAL, 1=REMOTE, 2=OFF (-1=infer) —
       A, X, Y, Z all remote via CAN slaves */
    .ROUTE_MOTOR = {1, 1, 1, 1},
    .ROUTE_HOME  = {1, 1, 1, 1},
    .ROUTE_TMC   = {1, 1, 1, 1},
    .ROUTE_LASER = {1, 1, 1, 1},
    .ROUTE_LED   = 1, /* on-board WS2812 is local */
    .ROUTE_GALVO = 1, /* remote */

    /* All four laser channels live on node 0x14 (sub-axes 0-3). */
    .CAN_NODE_LASER = {0x14, 0x14, 0x14, 0x14},
    .CAN_SUBAXIS_LASER = {0, 1, 2, 3},

    /* Lighting / status */
    .LED_PIN = GPIO_NUM_19,
    .LED_COUNT = 1,
    .IS_STATUS_LED = true,

    /* Camera / trigger / panelboard IO */
    .CAMERA_TRIGGER_PIN = GPIO_NUM_27, /* "Cam In Line 0" (CAM_IO0_IN) */
    /* Panelboard fan tachometer (Noctua open-drain, 2 pulses/rev,
       J1901 pin 7). */
    .FAN_TACHO_PIN = GPIO_NUM_33,

    /* Motors (step/dir/ena via expander on this rev) */
    .MOTOR_A_STEP = GPIO_NUM_0,
    .MOTOR_X_STEP = GPIO_NUM_0,
    .MOTOR_Y_STEP = GPIO_NUM_0,
    .MOTOR_Z_STEP = GPIO_NUM_0,
    .ENC_X_encoderDirection = true,
    .ENC_Y_encoderDirection = true,
    .ENC_Z_encoderDirection = true,
    .MOTOR_ENABLE_INVERTED = true,
    .MOTOR_AUTOENABLE = false,
    .AccelStepperMotorType = 1,

    /* BUZZER (HAT v2 board buzzer on GPIO25) */
    .BUZZER_PIN = GPIO_NUM_25,

    /* Objective defaults */
    .objectiveMotorAxis = 0, /* 0=A, 1=X, 2=Y, 3=Z */
    .focusMotorAxis = 3,

    /* WiFi defaults */
    .mSSID = "UC2x-CAN-HAT",
    .mPWD = "",

    /* UI / joystick */
    .JOYSTICK_SPEED_MULTIPLIER = 10,
    .JOYSTICK_SPEED_MULTIPLIER_Z = 1,
};
