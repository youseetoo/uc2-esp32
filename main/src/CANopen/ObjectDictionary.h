/**
 * ObjectDictionary.h
 *
 * UC2 CANopen-Lite Object Dictionary schema.
 *
 * Index allocation:
 *   0x1000-0x1FFF  Standard CANopen (CiA 301)
 *   0x2000-0x2FFF  UC2 manufacturer-specific device objects
 *   0x6000-0x67FF  CiA 401 I/O profile (kept for MWE compatibility)
 *
 * Standard CANopen CAN-ID allocation:
 *   0x000        NMT command (broadcast)
 *   0x080        SYNC
 *   0x180+nodeId TPDO1  (slave → master, motor status)
 *   0x200+nodeId RPDO1  (master → slave, motor command)
 *   0x280+nodeId TPDO2  (slave → master, node status)
 *   0x300+nodeId RPDO2  (master → slave, multi-control)
 *   0x380+nodeId TPDO3  (slave → master, sensor data)
 *   0x400+nodeId RPDO3  (master → slave, LED extended)
 *   0x580+nodeId SDO response (slave → master)
 *   0x600+nodeId SDO request  (master → slave)
 *   0x700+nodeId NMT heartbeat
 *
 * PDO payload structs are packed and little-endian (native on ESP32).
 * All multi-byte integers use little-endian byte order.
 */
#pragma once
#include <stdint.h>

// ============================================================================
// Standard CANopen OD indices
// ============================================================================
#define UC2_OD_DEVICE_TYPE      0x1000u  // uint32: device type profile
#define UC2_OD_ERROR_REGISTER   0x1001u  // uint8:  error register
#define UC2_OD_HEARTBEAT_TIME   0x1017u  // uint16: producer heartbeat ms (0=off)
#define UC2_OD_IDENTITY         0x1018u  // identity object (vendor, product, ...)
#define UC2_OD_FW_DATA          0x1F50u  // OTA firmware data (octet string)
#define UC2_OD_PROG_CTRL        0x1F51u  // program control (uint8)

// ============================================================================
// UC2 Manufacturer-specific OD indices (0x2000–0x2FFF)
// ============================================================================

// Node info
#define UC2_OD_NODE_CAPS        0x2000u  // sub0: capability bitmask (uint8)
#define UC2_OD_NODE_ROLE        0x2001u  // sub0: role (uint8)

// Motor axes 0-9  (UC2_OD_MOTOR_BASE + axis gives the object index)
#define UC2_OD_MOTOR_COUNT      0x2100u  // sub0: number of motor axes (uint8)
#define UC2_OD_MOTOR_BASE       0x2101u  // 0x2101+axis: motor axis object
#define UC2_OD_MOTOR_SUB_TARGET   0x01u  //   sub1: target_position (int32)
#define UC2_OD_MOTOR_SUB_SPEED    0x02u  //   sub2: speed steps/s   (int32)
#define UC2_OD_MOTOR_SUB_CMD      0x03u  //   sub3: command byte    (uint8)
#define UC2_OD_MOTOR_SUB_ACTUAL   0x04u  //   sub4: actual_position (int32)
#define UC2_OD_MOTOR_SUB_RUNNING  0x05u  //   sub5: is_running flag (uint8)
#define UC2_OD_MOTOR_SUB_QID      0x06u  //   sub6: qid             (int16)
#define UC2_OD_MOTOR_SUB_ACCEL    0x07u  //   sub7: acceleration    (int32, steps/s²)

// Laser channels 0-9
#define UC2_OD_LASER_COUNT      0x2200u  // sub0: number of laser channels (uint8)
#define UC2_OD_LASER_BASE       0x2201u  // 0x2201+channel: laser object
#define UC2_OD_LASER_SUB_PWM      0x01u  //   sub1: pwm value 0-1023 (uint16)
#define UC2_OD_LASER_SUB_DESPECKL 0x02u  //   sub2: despeckle on/off  (uint8)
#define UC2_OD_LASER_SUB_PERIOD   0x03u  //   sub3: despeckle period  (uint16)

