/**
 * @file canopen_types.h
 * @brief CANopen protocol data types and structures for UC2-ESP32
 * 
 * This file defines CANopen-compliant data structures following CiA 301 specification.
 * It provides a standardized approach to CAN communication while maintaining
 * backward compatibility with the existing UC2 custom protocol.
 * 
 * References:
 * - CiA 301: CANopen Application Layer and Communication Profile
 * - CiA 402: CANopen Device Profile for Drives and Motion Control
 */

#pragma once

#include <stdint.h>

// ============================================================================
// CANopen COB-ID Function Codes (bits 7-10 of CAN identifier)
// ============================================================================

#define CANOPEN_FUNC_NMT        0x000  // Network management (broadcast)
#define CANOPEN_FUNC_SYNC       0x080  // Synchronization object (fixed COB-ID)
#define CANOPEN_FUNC_EMCY       0x080  // Emergency (0x080 + Node-ID, range: 0x081-0x0FF)
                                       // Note: EMCY and SYNC share base 0x080, but SYNC is fixed
                                       // while EMCY uses Node-ID offset (0x080 + 1 to 127)
#define CANOPEN_FUNC_TPDO1      0x180  // Transmit PDO 1 (+ Node-ID)
#define CANOPEN_FUNC_RPDO1      0x200  // Receive PDO 1 (+ Node-ID)
#define CANOPEN_FUNC_TPDO2      0x280  // Transmit PDO 2 (+ Node-ID)
#define CANOPEN_FUNC_RPDO2      0x300  // Receive PDO 2 (+ Node-ID)
#define CANOPEN_FUNC_TPDO3      0x380  // Transmit PDO 3 (+ Node-ID)
#define CANOPEN_FUNC_RPDO3      0x400  // Receive PDO 3 (+ Node-ID)
#define CANOPEN_FUNC_TPDO4      0x480  // Transmit PDO 4 (+ Node-ID)
#define CANOPEN_FUNC_RPDO4      0x500  // Receive PDO 4 (+ Node-ID)
#define CANOPEN_FUNC_SDO_TX     0x580  // SDO response (+ Node-ID)
#define CANOPEN_FUNC_SDO_RX     0x600  // SDO request (+ Node-ID)
#define CANOPEN_FUNC_HEARTBEAT  0x700  // Node heartbeat (+ Node-ID)

// ============================================================================
// NMT (Network Management) Protocol
// ============================================================================

/**
 * @brief NMT command specifiers (sent from master to nodes)
 */
typedef enum {
    NMT_CMD_START_REMOTE_NODE       = 0x01,  // Enter Operational state
    NMT_CMD_STOP_REMOTE_NODE        = 0x02,  // Enter Stopped state
    NMT_CMD_ENTER_PREOPERATIONAL    = 0x80,  // Enter Pre-operational state
    NMT_CMD_RESET_NODE              = 0x81,  // Software reset
    NMT_CMD_RESET_COMMUNICATION     = 0x82   // Reset communication parameters
} CANopen_NMT_Command;

/**
 * @brief NMT node states
 */
typedef enum {
    NMT_STATE_BOOTUP          = 0x00,  // Initialization/boot-up
    NMT_STATE_STOPPED         = 0x04,  // Stopped (no PDOs, only NMT/heartbeat)
    NMT_STATE_OPERATIONAL     = 0x05,  // Normal operation (PDOs enabled)
    NMT_STATE_PREOPERATIONAL  = 0x7F   // Configuration mode (SDOs allowed)
} CANopen_NMT_State;

/**
 * @brief NMT message structure (2 bytes)
 * COB-ID: 0x000 (broadcast from master)
 */
typedef struct __attribute__((packed)) {
    uint8_t command;   // NMT command (CANopen_NMT_Command)
    uint8_t nodeId;    // Target node ID (0 = all nodes)
} CANopen_NMT_Message;

// ============================================================================
// Heartbeat Protocol
// ============================================================================

/**
 * @brief Heartbeat/Boot-up message (1 byte)
 * COB-ID: 0x700 + Node-ID
 * Sent periodically by each node to indicate it's alive
 */
typedef struct __attribute__((packed)) {
    uint8_t state;     // Current NMT state (CANopen_NMT_State)
} CANopen_Heartbeat;

// ============================================================================
// Emergency (EMCY) Messages
// ============================================================================

