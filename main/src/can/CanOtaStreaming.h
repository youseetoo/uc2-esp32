/**
 * @file CanOtaStreaming.h
 * @brief High-speed CAN OTA streaming protocol
 * 
 * This implements a windowed streaming protocol for faster OTA updates:
 * - 4KB page-based transfers (flash-aligned)
 * - Cumulative ACK per page (not per chunk)
 * - Async flash writes via separate task
 * - Ring buffer to decouple CAN RX from flash writes
 * 
 * Speed comparison (1MB firmware):
 * - Old protocol (ACK per 1KB): ~6 minutes
 * - New streaming (ACK per 4KB): ~1.5-2 minutes
 * 
 * Protocol:
 * 1. STREAM_START: Initialize with page size, window size
 * 2. STREAM_DATA: Continuous frames with seq number
 * 3. STREAM_ACK: Cumulative ACK after each page
 * 4. STREAM_FINISH: Final verification and commit
 */

#pragma once

#include <Arduino.h>
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#include <freertos/queue.h>
#include <freertos/semphr.h>
#include "CanOtaTypes.h"
#include "cJSON.h"

namespace can_ota_stream {

// ============================================================================
// Constants
// ============================================================================

// Page size: 4KB aligned to flash sector
constexpr size_t STREAM_PAGE_SIZE = 4096;

// Ring buffer: 2 pages allows one being written while other fills
constexpr size_t STREAM_RING_PAGES = 2;
constexpr size_t STREAM_RING_SIZE = STREAM_PAGE_SIZE * STREAM_RING_PAGES;

// Data frames per page (7 bytes payload per ISO-TP frame for classic CAN)
// 4096 / 7 = 585 frames per page, but we send in larger chunks via ISO-TP
constexpr size_t STREAM_CHUNK_SIZE = 512;  // Smaller chunks for better flow
constexpr size_t STREAM_CHUNKS_PER_PAGE = STREAM_PAGE_SIZE / STREAM_CHUNK_SIZE;

// ACK interval: Every page (4KB)
constexpr size_t STREAM_ACK_INTERVAL = STREAM_PAGE_SIZE;

// Timeouts
constexpr uint32_t STREAM_PAGE_TIMEOUT_MS = 30000;  // 30s per page
constexpr uint32_t STREAM_TOTAL_TIMEOUT_MS = 300000; // 5 min total

// Message types (using unused range 0x70-0x7F)
enum StreamMessageType : uint8_t {
    STREAM_START    = 0x70,  // Initialize streaming session
    STREAM_DATA     = 0x71,  // Data frame with sequence number
    STREAM_ACK      = 0x72,  // Cumulative acknowledgment
    STREAM_NAK      = 0x73,  // Request retransmit from offset
    STREAM_FINISH   = 0x74,  // End session, verify, commit
    STREAM_ABORT    = 0x75,  // Abort session
    STREAM_STATUS   = 0x76,  // Query status
};

static int CAN_OTA_ERROR_ABORTED = 0xFF;
// ============================================================================
// Data Structures
// ============================================================================

/**
 * @brief Stream start command
 */
struct StreamStartCmd {
    uint32_t firmwareSize;      // Total firmware size
    uint32_t totalPages;        // Number of 4KB pages
    uint16_t pageSize;          // Page size (should be 4096)
    uint16_t chunkSize;         // Chunk size within page
    uint8_t  md5Hash[16];       // MD5 of complete firmware
} __attribute__((packed));

/**
 * @brief Stream data header (prepended to each chunk)
 */
struct StreamDataHeader {
    uint16_t pageIndex;         // Current page (0-based)
    uint16_t offsetInPage;      // Byte offset within page
    uint16_t dataSize;          // Size of following data
    uint16_t seq;               // Global sequence number (for loss detection)
} __attribute__((packed));

/**
 * @brief Stream cumulative ACK
 */
struct StreamAck {
    uint8_t  status;            // 0 = OK
    uint8_t  canId;             // Sender CAN ID
    uint16_t lastCompletePage;  // Last fully received page (-1 if none)
    uint32_t bytesReceived;     // Total bytes received
    uint16_t nextExpectedSeq;   // Next expected sequence number
    uint16_t reserved;
} __attribute__((packed));

/**
 * @brief Stream NAK (request retransmit)
 */
struct StreamNak {
    uint8_t  status;            // Error code
    uint8_t  canId;
    uint16_t pageIndex;         // Page with missing data
    uint16_t missingOffset;     // Offset of first missing byte
    uint16_t missingSeq;        // First missing sequence number
} __attribute__((packed));

/**
 * @brief Streaming session context
 */
struct StreamContext {
    bool active;
    uint32_t firmwareSize;
    uint32_t totalPages;
    uint16_t pageSize;
    uint16_t chunkSize;
    uint8_t  expectedMd5[16];
    
    // Progress tracking
    uint16_t currentPage;
    uint32_t pageOffset;
    uint32_t bytesReceived;
    uint16_t nextExpectedSeq;
    
    // Timing
    uint32_t startTime;
    uint32_t lastDataTime;
    
    // Ring buffer
    uint8_t* ringBuffer;
    size_t   ringWritePos;
    size_t   ringReadPos;
    
    // Flash writer
    TaskHandle_t flashWriterTask;
    QueueHandle_t pageQueue;
    SemaphoreHandle_t bufferMutex;
    
    // Master info
    uint8_t masterCanId;
};

// ============================================================================
// Page Queue Entry
// ============================================================================

struct PageEntry {
    uint16_t pageIndex;
    uint8_t  data[STREAM_PAGE_SIZE];
    uint32_t crc32;
};

// ============================================================================
// Function Declarations
// ============================================================================

/**
 * @brief Initialize streaming OTA subsystem
 */
void init();

/**
 * @brief Handle incoming stream message
 */
void handleStreamMessage(uint8_t msgType, const uint8_t* data, size_t len, uint8_t sourceCanId);

/**
 * @brief Check if streaming is active
 */
bool isActive();

/**
 * @brief Get progress (0-100)
 */
uint8_t getProgress();

/**
 * @brief Abort streaming session
 */
void abort(uint8_t status);

/**
 * @brief Loop function (call from main loop)
 */
void loop();

int actFromJsonStreaming(cJSON* doc);
// ============================================================================
// Master-side functions
// ============================================================================

#ifdef CAN_SEND_COMMANDS

/**
 * @brief Start streaming session to slave
 */
int startStreamToSlave(uint8_t slaveId, uint32_t firmwareSize, const uint8_t* md5Hash);

/**
 * @brief Send data chunk in streaming mode
 * @return Bytes sent, or negative on error
 */
int sendStreamData(uint8_t slaveId, uint16_t pageIndex, uint16_t offset, 
                   const uint8_t* data, size_t len, uint16_t seq);

/**
 * @brief Wait for page ACK (non-blocking check)
 * @return 0 if ACK received, 1 if still waiting, negative on NAK/error
 */
int checkStreamAck(uint16_t expectedPage, uint32_t timeoutMs);

/**
 * @brief Finish streaming and request verification
 */
int finishStream(uint8_t slaveId, const uint8_t* md5Hash);

/**
 * @brief Handle ACK/NAK from slave
 */
void handleSlaveStreamResponse(uint8_t msgType, const uint8_t* data, size_t len);

#endif // CAN_SEND_COMMANDS

} // namespace can_ota_stream