// LED controller
#define UC2_OD_LED_COUNT        0x2300u  // sub0: number of LEDs (uint16)
#define UC2_OD_LED_CTRL         0x2301u  // LED control object
#define UC2_OD_LED_SUB_MODE       0x01u  //   sub1: mode enum (uint8)
#define UC2_OD_LED_SUB_R          0x02u  //   sub2: red       (uint8)
#define UC2_OD_LED_SUB_G          0x03u  //   sub3: green     (uint8)
#define UC2_OD_LED_SUB_B          0x04u  //   sub4: blue      (uint8)
#define UC2_OD_LED_SUB_IDX        0x05u  //   sub5: led_index (uint16)
#define UC2_OD_LED_SUB_QID        0x06u  //   sub6: qid       (int16)

// DAC channels 0-7
#define UC2_OD_DAC_COUNT        0x2400u  // sub0: number of DAC channels (uint8)
#define UC2_OD_DAC_BASE         0x2401u  // 0x2401+ch: sub1=value (uint16)
#define UC2_OD_DAC_SUB_VALUE      0x01u

// Heater / temperature control
#define UC2_OD_HEATER_CTRL      0x2500u  // heater control object
#define UC2_OD_HEATER_SUB_TARGET  0x01u  //   sub1: target temp × 10 (int16, °C×10)
#define UC2_OD_HEATER_SUB_ACTUAL  0x02u  //   sub2: actual temp × 10 (int16)
#define UC2_OD_HEATER_SUB_PID_EN  0x03u  //   sub3: PID enabled       (uint8)

// Encoder axes 0-9
#define UC2_OD_ENCODER_COUNT    0x2600u  // sub0: encoder axis count (uint8)
#define UC2_OD_ENCODER_BASE     0x2601u  // 0x2601+axis: sub1=position (int32)
#define UC2_OD_ENCODER_SUB_POS    0x01u

// Digital I/O
#define UC2_OD_DIO_CTRL         0x2700u
#define UC2_OD_DIO_SUB_IN_COUNT  0x01u   // input count  (uint8)
#define UC2_OD_DIO_SUB_OUT_COUNT 0x02u   // output count (uint8)
#define UC2_OD_DIO_SUB_IN_STATE  0x03u   // input bitmask  (uint32)
#define UC2_OD_DIO_SUB_OUT_STATE 0x04u   // output bitmask (uint32)

// Capability table and self-test
#define UC2_OD_CAP_TABLE        0x2F00u  // sub0: same as UC2_OD_NODE_CAPS (uint8)
#define UC2_OD_SELF_TEST        0x2FF0u  // sub0: trigger (uint8, write 1 to start)

// Homing parameters per axis (0x2800 + axis)
#define UC2_OD_HOME_BASE          0x2800u  // 0x2800+axis: homing config object
#define UC2_OD_HOME_SUB_DIR         0x01u  //   sub1: direction       (int8)
#define UC2_OD_HOME_SUB_SPEED       0x02u  //   sub2: speed           (int32, steps/s)
#define UC2_OD_HOME_SUB_MAXSPEED    0x03u  //   sub3: max speed       (uint32, steps/s)
#define UC2_OD_HOME_SUB_POLARITY    0x04u  //   sub4: endstop polarity (uint8, 0=NO 1=NC)
#define UC2_OD_HOME_SUB_RETRACT     0x05u  //   sub5: retract distance (uint16, steps)
#define UC2_OD_HOME_SUB_OFFSET      0x06u  //   sub6: end offset       (int32, steps)
#define UC2_OD_HOME_SUB_TIMEOUT     0x07u  //   sub7: timeout          (uint32, ms)

// Galvo scanner control
#define UC2_OD_GALVO_CTRL       0x2900u  // galvo control object
#define UC2_OD_GALVO_SUB_TRIGGER  0x01u  //   sub1: trigger_mode  (uint8)
#define UC2_OD_GALVO_SUB_NPOINTS  0x02u  //   sub2: point count   (uint16)
// sub3+ reserved for segmented SDO point array download

