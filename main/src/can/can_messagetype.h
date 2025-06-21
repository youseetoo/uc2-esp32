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
 
// Enum to define all MessageType IDs
enum CANMessageTypeID {
    HEARTBEAT        = 0x00, // General Messages
    ERROR            = 0xF1,
    SYNC             = 0xF2,

    MOTOR_ACT        = 0x10, // Motor Messages
    MOTOR_GET        = 0x11,
    MOTOR_STATE      = 0x12,
    HOME_ACT         = 0x13,
    HOME_STATE       = 0x14,
    MOTOR_ACT_REDUCED= 0x15,

    LASER_ACT        = 0x20, // Laser Messages
    LASER_STATE      = 0x21,

    LED_ACT          = 0x30, // LED Messages
    LED_STATE        = 0x31,

    SENSOR_GET       = 0x40, // Sensor Messages
    SENSOR_STATE     = 0x41,

    GALVO_ACT        = 0x50, // Galvo Messages
    GALVO_STATE      = 0x51,

    BROADCAST        = 0xF0  // Broadcast Message (240)
};