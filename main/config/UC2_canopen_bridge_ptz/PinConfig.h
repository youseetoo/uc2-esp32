// UC2_canopen_bridge_ptz — RS-485 PTZ keyboard → CANopen bridge node.
//
// Runs on a Seeed XIAO ESP32-S3 (the can-gpio-esp carrier board). A CCTV
// PTZ joystick keyboard (TPS3103C family, "Keyboard Controller Ver 3.057")
// talks Pelco-D/P on its RS-485 port; an RS485↔TTL transceiver feeds UART1.
// PtzKeyboard parses the frames, PtzRouter dispatches motor moves as
// expedited SDO writes (X=pan, Y=tilt, Z=zoom, A=focus) and forwards the
// discrete keys (presets, AUX, iris) to the master via TPDO2 so ImSwitch
// can map them (snap image, toggle illumination, …).
//
// Keyboard setup (once, on the keyboard itself):
//   995+AUX → BAUDRATE=9600, PROTOCOL → PTZ → PELCO_D → EXIT
//   (shortcuts: 96+ACK = 9600 baud, 44+ACK = Pelco-D)
//   then select camera address 1 with:  1 + CAM
//
// Wiring (XIAO ESP32-S3 ←→ RS485-TTL adapter ←→ keyboard RJ45/terminal):
//   D7 / GPIO44  ← RXD   (adapter RO)
//   D6 / GPIO43  → TXD   (adapter DI — unused, we only listen)
//   3V3 / GND    ↔ VCC / GND (adapter GND also to keyboard GND if available)
//   adapter A ↔ keyboard RS485A (A+), adapter B ↔ keyboard RS485B (B-)
//   (no signal on the bench? swap A/B first, then check baud/protocol)
//
// NODE_ROLE=2 + ROUTE_*=REMOTE: this node only originates commands; no
// actuator controllers are compiled in (same pattern as the PS4 bridge).
#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"

#undef PSXCONTROLLER

#define ESP32S3_MODEL_XIAO
#define MESSAGE_CONTROLLER
#define CAN_BUS_ENABLED
#define CAN_CONTROLLER_CANOPEN
#define PTZ_KEYBOARD_CONTROLLER
// PTZ_KEYBOARD_CONTROLLER also comes from build_flags in platformio.ini.

struct UC2_canopen_bridge_ptz : PinConfig
{
    bool DEBUG_CAN_ISO_TP = 0;

    const char* pindefName = "UC2_canopen_bridge_ptz";
    const unsigned long BAUDRATE = 115200;

    // ── CAN — same pinout as the can-gpio-esp board ──────────────────────
    int8_t CAN_TX = GPIO_NUM_3;
    int8_t CAN_RX = GPIO_NUM_2;
    // Node-id 61: right after the GPIO slave (60), outside the motor
    // (10..19) / laser (20..) / LED (30) / galvo (40) ranges. The master's
    // MASTER_PTZ_NODE_ID must match (see UC2_canopen_master/PinConfig.h).
    uint32_t CAN_ID_CURRENT = 61;

    // ── RS-485 keyboard link (UART1) ─────────────────────────────────────
    int8_t  PTZ_RS485_RX = GPIO_NUM_5;  // D4 (SDA) ← adapter RO
    int8_t  PTZ_RS485_TX = GPIO_NUM_6;  // D5 (SCL) → adapter DI (listen-only, spare)
    int32_t PTZ_BAUDRATE = 9600;         // keyboard menu: 96+ACK
    // Accept frames for this Pelco camera address only; 0 = accept any.
    // Handy once several bridges share one keyboard (N+CAM switches target).
    uint8_t PTZ_CAMERA_ADDRESS = 0;
    // 0 quiet / 1 decoded frames / 2 + raw hex — runtime: /ptz_act {"debug":N}
    uint8_t PTZ_DEBUG_DEFAULT = 2;
    // Stop all axes if no frame arrives for this long while moving (0 = off).
    uint32_t PTZ_MOTION_TIMEOUT_MS = 2000;

    // Speed scaling, same convention as JOYSTICK_SPEED_MULTIPLIER (final
    // speed = (pelcoSpeed/63) * 2000 * multiplier, steps/s).
    int8_t PTZ_SPEED_MULTIPLIER_XY = 50;
    int8_t PTZ_SPEED_MULTIPLIER_Z  = 10; // zoom rocker → focus drive
    int8_t PTZ_SPEED_MULTIPLIER_A  = 5;  // NEAR/FAR keys → aux axis

    // ── Routing — all actuators REMOTE (dispatch via SDO) ────────────────
    // Indexed by Stepper enum: A=0, X=1, Y=2, Z=3. A is REMOTE here because
    // the focus keys map to it; set [0]=2 (OFF) if no node 0x0A exists and
    // the NEAR/FAR keys should be ignored instead of probing a dead node
    // (the dead-node muting keeps the bus quiet either way).
    int8_t ROUTE_MOTOR[4] = {1, 1, 1, 1};
    int8_t ROUTE_HOME[4]  = {2, 2, 2, 2};
    int8_t ROUTE_TMC[4]   = {2, 2, 2, 2};
    int8_t ROUTE_LASER[4] = {2, 2, 2, 2};
    int8_t ROUTE_LED      = 2;
    int8_t ROUTE_GALVO    = 2;

    // No actuator pins on this node.
    int8_t MOTOR_X_STEP = disabled;
    int8_t MOTOR_X_DIR  = disabled;
    int8_t MOTOR_ENABLE = disabled;
    int8_t I2C_SCL      = disabled;
    int8_t I2C_SDA      = disabled;
    int8_t I2C_SCL_ext  = disabled;
    int8_t I2C_SDA_ext  = disabled;

    const uint16_t serialTimeout = 100;
};

const UC2_canopen_bridge_ptz pinConfig;
