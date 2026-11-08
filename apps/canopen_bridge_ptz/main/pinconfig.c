// T6.6: UC2 CANopen bridge PTZ (XIAO ESP32-S3) — board pin table, pure C.
//
// Starts from PINCONFIG_DEFAULTS_FIELDS (all pins disabled / safe defaults)
// and overrides only the fields this board wires up. Former C++ derived struct
// `UC2_canopen_bridge_ptz : PinConfig` (lived in PinConfig.h).
#include "pinconfig_defaults.h"
#include "driver/gpio.h"

const struct PinConfig pinConfig = {
    PINCONFIG_DEFAULTS_FIELDS
    /* ── board overrides (after the defaults — later designators win) ── */
    .pindefName = "UC2_canopen_bridge_ptz",
    .BAUDRATE = 115200,

    /* CAN — same pinout as the can-gpio-esp board */
    .CAN_TX = GPIO_NUM_3,
    .CAN_RX = GPIO_NUM_2,
    /* Node-id 61: right after the GPIO slave (60), outside the motor
       (10..19) / laser (20..) / LED (30) / galvo (40) ranges. */
    .CAN_ID_CURRENT = 61,

    /* RS-485 keyboard link (UART1) */
    .PTZ_RS485_RX = GPIO_NUM_5,   // D4 (SDA) <- adapter RO
    .PTZ_RS485_TX = GPIO_NUM_6,   // D5 (SCL) -> adapter DI (listen-only, spare)
    .PTZ_BAUDRATE = 9600,         // keyboard menu: 96+ACK
    .PTZ_CAMERA_ADDRESS = 0,      // 0 = accept any Pelco camera address
    .PTZ_DEBUG_DEFAULT = 2,       // 0 quiet / 1 frames / 2 + raw hex
    .PTZ_MOTION_TIMEOUT_MS = 2000,

    /* Speed scaling (final speed = (pelcoSpeed/63) * 2000 * multiplier, steps/s) */
    .PTZ_SPEED_MULTIPLIER_XY = 50,
    .PTZ_SPEED_MULTIPLIER_Z  = 10,  // zoom rocker
    .PTZ_SPEED_MULTIPLIER_A  = 5,   // NEAR/FAR keys -> aux axis

    /* Routing — all actuators REMOTE (dispatch via SDO) */
    /* Indexed by Stepper enum: A=0, X=1, Y=2, Z=3. */
    .ROUTE_MOTOR = {1, 1, 1, 1},
    .ROUTE_HOME  = {1, 1, 1, 1},
    .ROUTE_TMC   = {1, 1, 1, 1},
    .ROUTE_LASER = {1, 1, 1, 1},
    .ROUTE_LED   = 1,
    .ROUTE_GALVO = 1,

    /* No actuator pins on this node. */
    .MOTOR_X_STEP = GPIO_NUM_NC,
    .MOTOR_X_DIR  = GPIO_NUM_NC,
    .MOTOR_ENABLE = GPIO_NUM_NC,
    .I2C_SCL      = GPIO_NUM_NC,
    .I2C_SDA      = GPIO_NUM_NC,
};