/**
 * @brief Emergency error codes (CiA 301 standard + UC2-specific)
 */
typedef enum {
    EMCY_NO_ERROR                    = 0x0000,
    EMCY_GENERIC_ERROR               = 0x1000,
    EMCY_CURRENT_ERROR               = 0x2000,
    EMCY_VOLTAGE_ERROR               = 0x3000,
    EMCY_TEMPERATURE_ERROR           = 0x4000,
    EMCY_COMMUNICATION_ERROR         = 0x5000,
    EMCY_DEVICE_PROFILE_SPECIFIC     = 0x6000,
    
    // UC2-specific motor errors (0x2300 range)
    EMCY_MOTOR_POSITION_LIMIT        = 0x2300,
    EMCY_MOTOR_OVER_TEMP             = 0x2310,
    EMCY_MOTOR_DRIVE_ERROR           = 0x2320,
    EMCY_MOTOR_ENCODER_ERROR         = 0x2330,
    
    // UC2-specific laser errors (0x3000 range)
    EMCY_LASER_SAFETY_INTERLOCK      = 0x3000,
    EMCY_LASER_OVER_POWER            = 0x3010,
    
    // UC2-specific LED errors (0x4000 range)
    EMCY_LED_CONTROLLER_ERROR        = 0x4000,
    EMCY_LED_OVER_CURRENT            = 0x4010,
    
    // UC2-specific sensor errors (0x6000 range)
    EMCY_SENSOR_FAILURE              = 0x6000,
    EMCY_SENSOR_OUT_OF_RANGE         = 0x6010
} CANopen_EMCY_ErrorCode;

/**
 * @brief Error register bits (CiA 301)
 */
typedef enum {
    EMCY_ERR_REG_GENERIC       = 0x01,  // Generic error
    EMCY_ERR_REG_CURRENT       = 0x02,  // Current error
    EMCY_ERR_REG_VOLTAGE       = 0x04,  // Voltage error
    EMCY_ERR_REG_TEMPERATURE   = 0x08,  // Temperature error
    EMCY_ERR_REG_COMMUNICATION = 0x10,  // Communication error
    EMCY_ERR_REG_DEVICE_PROFILE= 0x20,  // Device profile specific error
    EMCY_ERR_REG_RESERVED      = 0x40,  // Reserved
    EMCY_ERR_REG_MANUFACTURER  = 0x80   // Manufacturer specific error
} CANopen_ErrorRegister;

/**
 * @brief Emergency message structure (8 bytes)
 * COB-ID: 0x080 + Node-ID
 */
typedef struct __attribute__((packed)) {
    uint16_t errorCode;           // Emergency error code (CANopen_EMCY_ErrorCode)
    uint8_t errorRegister;        // Error register bitmap
    uint8_t mfrSpecific[5];       // Manufacturer-specific error info
} CANopen_EMCY_Message;

// ============================================================================
// SDO (Service Data Object) Protocol
// ============================================================================

/**
 * @brief SDO command specifiers (Client Command Specifier - CCS)
 */
typedef enum {
    SDO_CCS_DOWNLOAD_INITIATE      = 0x20,  // Write request (1-4 bytes)
    SDO_CCS_DOWNLOAD_SEGMENT       = 0x00,  // Write segment (block transfer)
    SDO_CCS_UPLOAD_INITIATE        = 0x40,  // Read request
    SDO_CCS_UPLOAD_SEGMENT         = 0x60,  // Read segment (block transfer)
    SDO_CCS_ABORT                  = 0x80   // Abort transfer
} CANopen_SDO_CCS;

/**
 * @brief SDO server response specifiers (Server Command Specifier - SCS)
 */
typedef enum {
    SDO_SCS_DOWNLOAD_RESPONSE      = 0x60,  // Write confirmation
    SDO_SCS_UPLOAD_RESPONSE        = 0x40,  // Read response (expedited)
    SDO_SCS_ABORT                  = 0x80   // Abort transfer
} CANopen_SDO_SCS;

/**
 * @brief SDO abort codes (CiA 301)
 */
