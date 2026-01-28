/**
 * @file CanOtaTypes.h
 * @brief Type definitions for CAN-based OTA firmware updates
 * 
 * This header defines the data structures and constants used for
 * Over-The-Air (OTA) firmware updates via CAN bus using ISO-TP protocol.
 * 
 * The CAN OTA protocol works by:
 * 1. Python script reads firmware binary and calculates MD5 hash
 * 2. Sends OTA_CAN_START command with firmware metadata via serial to master
 * 3. Master relays command to slave via CAN ISO-TP
 * 4. Firmware is split into chunks (2KB default) and sent sequentially
 * 5. Each chunk is CRC32 verified before writing to flash
 * 6. After all chunks received, full MD5 verification is performed
 * 7. On success, slave reboots into new firmware
 */

#pragma once
#include <stdint.h>

// ============================================================================
// Configuration Constants
// ============================================================================

/**
 * @brief Default chunk size for OTA data transfer
 * 
 * 2KB is chosen as a balance between:
 * - ISO-TP efficiency (larger chunks = less overhead)
 * - Memory usage on ESP32 (must fit in RAM)
 * - Error recovery granularity (smaller chunks = faster retry)
 */
#define CAN_OTA_CHUNK_SIZE          2048

/**
 * @brief Maximum firmware size supported
 * Based on partition table: app0/app1 are 0x140000 (1.25MB) each
 */
#define CAN_OTA_MAX_FIRMWARE_SIZE   0x140000

/**
 * @brief Timeout for ACK response in milliseconds
 */
#define CAN_OTA_ACK_TIMEOUT_MS      5000

/**
 * @brief Maximum retry attempts for a single chunk
 */
#define CAN_OTA_MAX_RETRIES         3

/**
 * @brief MD5 hash length in bytes
 */
#define CAN_OTA_MD5_LENGTH          16

// ============================================================================
// Message Types (add to CANMessageTypeID enum in can_messagetype.h)
// ============================================================================

/**
 * @brief CAN OTA specific message type IDs
 * These should be added to the CANMessageTypeID enum
 */
enum CANOtaMessageType {
    OTA_CAN_START    = 0x62,  ///< Start CAN-based OTA update
    OTA_CAN_DATA     = 0x63,  ///< Firmware data chunk
    OTA_CAN_VERIFY   = 0x64,  ///< Request MD5 verification
    OTA_CAN_FINISH   = 0x65,  ///< Finalize and reboot
    OTA_CAN_ABORT    = 0x66,  ///< Abort OTA process
    OTA_CAN_ACK      = 0x67,  ///< Positive acknowledgment
    OTA_CAN_NAK      = 0x68,  ///< Negative acknowledgment (error)
    OTA_CAN_STATUS   = 0x69,  ///< Status query/response
};

// ============================================================================
// Status Codes
// ============================================================================

/**
 * @brief OTA operation status codes
 */
enum CANOtaStatus {
    CAN_OTA_OK                  = 0x00,  ///< Success
    CAN_OTA_ERR_BUSY            = 0x01,  ///< OTA already in progress
    CAN_OTA_ERR_INVALID_SIZE    = 0x02,  ///< Firmware size invalid
    CAN_OTA_ERR_PARTITION       = 0x03,  ///< Partition error
    CAN_OTA_ERR_BEGIN           = 0x04,  ///< Update.begin() failed
    CAN_OTA_ERR_WRITE           = 0x05,  ///< Flash write error
    CAN_OTA_ERR_CRC             = 0x06,  ///< CRC mismatch
    CAN_OTA_ERR_SEQUENCE        = 0x07,  ///< Chunk sequence error
    CAN_OTA_ERR_MD5             = 0x08,  ///< MD5 verification failed
    CAN_OTA_ERR_END             = 0x09,  ///< Update.end() failed
    CAN_OTA_ERR_TIMEOUT         = 0x0A,  ///< Operation timeout
    CAN_OTA_ERR_ABORTED         = 0x0B,  ///< OTA was aborted
    CAN_OTA_ERR_MEMORY          = 0x0C,  ///< Memory allocation failed
    CAN_OTA_ERR_NOT_STARTED     = 0x0D,  ///< OTA not started
};

// ============================================================================
// Data Structures
// ============================================================================

/**
 * @brief OTA Start command structure
 * Sent from master to slave to initiate CAN OTA process
 */
typedef struct __attribute__((packed)) {
    uint32_t firmwareSize;              ///< Total firmware size in bytes
    uint32_t totalChunks;               ///< Total number of chunks
    uint16_t chunkSize;                 ///< Size of each chunk (default 2048)
    uint8_t md5Hash[CAN_OTA_MD5_LENGTH]; ///< MD5 hash of entire firmware
} CanOtaStartCommand;

