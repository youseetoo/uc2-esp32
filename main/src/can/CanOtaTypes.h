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

// Forward declare enum from can_messagetype.h to avoid circular includes
// OTA_CAN_START = 0x62, OTA_CAN_DATA = 0x63, etc.

/**
 * @brief Default chunk size for OTA data transfer
 * 
 * 1KB is chosen as a balance between:
 * - ISO-TP efficiency (larger chunks = less overhead)
 * - Memory usage on ESP32 (must fit in RAM)
 * - Serial/JSON reliability (smaller = less buffer issues)
 * - Error recovery granularity (smaller chunks = faster retry)
 * 
 * Note: With hex encoding, 1KB data = 2KB hex string = ~2.3KB JSON
 * This is within safe limits for serial and JSON parsing.
 */
#define CAN_OTA_CHUNK_SIZE          1024

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

// Message types are defined in can_messagetype.h as CANMessageTypeID enum values:
// OTA_CAN_START = 0x62, OTA_CAN_DATA = 0x63, OTA_CAN_VERIFY = 0x64,
// OTA_CAN_FINISH = 0x65, OTA_CAN_ABORT = 0x66, OTA_CAN_ACK = 0x67,
// OTA_CAN_NAK = 0x68, OTA_CAN_STATUS = 0x69

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
 * @brief OTA start command structure
 * 
 * Sent to initiate OTA process. Contains firmware metadata.
 */
struct CanOtaStartCommand {
    uint32_t firmwareSize;              ///< Total firmware size in bytes
    uint32_t totalChunks;               ///< Total number of chunks
    uint16_t chunkSize;                 ///< Size of each chunk (except last)
    uint8_t  md5Hash[CAN_OTA_MD5_LENGTH]; ///< MD5 hash of complete firmware
} __attribute__((packed));

/**
 * @brief OTA data chunk header
 * 
 * Precedes the actual firmware data in OTA_CAN_DATA messages.
 */
struct CanOtaDataHeader {
    uint16_t chunkIndex;                ///< Sequential chunk index (0-based)
    uint16_t chunkSize;                 ///< Size of data following this header
    uint32_t crc32;                     ///< CRC32 of chunk data
} __attribute__((packed));

/**
 * @brief OTA verification command
 * 
 * Requests slave to verify the complete received firmware.
 */
struct CanOtaVerifyCommand {
    uint8_t md5Hash[CAN_OTA_MD5_LENGTH]; ///< Expected MD5 hash
} __attribute__((packed));

/**
 * @brief OTA acknowledgment response (positive)
 * 
 * Sent by slave to confirm successful operation.
 */
struct CanOtaAck {
    uint8_t  status;                    ///< Status code (CAN_OTA_OK = success)
    uint8_t  canId;                     ///< Sender's CAN ID
    uint32_t expectedChunk;             ///< Next expected chunk index
    uint32_t bytesReceived;             ///< Total bytes received so far
    uint32_t crc32Received;             ///< CRC32 of last received chunk
} __attribute__((packed));

/**
 * @brief OTA negative acknowledgment (error response)
 * 
 * Sent by slave to report an error condition.
 */
struct CanOtaNak {
    uint8_t  status;                    ///< Error code (see CANOtaStatus)
    uint8_t  canId;                     ///< Sender's CAN ID
    uint16_t errorChunk;                ///< Chunk index where error occurred
    uint32_t expectedCrc;               ///< Expected CRC32 value
    uint32_t receivedCrc;               ///< Actually received CRC32 value
} __attribute__((packed));

/**
 * @brief OTA status response
 * 
 * Provides current status of OTA operation.
 */
struct CanOtaStatusResponse {
    uint8_t  isActive;                  ///< 1 if OTA in progress, 0 if idle
    uint8_t  canId;                     ///< Sender's CAN ID
    uint32_t currentChunk;              ///< Current chunk being processed
    uint32_t totalChunks;               ///< Total chunks expected
    uint32_t bytesReceived;             ///< Bytes received so far
    uint32_t bytesTotal;                ///< Total bytes expected
    uint8_t  progressPercent;           ///< Progress percentage (0-100)
} __attribute__((packed));

// ============================================================================
// State Management
// ============================================================================

/**
 * @brief OTA state machine states
 */
enum CanOtaState {
    CAN_OTA_STATE_IDLE       = 0,       ///< No OTA in progress
    CAN_OTA_STATE_STARTED    = 1,       ///< OTA started, waiting for data
    CAN_OTA_STATE_RECEIVING  = 2,       ///< Receiving data chunks
    CAN_OTA_STATE_VERIFYING  = 3,       ///< All data received, verifying
    CAN_OTA_STATE_FINISHING  = 4,       ///< Verification passed, ready to reboot
    CAN_OTA_STATE_ERROR      = 5,       ///< Error occurred, need to abort
};

/**
 * @brief OTA context structure
 * 
 * Maintains state during OTA operation.
 */
struct CanOtaContext {
    CanOtaState state;                  ///< Current state
    uint32_t firmwareSize;              ///< Total firmware size
    uint32_t totalChunks;               ///< Expected number of chunks
    uint16_t chunkSize;                 ///< Size of each chunk
    uint32_t nextExpectedChunk;         ///< Next chunk index to receive
    uint32_t bytesReceived;             ///< Bytes received so far
    uint8_t  expectedMd5[CAN_OTA_MD5_LENGTH]; ///< Expected MD5 hash
    uint32_t startTime;                 ///< Timestamp when OTA started
    uint32_t lastChunkTime;             ///< Timestamp of last received chunk
    uint8_t  retryCount;                ///< Number of retries for current chunk
};

// ============================================================================
// Helper Macros
// ============================================================================

/**
 * @brief Calculate number of chunks needed
 */
#define CAN_OTA_CALC_CHUNKS(size, chunkSize) (((size) + (chunkSize) - 1) / (chunkSize))

/**
 * @brief Calculate progress percentage
 */
#define CAN_OTA_PROGRESS(received, total) ((uint8_t)(((received) * 100) / (total)))