// TMC stepper driver config per axis (0x2A00 + axis)
#define UC2_OD_TMC_BASE           0x2A00u  // 0x2A00+axis: TMC config object
#define UC2_OD_TMC_SUB_MSTEPS      0x01u  //   sub1: microsteps     (uint16)
#define UC2_OD_TMC_SUB_RMS_CUR     0x02u  //   sub2: rms_current    (uint16, mA)
#define UC2_OD_TMC_SUB_STALL       0x03u  //   sub3: stall_value    (uint16)
#define UC2_OD_TMC_SUB_SGTHRS      0x04u  //   sub4: stall guard thr (uint16)
#define UC2_OD_TMC_SUB_BLANK       0x05u  //   sub5: blank_time     (uint8)
#define UC2_OD_TMC_SUB_TOFF        0x06u  //   sub6: toff           (uint8)

// ============================================================================
// Node capability bitmask (for UC2_OD_NODE_CAPS)
// ============================================================================
#define UC2_CAP_MOTOR   (1u << 0)
#define UC2_CAP_LASER   (1u << 1)
#define UC2_CAP_LED     (1u << 2)
#define UC2_CAP_ENCODER (1u << 3)
#define UC2_CAP_GALVO   (1u << 4)  // replaces former UC2_CAP_HEATER (no CANopen heater device)
#define UC2_CAP_DAC     (1u << 5)
#define UC2_CAP_DOUT    (1u << 6)
#define UC2_CAP_DIN     (1u << 7)

// ============================================================================
// Motor command byte (UC2_OD_MOTOR_SUB_CMD)
// ============================================================================
#define UC2_MOTOR_CMD_STOP      0u
#define UC2_MOTOR_CMD_MOVE_REL  1u  // relative steps
#define UC2_MOTOR_CMD_MOVE_ABS  2u  // absolute position
#define UC2_MOTOR_CMD_HOME      3u  // homing sequence

// ============================================================================
// NMT state values (transmitted in heartbeat frame data byte)
// ============================================================================
#define UC2_NMT_STATE_INITIALIZING    0x00u
#define UC2_NMT_STATE_PRE_OPERATIONAL 0x7Fu
#define UC2_NMT_STATE_OPERATIONAL     0x05u
#define UC2_NMT_STATE_STOPPED         0x04u

// ============================================================================
// NMT command bytes (broadcast at COB-ID 0x000, data[0]=cmd, data[1]=nodeId)
// ============================================================================
#define UC2_NMT_CMD_START           0x01u
#define UC2_NMT_CMD_STOP            0x02u
#define UC2_NMT_CMD_PRE_OPERATIONAL 0x80u
#define UC2_NMT_CMD_RESET_NODE      0x81u
#define UC2_NMT_CMD_RESET_COMM      0x82u

// ============================================================================
// Standard CANopen CAN-ID base addresses
// ============================================================================
#define UC2_COBID_NMT               0x000u
#define UC2_COBID_SYNC              0x080u
#define UC2_COBID_TPDO1_BASE        0x180u
#define UC2_COBID_RPDO1_BASE        0x200u
#define UC2_COBID_TPDO2_BASE        0x280u
#define UC2_COBID_RPDO2_BASE        0x300u
#define UC2_COBID_TPDO3_BASE        0x380u
#define UC2_COBID_RPDO3_BASE        0x400u
#define UC2_COBID_SDO_RESP_BASE     0x580u  // slave  → master (SDO response)
#define UC2_COBID_SDO_REQ_BASE      0x600u  // master → slave  (SDO request)
#define UC2_COBID_HEARTBEAT_BASE    0x700u

// ============================================================================
// PDO payload structs — all 8 bytes, little-endian, packed
// ============================================================================