typedef enum {
    SDO_ABORT_TOGGLE_BIT           = 0x05030000,  // Toggle bit error
    SDO_ABORT_TIMEOUT              = 0x05040000,  // SDO protocol timeout
    SDO_ABORT_INVALID_CS           = 0x05040001,  // Invalid command specifier
    SDO_ABORT_INVALID_BLOCK_SIZE   = 0x05040002,  // Invalid block size
    SDO_ABORT_INVALID_SEQ_NUM      = 0x05040003,  // Invalid sequence number
    SDO_ABORT_OUT_OF_MEMORY        = 0x05040005,  // Out of memory
    SDO_ABORT_UNSUPPORTED_ACCESS   = 0x06010000,  // Unsupported access
    SDO_ABORT_WRITE_ONLY           = 0x06010001,  // Write-only object
    SDO_ABORT_READ_ONLY            = 0x06010002,  // Read-only object
    SDO_ABORT_OBJECT_NOT_EXIST     = 0x06020000,  // Object does not exist
    SDO_ABORT_UNMAPPABLE_OBJECT    = 0x06040041,  // Object cannot be mapped to PDO
    SDO_ABORT_PDO_LENGTH_EXCEEDED  = 0x06040042,  // PDO length exceeded
    SDO_ABORT_GENERAL_PARAM_ERROR  = 0x06040043,  // General parameter incompatibility
    SDO_ABORT_GENERAL_INTERNAL_ERROR = 0x06040047, // General internal incompatibility
    SDO_ABORT_HARDWARE_ERROR       = 0x06060000,  // Access failed due to hardware error
    SDO_ABORT_TYPE_MISMATCH        = 0x06070010,  // Data type mismatch
    SDO_ABORT_DATA_TOO_LONG        = 0x06070012,  // Data type length too high
    SDO_ABORT_DATA_TOO_SHORT       = 0x06070013,  // Data type length too low
    SDO_ABORT_SUBINDEX_NOT_EXIST   = 0x06090011,  // Sub-index does not exist
    SDO_ABORT_VALUE_RANGE_EXCEEDED = 0x06090030,  // Value range exceeded
    SDO_ABORT_VALUE_TOO_HIGH       = 0x06090031,  // Value too high
    SDO_ABORT_VALUE_TOO_LOW        = 0x06090032,  // Value too low
    SDO_ABORT_MAX_LESS_THAN_MIN    = 0x06090036,  // Maximum < minimum
    SDO_ABORT_GENERAL_ERROR        = 0x08000000,  // General error
    SDO_ABORT_DATA_TRANSFER_ERROR  = 0x08000020,  // Data cannot be transferred/stored
    SDO_ABORT_DATA_LOCAL_CONTROL   = 0x08000021,  // Data transfer error due to local control
    SDO_ABORT_DATA_DEVICE_STATE    = 0x08000022   // Data transfer error due to device state
} CANopen_SDO_AbortCode;

/**
 * @brief SDO message structure (8 bytes)
 * COB-ID: 0x600 + Node-ID (request), 0x580 + Node-ID (response)
 */
typedef struct __attribute__((packed)) {
    uint8_t commandSpecifier;  // Command/response specifier
    uint16_t index;            // Object dictionary index (little-endian)
    uint8_t subIndex;          // Object dictionary sub-index
    union {
        uint8_t data[4];       // Expedited data (1-4 bytes)
        uint32_t abortCode;    // SDO abort code (if CS = 0x80)
        uint32_t dataSize;     // Data size for segmented transfer
    };
} CANopen_SDO_Message;

// Helper macros for SDO expedited transfer
#define SDO_EXPEDITED_FLAG      0x02  // Bit 1: expedited transfer
#define SDO_SIZE_INDICATED      0x01  // Bit 0: size indicated
#define SDO_SIZE_1_BYTE         0x0C  // Bits 2-3: 1 byte of data
#define SDO_SIZE_2_BYTES        0x08  // Bits 2-3: 2 bytes of data
#define SDO_SIZE_3_BYTES        0x04  // Bits 2-3: 3 bytes of data
#define SDO_SIZE_4_BYTES        0x00  // Bits 2-3: 4 bytes of data

// ============================================================================
// PDO (Process Data Object) Protocol
// ============================================================================

/**
 * @brief PDO transmission type (from PDO communication parameter)
 */
