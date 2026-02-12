/*
 * ==============================
 * MessageType ID Table
 * ==============================
 * Defines unique identifiers for various commands, requests, and responses 
 * in the CAN communication protocol. These IDs are used to distinguish 
 * message types for motors, lasers, LEDs, sensors, and system broadcasts.
 *
 * MessageType IDs:
 * - General Control and Status Messages
 *   HEARTBEAT        : 0x00  - Periodic node status check (alive)
 *   ERROR            : 0xF1  - Error reporting message
 *   SYNC             : 0xF2  - Synchronization command
 *
 * - Motor Control Messages
 *   MOTOR_ACT        : 0x10  - Start/stop motor or set parameters
 *   MOTOR_GET        : 0x11  - Request motor position/state
 *   MOTOR_STATE      : 0x12  - Response with motor position/state
 *   HOME_ACT         : 0x13  - Initiate homing process
 *   HOME_STATE       : 0x14  - Response with homing state
 *
 * - Laser Control Messages
 *   LASER_ACT        : 0x20  - Set laser intensity
 *   LASER_STATE      : 0x21  - Response with laser intensity
 *
 * - LED Control Messages
 *   LED_ACT          : 0x30  - Set LED state/intensity
 *   LED_STATE        : 0x31  - Response with LED state/intensity
 *
 * - Sensor Messages
 *   SENSOR_GET       : 0x40  - Request sensor data
 *   SENSOR_STATE     : 0x41  - Response with sensor data
 *
 * - Galvo Control Messages
 *   GALVO_ACT        : 0x50  - Set galvo scanning parameters
 *   GALVO_STATE      : 0x51  - Response with galvo state
 *
 * - Broadcast Messages
 *   BROADCAST        : 0xF0  - General broadcast messages (errors, updates)
 */

#ifndef CAN_MESSAGETYPE_H
#define CAN_MESSAGETYPE_H
 
// Enum to define all MessageType IDs
enum CANMessageTypeID {
    HEARTBEAT        = 0x00, // General Messages
    ERROR            = 0xF1,
    SYNC             = 0xF2,

    RESTART_CMD      = 0x01, // System restart command

    MOTOR_ACT        = 0x10, // Motor Messages - Full MotorData struct
    MOTOR_GET        = 0x11, // Request motor position/state
    MOTOR_STATE      = 0x12, // Response with motor position/state
    HOME_ACT         = 0x13, // Initiate homing process
    HOME_STATE       = 0x14, // Response with homing state
    MOTOR_ACT_REDUCED= 0x15, // MotorDataReduced (compact motor command)
    SOFT_LIMIT_SET   = 0x16, // Soft limits configuration
    MOTOR_SETTINGS   = 0x17, // Motor configuration settings (MotorSettings struct)
    MOTOR_SINGLE_VAL = 0x18, // Single value motor data update (MotorDataValueUpdate)
    TMC_ACT          = 0x19, // TMC driver configuration (TMCData struct)
    STOP_HOME        = 0x1A, // Stop homing command (StopHomeCommand struct)

    LASER_ACT        = 0x20, // Laser Messages
    LASER_STATE      = 0x21,

    LED_ACT          = 0x30, // LED Messages
    LED_STATE        = 0x31,

    SENSOR_GET       = 0x40, // Sensor Messages
    SENSOR_STATE     = 0x41,

    GALVO_ACT        = 0x50, // Galvo Messages
    GALVO_STATE      = 0x51,

    OTA_START        = 0x60, // OTA Update Messages (WiFi-based)
    OTA_ACK          = 0x61,

    // CAN OTA Direct Update Messages (no WiFi required)
    OTA_CAN_START    = 0x62, // Start CAN-based OTA update
    OTA_CAN_DATA     = 0x63, // Firmware data chunk
    OTA_CAN_VERIFY   = 0x64, // Request MD5 verification
    OTA_CAN_FINISH   = 0x65, // Finalize and reboot
    OTA_CAN_ABORT    = 0x66, // Abort OTA process
    OTA_CAN_ACK      = 0x67, // Positive acknowledgment
    OTA_CAN_NAK      = 0x68, // Negative acknowledgment
    OTA_CAN_STATUS   = 0x69, // Status query/response

    SCAN_REQUEST     = 0x80, // Network Scan Messages
    SCAN_RESPONSE    = 0x81,

    BROADCAST        = 0xF0,  // Broadcast Message (240)

    OTA_STREAM_START = 0x70, // Start OTA streaming session (defined in CanOtaStreaming.h)
    OTA_STREAM_DATA  = 0x71, // Data chunk in OTA streaming mode
    OTA_STREAM_ACK   = 0x72, // ACK for OTA streaming
    OTA_STREAM_NAK   = 0x73, // NAK for OTA streaming
    OTA_STREAM_FINISH= 0x74, // Finish OTA streaming session
    OTA_STREAM_ABORT = 0x75, // Abort OTA streaming session
    OTA_STREAM_STATUS= 0x76  // Status query for OTA streaming session

};

#endif // CAN_MESSAGETYPE_H