// RPDO1 (COB-ID 0x200+nodeId): master → slave, motor position + speed command
// Usage: one per axis move; send UC2_RPDO2_Control.cmd separately to trigger
#pragma pack(push, 1)
struct UC2_RPDO1_MotorPos {
    int32_t  target_pos;   // target position (steps; relative or absolute)
    int32_t  speed;        // max speed in steps/s (0 = use slave default)
};
#pragma pack(pop)

// RPDO2 (COB-ID 0x300+nodeId): master → slave, motor trigger + laser/LED control
// axis=0xFF → laser-only command; axis=0xFE → LED-only command
#pragma pack(push, 1)
struct UC2_RPDO2_Control {
    uint8_t  axis;         // motor axis (0-9), 0xFF=laser cmd, 0xFE=LED cmd
    uint8_t  cmd;          // UC2_MOTOR_CMD_* (or 0 when axis=0xFF/0xFE)
    uint8_t  laser_id;     // laser channel ID
    uint16_t laser_pwm;    // laser PWM 0–1023
    uint8_t  led_mode;     // LedMode enum (0=off, 1=fill, 2=single, ...)
    uint8_t  r;            // LED red
    uint8_t  g;            // LED green
    // LED blue + led_index go in RPDO3
};
#pragma pack(pop)

// RPDO3 (COB-ID 0x400+nodeId): master → slave, LED extended data + QID
#pragma pack(push, 1)
struct UC2_RPDO3_LEDExtra {
    uint8_t  b;            // LED blue
    uint16_t led_index;    // LED pixel index (single mode)
    int16_t  qid;          // QID for acknowledgement (-1 = no ack needed)
    uint8_t  _pad[3];
};
#pragma pack(pop)

// TPDO1 (COB-ID 0x180+nodeId): slave → master, motor status (sent on change)
#pragma pack(push, 1)
struct UC2_TPDO1_MotorStatus {
    int32_t  actual_pos;   // current position in steps
    uint8_t  axis;         // which axis this is for
    uint8_t  is_running;   // 1=moving, 0=idle
    int16_t  qid;          // qid that triggered the last completed move
};
#pragma pack(pop)

// TPDO2 (COB-ID 0x280+nodeId): slave → master, node status (sent periodically)
#pragma pack(push, 1)
struct UC2_TPDO2_NodeStatus {
    uint8_t  capabilities; // UC2_CAP_* bitmask
    uint8_t  node_id;      // own node-ID (for self-identification at boot)
    uint8_t  error_reg;    // error flags (0 = no error)
    int16_t  temperature;  // °C × 10 (0x7FFF = no sensor)
    int16_t  encoder_pos;  // encoder position axis-0 (truncated; use SDO for full)
    uint8_t  nmt_state;    // own NMT state (UC2_NMT_STATE_*)
};
#pragma pack(pop)

// TPDO3 (COB-ID 0x380+nodeId): slave → master, extended sensor data
#pragma pack(push, 1)
struct UC2_TPDO3_SensorData {
    int32_t  encoder_pos_hi; // full int32 encoder position (axis 0)
    uint16_t adc_raw;        // raw ADC value (first channel)
    uint8_t  din_state;      // digital-input bitmask (8 inputs)
    uint8_t  dout_state;     // current digital-output bitmask
};
#pragma pack(pop)

// Compile-time size guards
static_assert(sizeof(UC2_RPDO1_MotorPos)    == 8, "RPDO1 must be 8 bytes");
static_assert(sizeof(UC2_RPDO2_Control)     == 8, "RPDO2 must be 8 bytes");
static_assert(sizeof(UC2_RPDO3_LEDExtra)    == 8, "RPDO3 must be 8 bytes");
static_assert(sizeof(UC2_TPDO1_MotorStatus) == 8, "TPDO1 must be 8 bytes");
static_assert(sizeof(UC2_TPDO2_NodeStatus)  == 8, "TPDO2 must be 8 bytes");
static_assert(sizeof(UC2_TPDO3_SensorData)  == 8, "TPDO3 must be 8 bytes");