typedef enum {
    PDO_TRANS_TYPE_SYNC_ACYCLIC  = 0x00,  // Synchronous, acyclic
    PDO_TRANS_TYPE_SYNC_1        = 0x01,  // Synchronous, every SYNC
    PDO_TRANS_TYPE_SYNC_2_240    = 0x02,  // Synchronous, every 2nd-240th SYNC
    PDO_TRANS_TYPE_RTR_SYNC      = 0xFC,  // RTR synchronous
    PDO_TRANS_TYPE_RTR_ASYNC     = 0xFD,  // RTR asynchronous
    PDO_TRANS_TYPE_EVENT_DRIVEN  = 0xFE,  // Event-driven (manufacturer-specific)
    PDO_TRANS_TYPE_RESERVED      = 0xFF   // Reserved
} CANopen_PDO_TransmissionType;

/**
 * @brief Motor Controller RPDO1 (Receive PDO - commands from master)
 * COB-ID: 0x200 + Node-ID
 * Maps to motor control commands
 */
typedef struct __attribute__((packed)) {
    int32_t targetPosition;    // Target position in steps
    uint32_t speed;            // Speed in steps/sec
} CANopen_Motor_RPDO1;

/**
 * @brief Motor Controller TPDO1 (Transmit PDO - status to master)
 * COB-ID: 0x180 + Node-ID
 * Maps to motor status
 */
typedef struct __attribute__((packed)) {
    int32_t currentPosition;   // Current position in steps
    uint8_t statusWord;        // Motor status bitmap
    uint8_t reserved[3];       // Reserved for alignment
} CANopen_Motor_TPDO1;

/**
 * @brief Motor status word bits (for TPDO1)
 */
typedef enum {
    MOTOR_STATUS_ENABLED       = 0x01,  // Motor is enabled
    MOTOR_STATUS_RUNNING       = 0x02,  // Motor is running
    MOTOR_STATUS_TARGET_REACHED= 0x04,  // Target position reached
    MOTOR_STATUS_HOMING_ACTIVE = 0x08,  // Homing in progress
    MOTOR_STATUS_ERROR         = 0x10,  // Error state
    MOTOR_STATUS_LIMIT_SWITCH  = 0x20,  // Limit switch active
    MOTOR_STATUS_EMERGENCY_STOP= 0x40,  // Emergency stop
    MOTOR_STATUS_WARNING       = 0x80   // Warning condition
} CANopen_Motor_StatusBits;

/**
 * @brief Laser Controller RPDO1 (Receive PDO - commands from master)
 * COB-ID: 0x200 + Node-ID
 */
typedef struct __attribute__((packed)) {
    uint16_t intensity;        // Laser intensity (0-65535)
    uint8_t laserId;           // Laser channel ID
    uint8_t enable;            // Enable/disable laser
    uint8_t reserved[4];       // Reserved for alignment
} CANopen_Laser_RPDO1;

/**
 * @brief LED Controller RPDO1 (Receive PDO - commands from master)
 * COB-ID: 0x200 + Node-ID
 */
typedef struct __attribute__((packed)) {
    uint8_t mode;              // LED mode (off, on, pattern)
    uint8_t red;               // Red channel (0-255)
    uint8_t green;             // Green channel (0-255)
    uint8_t blue;              // Blue channel (0-255)
    uint8_t brightness;        // Overall brightness (0-255)
    uint8_t reserved[3];       // Reserved for alignment
} CANopen_LED_RPDO1;

// ============================================================================
// Object Dictionary Indexes (CiA 301 + UC2 manufacturer-specific)
// ============================================================================

// Standard Communication Profile Area (0x1000-0x1FFF)
#define OD_DEVICE_TYPE              0x1000  // Device type
#define OD_ERROR_REGISTER           0x1001  // Error register
#define OD_PRODUCER_HEARTBEAT_TIME  0x1017  // Heartbeat interval (ms)
#define OD_IDENTITY_OBJECT          0x1018  // Identity object
#define OD_IDENTITY_VENDOR_ID       0x1018  // Sub 1: Vendor ID
#define OD_IDENTITY_PRODUCT_CODE    0x1018  // Sub 2: Product code
#define OD_IDENTITY_REVISION        0x1018  // Sub 3: Revision number
#define OD_IDENTITY_SERIAL          0x1018  // Sub 4: Serial number

// PDO Communication Parameters (0x1400-0x19FF)
#define OD_RPDO1_COMM_PARAM         0x1400  // RPDO1 communication parameters
#define OD_RPDO2_COMM_PARAM         0x1401  // RPDO2 communication parameters
#define OD_TPDO1_COMM_PARAM         0x1800  // TPDO1 communication parameters
#define OD_TPDO2_COMM_PARAM         0x1801  // TPDO2 communication parameters