/**
 * @brief OTA Data chunk header
 * Precedes the actual chunk data in OTA_CAN_DATA messages
 */
typedef struct __attribute__((packed)) {
    uint16_t chunkIndex;                ///< Chunk index (0-based)
    uint16_t chunkSize;                 ///< Actual size of this chunk's data
    uint32_t crc32;                     ///< CRC32 of the chunk data
    // uint8_t data[] follows this header (variable length)
} CanOtaDataHeader;

/**
 * @brief OTA Acknowledgment structure
 * Sent from slave to master after receiving a command/chunk
 */
typedef struct __attribute__((packed)) {
    uint8_t status;                     ///< CANOtaStatus code
    uint8_t canId;                      ///< Responding device's CAN ID
    uint16_t expectedChunk;             ///< Next expected chunk index
    uint32_t bytesReceived;             ///< Total bytes received so far
    uint32_t crc32Received;             ///< CRC32 of last received chunk (for debugging)
} CanOtaAck;

/**
 * @brief OTA Negative Acknowledgment structure
 * Sent when an error occurs during OTA
 */
typedef struct __attribute__((packed)) {
    uint8_t status;                     ///< CANOtaStatus error code
    uint8_t canId;                      ///< Responding device's CAN ID
    uint16_t errorChunk;                ///< Chunk index that caused error (if applicable)
    uint32_t expectedCrc;               ///< Expected CRC (for CRC errors)
    uint32_t receivedCrc;               ///< Received CRC (for CRC errors)
} CanOtaNak;

/**
 * @brief OTA Status response structure
 * Response to OTA_CAN_STATUS query
 */
typedef struct __attribute__((packed)) {
    uint8_t isActive;                   ///< 1 if OTA in progress, 0 otherwise
    uint8_t canId;                      ///< Device CAN ID
    uint16_t currentChunk;              ///< Current chunk being processed
    uint16_t totalChunks;               ///< Total chunks expected
    uint32_t bytesReceived;             ///< Bytes received so far
    uint32_t bytesTotal;                ///< Total bytes expected
    uint8_t progressPercent;            ///< Progress 0-100%
} CanOtaStatusResponse;

/**
 * @brief OTA Verify command structure
 * Sent to request final MD5 verification
 */
typedef struct __attribute__((packed)) {
    uint8_t md5Hash[CAN_OTA_MD5_LENGTH]; ///< Expected MD5 hash
} CanOtaVerifyCommand;

// ============================================================================
// State Machine
// ============================================================================

/**
 * @brief CAN OTA state machine states
 */
enum CanOtaState {
    CAN_OTA_STATE_IDLE,         ///< No OTA in progress
    CAN_OTA_STATE_STARTED,      ///< OTA started, waiting for first chunk
    CAN_OTA_STATE_RECEIVING,    ///< Receiving chunks
    CAN_OTA_STATE_VERIFYING,    ///< All chunks received, verifying MD5
    CAN_OTA_STATE_FINISHING,    ///< Verification passed, finalizing
    CAN_OTA_STATE_ERROR,        ///< Error state
};

/**
 * @brief CAN OTA context structure
 * Holds all state information for an ongoing OTA update
 */
typedef struct {
    CanOtaState state;                      ///< Current state
    uint32_t firmwareSize;                  ///< Expected firmware size
    uint32_t totalChunks;                   ///< Total expected chunks
    uint16_t chunkSize;                     ///< Chunk size
    uint32_t nextExpectedChunk;             ///< Next chunk index expected
    uint32_t bytesReceived;                 ///< Bytes written to flash
    uint8_t expectedMd5[CAN_OTA_MD5_LENGTH]; ///< Expected MD5 hash
    uint32_t startTime;                     ///< Timestamp when OTA started
    uint32_t lastChunkTime;                 ///< Timestamp of last chunk received
    uint8_t retryCount;                     ///< Current retry count
} CanOtaContext;

// ============================================================================
// Helper Macros
// ============================================================================

/**
 * @brief Calculate number of chunks needed for a firmware size
 */
#define CAN_OTA_CALC_CHUNKS(size, chunk_size) \
    (((size) + (chunk_size) - 1) / (chunk_size))

/**
 * @brief Check if chunk index is valid
 */
#define CAN_OTA_VALID_CHUNK(index, total) \
    ((index) < (total))

/**
 * @brief Calculate progress percentage
 */
#define CAN_OTA_PROGRESS(received, total) \
    ((total) > 0 ? ((received) * 100 / (total)) : 0)