// PDO Mapping Parameters (0x1600-0x1BFF)
#define OD_RPDO1_MAPPING            0x1600  // RPDO1 mapping
#define OD_RPDO2_MAPPING            0x1601  // RPDO2 mapping
#define OD_TPDO1_MAPPING            0x1A00  // TPDO1 mapping
#define OD_TPDO2_MAPPING            0x1A01  // TPDO2 mapping

// Manufacturer-Specific Profile Area (0x2000-0x5FFF)

// Motor Controller Objects (0x2000-0x2FFF)
#define OD_MOTOR_CONTROL            0x2000  // Motor enable/disable
#define OD_MOTOR_TARGET_POSITION    0x2001  // Target position
#define OD_MOTOR_CURRENT_POSITION   0x2002  // Current position
#define OD_MOTOR_SPEED              0x2003  // Motor speed
#define OD_MOTOR_ACCELERATION       0x2004  // Acceleration
#define OD_MOTOR_STATUS             0x2005  // Motor status
#define OD_HOMING_CONTROL           0x2010  // Homing control
#define OD_HOMING_SPEED             0x2011  // Homing speed
#define OD_HOMING_DIRECTION         0x2012  // Homing direction
#define OD_HOMING_STATUS            0x2013  // Homing status
#define OD_SOFT_LIMIT_ENABLE        0x2020  // Soft limit enable
#define OD_SOFT_LIMIT_MIN           0x2021  // Minimum position
#define OD_SOFT_LIMIT_MAX           0x2022  // Maximum position

// Laser Controller Objects (0x3000-0x3FFF)
#define OD_LASER_ENABLE             0x3000  // Laser enable
#define OD_LASER_INTENSITY          0x3001  // Laser intensity
#define OD_LASER_ID                 0x3002  // Laser channel ID

// LED Controller Objects (0x4000-0x4FFF)
#define OD_LED_MODE                 0x4000  // LED mode
#define OD_LED_COLOR_R              0x4001  // Red channel
#define OD_LED_COLOR_G              0x4002  // Green channel
#define OD_LED_COLOR_B              0x4003  // Blue channel
#define OD_LED_BRIGHTNESS           0x4004  // Brightness

// ============================================================================
// Device Type Identifiers (CiA 301)
// ============================================================================

#define CANOPEN_DEVICE_TYPE_GENERIC     0x00000000
#define CANOPEN_DEVICE_TYPE_MOTOR       0x00020192  // CiA 402 (Drives and motion control)
#define CANOPEN_DEVICE_TYPE_IO          0x00000401  // Generic I/O module

// UC2-specific device types (manufacturer-specific, 32-bit compliant)
#define UC2_DEVICE_TYPE_MASTER          0x00002E00  // UC2 Gateway/Master
#define UC2_DEVICE_TYPE_MOTOR           0x00002E01  // UC2 Motor controller
#define UC2_DEVICE_TYPE_LASER           0x00002E02  // UC2 Laser controller
#define UC2_DEVICE_TYPE_LED             0x00002E03  // UC2 LED controller
#define UC2_DEVICE_TYPE_GALVO           0x00002E04  // UC2 Galvo controller
#define UC2_DEVICE_TYPE_SENSOR          0x00002E05  // UC2 Sensor node

// ============================================================================
// Helper Functions (Inline)
// ============================================================================

/**
 * @brief Calculate COB-ID for a given function code and node ID
 */
static inline uint16_t canopen_getCobId(uint16_t functionCode, uint8_t nodeId)
{
    return functionCode + nodeId;
}

/**
 * @brief Extract node ID from COB-ID
 */
static inline uint8_t canopen_getNodeId(uint16_t cobId, uint16_t functionCode)
{
    return (uint8_t)(cobId - functionCode);
}

/**
 * @brief Check if a COB-ID matches a specific function code
 */
static inline bool canopen_isCobIdFunction(uint16_t cobId, uint16_t functionCode)
{
    // For functions with node-ID, check if cobId is in range
    if (functionCode == CANOPEN_FUNC_NMT || functionCode == CANOPEN_FUNC_SYNC) {
        return cobId == functionCode;
    }
    // For functions with node-ID (1-127), check range
    uint8_t nodeId = cobId - functionCode;
    return (nodeId >= 1 && nodeId <= 127);
}
