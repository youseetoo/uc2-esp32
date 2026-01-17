/**
 * @file CanOtaHandler.cpp
 * @brief Implementation of CAN-based OTA firmware update handler
 * 
 * This module receives firmware updates over CAN bus using ISO-TP protocol.
 * Works in conjunction with can_controller.cpp for message routing.
 */

#include "CanOtaHandler.h"


#include <Arduino.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <MD5Builder.h>
#include <rom/crc.h>  // For crc32_le
#include <mbedtls/base64.h>  // For base64 decode
#include "cJSON.h"
#include "can_controller.h"
#include "can_messagetype.h"  // For OTA_CAN_* enum values
#include "BinaryOtaProtocol.h"  // For binary OTA mode
#include "PinConfig.h"  // For pinConfig.CAN_ID_CENTRAL_NODE

namespace can_ota {

// ============================================================================
// Static Variables
// ============================================================================

static CanOtaContext otaContext;
static MD5Builder md5Builder;
static bool isInitialized = false;

// Master-side: Variables for tracking slave responses
#ifdef CAN_SEND_COMMANDS
static volatile bool slaveResponseReceived = false;
static volatile bool slaveResponseIsAck = false;
static volatile uint8_t slaveResponseStatus = 0;
static volatile uint16_t slaveResponseChunk = 0;
static const uint32_t SLAVE_ACK_TIMEOUT_MS = 2000;  // 2 second timeout for slave ACK
#endif

// ============================================================================
// Helper Functions
// ============================================================================

/**
 * @brief Get device CAN ID from controller
 */
static uint8_t getDeviceCanId() {
    return can_controller::device_can_id;
}

/**
 * @brief Send CAN message via ISO-TP
 */
static int sendCanMessage(uint8_t receiverID, const uint8_t* data, size_t size) {
    return can_controller::sendIsoTpData(receiverID, data, size); // TODO: We should probably have a similar pattern to how the motor data is sent
}

// ============================================================================
// Initialization
// ============================================================================

void init() {
    memset(&otaContext, 0, sizeof(CanOtaContext));
    otaContext.state = CAN_OTA_STATE_IDLE;
    otaContext.masterCanId = pinConfig.CAN_ID_CENTRAL_NODE;  // Default to central node
    isInitialized = true;
    log_i("CAN OTA handler initialized, default master CAN ID: %u", otaContext.masterCanId);
}

// ============================================================================
// State Accessors
// ============================================================================

CanOtaState getState() {
    return otaContext.state;
}

uint8_t getProgress() {
    if (otaContext.firmwareSize == 0) return 0;
    return CAN_OTA_PROGRESS(otaContext.bytesReceived, otaContext.firmwareSize);
}

// ============================================================================
// Message Handler Entry Point
// ============================================================================

void handleCanOtaMessage(uint8_t messageType, const uint8_t* data, size_t len, uint8_t sourceCanId) {
    if (!isInitialized) {
        init();
    }
    
    log_i("CAN OTA message: type=0x%02X, len=%d, from=0x%02X", messageType, len, sourceCanId);
    
    switch (messageType) {
        case OTA_CAN_START:
            if (len >= sizeof(CanOtaStartCommand)) {
                handleStartCommand(reinterpret_cast<const CanOtaStartCommand*>(data), sourceCanId);
            } else {
                log_e("Invalid START length: %d (expected >= %d)", len, sizeof(CanOtaStartCommand));
                sendNak(CAN_OTA_ERR_INVALID_SIZE, 0, 0, 0, sourceCanId);
            }
            break;
            
        case OTA_CAN_DATA:
            handleDataChunk(data, len, sourceCanId);
            break;
            
        case OTA_CAN_VERIFY:
            if (len >= sizeof(CanOtaVerifyCommand)) {
                handleVerifyCommand(reinterpret_cast<const CanOtaVerifyCommand*>(data), sourceCanId);
            } else {
                sendNak(CAN_OTA_ERR_INVALID_SIZE, 0, 0, 0, sourceCanId);
            }
            break;
            
        case OTA_CAN_FINISH:
            handleFinishCommand(sourceCanId);
            break;
            
        case OTA_CAN_ABORT:
            handleAbortCommand(sourceCanId);
            break;
            
        case OTA_CAN_STATUS:
            handleStatusQuery(sourceCanId);
            break;
            
#ifdef CAN_SEND_COMMANDS
        // Master receives these from slaves
        case OTA_CAN_ACK:
        case OTA_CAN_NAK:
            handleSlaveResponse(messageType, data, len);
            break;
#endif
            
        default:
            log_w("Unknown CAN OTA message type: 0x%02X", messageType);
            break;
    }
}

// ============================================================================
// Slave-side Command Handlers
// ============================================================================

void handleStartCommand(const CanOtaStartCommand* cmd, uint8_t sourceCanId) {
    log_i("OTA START: size=%lu, chunks=%lu, chunkSize=%u", 
          cmd->firmwareSize, cmd->totalChunks, cmd->chunkSize);
    
    // Use the known master CAN ID for responses since ISO-TP doesn't provide source ID
    // The sourceCanId parameter from PDU is unreliable (set to 0)
    uint8_t masterCanId = pinConfig.CAN_ID_CENTRAL_NODE;
    log_i("OTA using master CAN ID %u for responses (sourceCanId param was %u)", masterCanId, sourceCanId);
    
    // Check if OTA already in progress
    if (otaContext.state != CAN_OTA_STATE_IDLE) {
        log_w("OTA already in progress, state=%d", otaContext.state);
        sendNak(CAN_OTA_ERR_BUSY, 0, 0, 0, masterCanId);
        return;
    }
    
    // Validate firmware size
    if (cmd->firmwareSize == 0 || cmd->firmwareSize > CAN_OTA_MAX_FIRMWARE_SIZE) {
        log_e("Invalid firmware size: %lu (max=%lu)", cmd->firmwareSize, CAN_OTA_MAX_FIRMWARE_SIZE);
        sendNak(CAN_OTA_ERR_INVALID_SIZE, 0, 0, 0, masterCanId);
        return;
    }
    
    // Validate chunk count consistency
    uint32_t expectedChunks = CAN_OTA_CALC_CHUNKS(cmd->firmwareSize, cmd->chunkSize);
    if (cmd->totalChunks != expectedChunks) {
        log_e("Chunk count mismatch: received=%lu, expected=%lu", cmd->totalChunks, expectedChunks);
        sendNak(CAN_OTA_ERR_INVALID_SIZE, 0, 0, 0, masterCanId);
        return;
    }
    
    // Get next OTA partition
    const esp_partition_t* partition = esp_ota_get_next_update_partition(NULL);
    if (partition == NULL) {
        log_e("No OTA partition available");
        sendNak(CAN_OTA_ERR_PARTITION, 0, 0, 0, masterCanId);
        return;
    }
    
    log_i("Target partition: %s (size=%lu)", partition->label, partition->size);
    
    // Check partition size
    if (cmd->firmwareSize > partition->size) {
        log_e("Firmware too large for partition: %lu > %lu", cmd->firmwareSize, partition->size);
        sendNak(CAN_OTA_ERR_INVALID_SIZE, 0, 0, 0, masterCanId);
        return;
    }
    
    // Begin update
    if (!Update.begin(cmd->firmwareSize, U_FLASH)) {
        log_e("Update.begin() failed: %s", Update.errorString());
        sendNak(CAN_OTA_ERR_BEGIN, 0, 0, 0, masterCanId);
        return;
    }
    
    // Initialize MD5 builder
    md5Builder.begin();
    
    // Store context - including master CAN ID for future responses
    otaContext.state = CAN_OTA_STATE_STARTED;
    otaContext.firmwareSize = cmd->firmwareSize;
    otaContext.totalChunks = cmd->totalChunks;
    otaContext.chunkSize = cmd->chunkSize;
    otaContext.nextExpectedChunk = 0;
    otaContext.bytesReceived = 0;
    memcpy(otaContext.expectedMd5, cmd->md5Hash, CAN_OTA_MD5_LENGTH);
    otaContext.startTime = millis();
    otaContext.lastChunkTime = millis();
    otaContext.retryCount = 0;
    otaContext.masterCanId = masterCanId;  // Store master CAN ID for all future responses
    
    log_i("OTA started successfully, waiting for chunks, sending ACK to master CAN ID: %u", masterCanId);
    sendAck(CAN_OTA_OK, masterCanId);
}

void handleDataChunk(const uint8_t* data, size_t len, uint8_t sourceCanId) {
    // Use stored master CAN ID from context for responses (sourceCanId from PDU is unreliable)
    uint8_t responseCanId = otaContext.masterCanId;
    
    // Validate state
    if (otaContext.state != CAN_OTA_STATE_STARTED && 
        otaContext.state != CAN_OTA_STATE_RECEIVING) {
        log_e("Unexpected DATA in state %d", otaContext.state);
        sendNak(CAN_OTA_ERR_NOT_STARTED, 0, 0, 0, responseCanId);
        return;
    }
    
    // Parse header
    if (len < sizeof(CanOtaDataHeader)) {
        log_e("Data too small: %d (need >= %d)", len, sizeof(CanOtaDataHeader));
        sendNak(CAN_OTA_ERR_INVALID_SIZE, otaContext.nextExpectedChunk, 0, 0, responseCanId);
        return;
    }
    
    const CanOtaDataHeader* header = reinterpret_cast<const CanOtaDataHeader*>(data);
    const uint8_t* chunkData = data + sizeof(CanOtaDataHeader);
    size_t chunkDataLen = len - sizeof(CanOtaDataHeader);
    
    // Log progress periodically
    if (header->chunkIndex % 50 == 0 || header->chunkIndex == otaContext.totalChunks - 1) {
        log_i("Chunk %u/%lu: size=%u, CRC=0x%08lX", 
              header->chunkIndex, otaContext.totalChunks, header->chunkSize, header->crc32);
    }
    
    // Verify sequence
    if (header->chunkIndex != otaContext.nextExpectedChunk) {
        log_e("Sequence error: expected=%lu, got=%u", otaContext.nextExpectedChunk, header->chunkIndex);
        sendNak(CAN_OTA_ERR_SEQUENCE, otaContext.nextExpectedChunk, 0, 0, responseCanId);
        return;
    }
    
    // Verify chunk size in header matches actual payload
    if (header->chunkSize != chunkDataLen) {
        log_e("Chunk size mismatch: header=%u, actual=%d", header->chunkSize, chunkDataLen);
        sendNak(CAN_OTA_ERR_INVALID_SIZE, header->chunkIndex, 0, 0, responseCanId);
        return;
    }
    
    // Verify CRC32
    uint32_t calculatedCrc = crc32_le(0, chunkData, chunkDataLen);
    if (calculatedCrc != header->crc32) {
        log_e("CRC mismatch at chunk %u: expected=0x%08lX, calc=0x%08lX", 
              header->chunkIndex, header->crc32, calculatedCrc);
        sendNak(CAN_OTA_ERR_CRC, header->chunkIndex, header->crc32, calculatedCrc, responseCanId);
        return;
    }
    
    // Write to flash
    size_t written = Update.write(const_cast<uint8_t*>(chunkData), header->chunkSize);
    if (written != header->chunkSize) {
        log_e("Flash write error: expected=%u, written=%d", header->chunkSize, written);
        abortOta(CAN_OTA_ERR_WRITE);
        sendNak(CAN_OTA_ERR_WRITE, header->chunkIndex, 0, 0, responseCanId);
        return;
    }
    
    // Update MD5
    md5Builder.add(const_cast<uint8_t*>(chunkData), header->chunkSize);
    
    // Update context
    otaContext.state = CAN_OTA_STATE_RECEIVING;
    otaContext.nextExpectedChunk++;
    otaContext.bytesReceived += header->chunkSize;
    otaContext.lastChunkTime = millis();
    
    // Check if all chunks received
    if (otaContext.nextExpectedChunk >= otaContext.totalChunks) {
        otaContext.state = CAN_OTA_STATE_VERIFYING;
        log_i("All %lu chunks received (%lu bytes), ready for verification", 
              otaContext.totalChunks, otaContext.bytesReceived);
    }
    
    sendAck(CAN_OTA_OK, responseCanId);
}

void handleVerifyCommand(const CanOtaVerifyCommand* cmd, uint8_t sourceCanId) {
    // Use stored master CAN ID from context for responses
    uint8_t responseCanId = otaContext.masterCanId;
    
    if (otaContext.state != CAN_OTA_STATE_VERIFYING) {
        log_e("Unexpected VERIFY in state %d", otaContext.state);
        sendNak(CAN_OTA_ERR_NOT_STARTED, 0, 0, 0, responseCanId);
        return;
    }
    
    // Calculate final MD5
    md5Builder.calculate();
    String calculatedMd5 = md5Builder.toString();
    
    // Convert expected MD5 bytes to string
    char expectedMd5Str[33];
    for (int i = 0; i < CAN_OTA_MD5_LENGTH; i++) {
        sprintf(&expectedMd5Str[i * 2], "%02x", cmd->md5Hash[i]);
    }
    expectedMd5Str[32] = '\0';
    
    log_i("MD5 verification:");
    log_i("  Expected:   %s", expectedMd5Str);
    log_i("  Calculated: %s", calculatedMd5.c_str());
    
    if (calculatedMd5.equalsIgnoreCase(expectedMd5Str)) {
        log_i("MD5 verification PASSED");
        otaContext.state = CAN_OTA_STATE_FINISHING;
        sendAck(CAN_OTA_OK, responseCanId);
    } else {
        log_e("MD5 verification FAILED!");
        abortOta(CAN_OTA_ERR_MD5);
        sendNak(CAN_OTA_ERR_MD5, 0, 0, 0, responseCanId);
    }
}

void handleFinishCommand(uint8_t sourceCanId) {
    // Use stored master CAN ID from context for responses
    uint8_t responseCanId = otaContext.masterCanId;
    
    if (otaContext.state != CAN_OTA_STATE_FINISHING) {
        log_e("Unexpected FINISH in state %d", otaContext.state);
        sendNak(CAN_OTA_ERR_NOT_STARTED, 0, 0, 0, responseCanId);
        return;
    }
    
    // Finalize update
    if (!Update.end(true)) {
        log_e("Update.end() failed: %s", Update.errorString());
        abortOta(CAN_OTA_ERR_END);
        sendNak(CAN_OTA_ERR_END, 0, 0, 0, responseCanId);
        return;
    }
    // TODO: I guess here is the firmware magically copied to the boot partition?
    uint32_t elapsed = millis() - otaContext.startTime;
    float kbps = (otaContext.bytesReceived * 1000.0f) / (elapsed * 1024.0f);
    log_i("OTA complete! %lu bytes in %lu ms (%.1f KB/s)", 
          otaContext.bytesReceived, elapsed, kbps);
    
    sendAck(CAN_OTA_OK, responseCanId);
    
    log_i("Rebooting in 1 second...");
    delay(1000);
    ESP.restart();
}

void handleAbortCommand(uint8_t sourceCanId) {
    // Use stored master CAN ID or default to central node if not in OTA
    uint8_t responseCanId = (otaContext.state != CAN_OTA_STATE_IDLE) ? 
                            otaContext.masterCanId : pinConfig.CAN_ID_CENTRAL_NODE;
    log_i("OTA abort requested, responding to CAN ID %u", responseCanId);
    abortOta(CAN_OTA_ERR_ABORTED);
    sendAck(CAN_OTA_OK, responseCanId);
}

void handleStatusQuery(uint8_t sourceCanId) {
    // Use stored master CAN ID or default to central node if not in OTA
    uint8_t responseCanId = (otaContext.state != CAN_OTA_STATE_IDLE) ? 
                            otaContext.masterCanId : pinConfig.CAN_ID_CENTRAL_NODE;
    log_d("Status query, responding to CAN ID %u", responseCanId);
    sendStatusResponse(responseCanId);
}

// ============================================================================
// Response Senders
// ============================================================================

void sendAck(uint8_t status, uint8_t sourceCanId) {
    CanOtaAck ack;
    ack.status = status;
    ack.canId = getDeviceCanId();
    ack.expectedChunk = otaContext.nextExpectedChunk;
    ack.bytesReceived = otaContext.bytesReceived;
    ack.crc32Received = 0;
    
    uint8_t buffer[1 + sizeof(CanOtaAck)];
    buffer[0] = OTA_CAN_ACK;
    memcpy(&buffer[1], &ack, sizeof(CanOtaAck));
    
    sendCanMessage(sourceCanId, buffer, sizeof(buffer));
}

void sendNak(uint8_t status, uint16_t errorChunk, uint32_t expectedCrc, uint32_t receivedCrc, uint8_t sourceCanId) {
    CanOtaNak nak;
    nak.status = status;
    nak.canId = getDeviceCanId();
    nak.errorChunk = errorChunk;
    nak.expectedCrc = expectedCrc;
    nak.receivedCrc = receivedCrc;
    
    uint8_t buffer[1 + sizeof(CanOtaNak)];
    buffer[0] = OTA_CAN_NAK;
    memcpy(&buffer[1], &nak, sizeof(CanOtaNak));
    
    sendCanMessage(sourceCanId, buffer, sizeof(buffer));
    log_w("Sent NAK: status=%d, chunk=%u", status, errorChunk);
}

void sendStatusResponse(uint8_t sourceCanId) {
    CanOtaStatusResponse status;
    status.isActive = (otaContext.state != CAN_OTA_STATE_IDLE);
    status.canId = getDeviceCanId();
    status.currentChunk = otaContext.nextExpectedChunk;
    status.totalChunks = otaContext.totalChunks;
    status.bytesReceived = otaContext.bytesReceived;
    status.bytesTotal = otaContext.firmwareSize;
    status.progressPercent = getProgress();
    
    uint8_t buffer[1 + sizeof(CanOtaStatusResponse)];
    buffer[0] = OTA_CAN_STATUS;
    memcpy(&buffer[1], &status, sizeof(CanOtaStatusResponse));
    
    sendCanMessage(sourceCanId, buffer, sizeof(buffer));
}

// ============================================================================
// Cleanup and Loop
// ============================================================================

void abortOta(uint8_t status) {
    if (otaContext.state != CAN_OTA_STATE_IDLE) {
        log_w("Aborting OTA with status %d (state was %d)", status, otaContext.state);
        Update.abort();
    }
    
    memset(&otaContext, 0, sizeof(CanOtaContext));
    otaContext.state = CAN_OTA_STATE_IDLE;
}

void loop() {
    if (otaContext.state == CAN_OTA_STATE_IDLE) {
        return;
    }
    
    // Timeout check during transfer
    if (otaContext.state == CAN_OTA_STATE_RECEIVING || 
        otaContext.state == CAN_OTA_STATE_STARTED) {
        uint32_t elapsed = millis() - otaContext.lastChunkTime;
        if (elapsed > CHUNK_TIMEOUT_MS) {
            log_e("OTA timeout: no data for %lu ms", elapsed);
            abortOta(CAN_OTA_ERR_TIMEOUT);
        }
    }
}

// ============================================================================
// Master-side Relay Functions
// ============================================================================

#ifdef CAN_SEND_COMMANDS

int relayStartToSlave(uint8_t slaveId, uint32_t firmwareSize, uint32_t totalChunks, 
                      uint16_t chunkSize, const uint8_t* md5Hash) {
    CanOtaStartCommand cmd;
    cmd.firmwareSize = firmwareSize;
    cmd.totalChunks = totalChunks;
    cmd.chunkSize = chunkSize;
    memcpy(cmd.md5Hash, md5Hash, CAN_OTA_MD5_LENGTH);
    
    uint8_t buffer[1 + sizeof(CanOtaStartCommand)];
    buffer[0] = OTA_CAN_START;
    memcpy(&buffer[1], &cmd, sizeof(CanOtaStartCommand));
    
    log_i("Sending OTA START to slave 0x%02X: size=%lu, chunks=%lu", 
          slaveId, firmwareSize, totalChunks);
    
    // Clear slave response flags before sending
    slaveResponseReceived = false;
    
    int result = sendCanMessage(slaveId, buffer, sizeof(buffer));
    if (result < 0) {
        log_e("Failed to send START to CAN bus");
        return result;
    }
    
    // Wait for slave to acknowledge the START command
    // Slave needs more time to initialize OTA (partition, Update.begin, etc.)
    uint32_t startTimeout = 3000;  // 3 seconds for START command
    uint32_t startTime = millis();
    
    while (!slaveResponseReceived && (millis() - startTime) < startTimeout) {
        delay(10);  // Let CAN controller process incoming messages
    }
    
    if (!slaveResponseReceived) {
        log_e("Slave did not respond to START command");
        return -1;
    }
    
    if (!slaveResponseIsAck) {
        log_e("Slave NAK'd START command: status=%d", slaveResponseStatus);
        return -2;
    }
    
    log_i("Slave ACKed START command");
    return 0;
}

/**
 * @brief Wait for slave ACK/NAK response after sending a chunk
 * @param timeoutMs Timeout in milliseconds
 * @return 0 on ACK, negative on NAK or timeout
 */
static int waitForSlaveAck(uint32_t timeoutMs = SLAVE_ACK_TIMEOUT_MS) {
    // Clear previous response
    slaveResponseReceived = false;
    slaveResponseIsAck = false;
    
    uint32_t startTime = millis();
    
    while (!slaveResponseReceived && (millis() - startTime) < timeoutMs) {
        // Let the CAN controller process incoming messages
        // The handleSlaveResponse callback will set slaveResponseReceived
        delay(1);  // Small delay to allow CAN processing
    }
    
    if (!slaveResponseReceived) {
        log_w("Slave ACK timeout after %lu ms", timeoutMs);
        return -1;  // Timeout
    }
    
    if (!slaveResponseIsAck) {
        log_w("Slave NAK received: status=%d, chunk=%d", slaveResponseStatus, slaveResponseChunk);
        return -2;  // NAK received
    }
    
    return 0;  // ACK received
}

int relayDataToSlave(uint8_t slaveId, uint16_t chunkIndex, uint16_t chunkSize, 
                     uint32_t crc32, const uint8_t* data) {
    // Use static buffer to avoid heap fragmentation from repeated malloc/free
    // Buffer size: 1 (cmd) + sizeof(CanOtaDataHeader) + CAN_OTA_CHUNK_SIZE
    static uint8_t buffer[1 + sizeof(CanOtaDataHeader) + CAN_OTA_CHUNK_SIZE + 16];
    
    size_t totalSize = 1 + sizeof(CanOtaDataHeader) + chunkSize;
    if (totalSize > sizeof(buffer)) {
        log_e("Data relay buffer overflow: %d > %d", totalSize, sizeof(buffer));
        return -1;
    }
    
    buffer[0] = OTA_CAN_DATA;
    
    CanOtaDataHeader header;
    header.chunkIndex = chunkIndex;
    header.chunkSize = chunkSize;
    header.crc32 = crc32;
    memcpy(&buffer[1], &header, sizeof(CanOtaDataHeader));
    memcpy(&buffer[1 + sizeof(CanOtaDataHeader)], data, chunkSize);
    
    // Clear slave response flags before sending
    slaveResponseReceived = false;
    
    int result = sendCanMessage(slaveId, buffer, totalSize);
    if (result < 0) {
        log_e("Failed to send chunk %d to CAN bus", chunkIndex);
        return result;
    }
    
    // Wait for slave ACK/NAK - this ensures we don't overrun the CAN bus
    // and allows proper error handling
    int ackResult = waitForSlaveAck(SLAVE_ACK_TIMEOUT_MS);
    if (ackResult < 0) {
        log_e("Chunk %d: slave did not ACK (result=%d)", chunkIndex, ackResult);
        return ackResult;
    }
    
    // Log progress every 50 chunks
    if (chunkIndex % 50 == 0) {
        log_i("Chunk %d relayed and ACKed by slave", chunkIndex);
    }
    
    return 0;
}

int relayVerifyToSlave(uint8_t slaveId, const uint8_t* md5Hash) {
    CanOtaVerifyCommand cmd;
    memcpy(cmd.md5Hash, md5Hash, CAN_OTA_MD5_LENGTH);
    
    uint8_t buffer[1 + sizeof(CanOtaVerifyCommand)];
    buffer[0] = OTA_CAN_VERIFY;
    memcpy(&buffer[1], &cmd, sizeof(CanOtaVerifyCommand));
    
    log_i("Sending OTA VERIFY to slave 0x%02X", slaveId);
    return sendCanMessage(slaveId, buffer, sizeof(buffer));
}

int relayFinishToSlave(uint8_t slaveId) {
    uint8_t buffer[1] = { OTA_CAN_FINISH };
    log_i("Sending OTA FINISH to slave 0x%02X", slaveId);
    return sendCanMessage(slaveId, buffer, sizeof(buffer));
}

int relayAbortToSlave(uint8_t slaveId) {
    uint8_t buffer[1] = { OTA_CAN_ABORT };
    log_i("Sending OTA ABORT to slave 0x%02X", slaveId);
    return sendCanMessage(slaveId, buffer, sizeof(buffer));
}

int relayStatusQueryToSlave(uint8_t slaveId) {
    uint8_t buffer[1] = { OTA_CAN_STATUS };
    log_d("Sending OTA STATUS query to slave 0x%02X", slaveId);
    return sendCanMessage(slaveId, buffer, sizeof(buffer));
}

void handleSlaveResponse(uint8_t messageType, const uint8_t* data, size_t len) {
    // Log and/or forward response to serial
    if (messageType == OTA_CAN_ACK && len >= sizeof(CanOtaAck)) {
        const CanOtaAck* ack = reinterpret_cast<const CanOtaAck*>(data);
        log_i("Slave ACK: status=%d, canId=0x%02X, nextChunk=%lu, bytes=%lu",
              ack->status, ack->canId, ack->expectedChunk, ack->bytesReceived);
        
        // Set response flags for synchronization
        slaveResponseStatus = ack->status;
        slaveResponseChunk = ack->expectedChunk;
        slaveResponseIsAck = true;
        slaveResponseReceived = true;
    } else if (messageType == OTA_CAN_NAK && len >= sizeof(CanOtaNak)) {
        const CanOtaNak* nak = reinterpret_cast<const CanOtaNak*>(data);
        log_w("Slave NAK: status=%d, canId=0x%02X, errorChunk=%u",
              nak->status, nak->canId, nak->errorChunk);
        
        // Set response flags for synchronization
        slaveResponseStatus = nak->status;
        slaveResponseChunk = nak->errorChunk;
        slaveResponseIsAck = false;
        slaveResponseReceived = true;
    }
}

#endif // CAN_SEND_COMMANDS

// ============================================================================
// JSON Command Interface (Master-side, from SerialProcess)
// ============================================================================

int actFromJson(cJSON* doc) {
#ifdef CAN_SEND_COMMANDS
    // Parse command type - accept both "cmd" and "action" for compatibility
    cJSON* cmdItem = cJSON_GetObjectItem(doc, "cmd");
    if (!cmdItem || !cJSON_IsString(cmdItem)) {
        cmdItem = cJSON_GetObjectItem(doc, "action");
        if (!cmdItem || !cJSON_IsString(cmdItem)) {
            log_e("Missing or invalid 'cmd' or 'action' field");
            return -1;
        }
    }
    const char* cmd = cmdItem->valuestring;
    
    // Parse slave ID - accept both "slaveId" and "canid" for compatibility
    cJSON* slaveIdItem = cJSON_GetObjectItem(doc, "slaveId");
    if (!slaveIdItem || !cJSON_IsNumber(slaveIdItem)) {
        slaveIdItem = cJSON_GetObjectItem(doc, "canid");
        if (!slaveIdItem || !cJSON_IsNumber(slaveIdItem)) {
            log_e("Missing or invalid 'slaveId' or 'canid' field");
            return -1;
        }
    }
    uint8_t slaveId = (uint8_t)slaveIdItem->valueint;
    
    if (strcmp(cmd, "start") == 0) {
        // START command: {"action":"start", "canid":X, "firmware_size":N, "total_chunks":N, "chunk_size":N, "md5":"..."}
        // Also accepts: {"cmd":"start", "slaveId":X, "size":N, "chunks":N, "chunkSize":N, "md5":"..."}
        cJSON* sizeItem = cJSON_GetObjectItem(doc, "size");
        if (!sizeItem) sizeItem = cJSON_GetObjectItem(doc, "firmware_size");
        
        cJSON* chunksItem = cJSON_GetObjectItem(doc, "chunks");
        if (!chunksItem) chunksItem = cJSON_GetObjectItem(doc, "total_chunks");
        
        cJSON* chunkSizeItem = cJSON_GetObjectItem(doc, "chunkSize");
        if (!chunkSizeItem) chunkSizeItem = cJSON_GetObjectItem(doc, "chunk_size");
        
        cJSON* md5Item = cJSON_GetObjectItem(doc, "md5");
        
        if (!sizeItem || !chunksItem || !chunkSizeItem || !md5Item) {
            log_e("Missing required fields for 'start' command");
            return -1;
        }
        
        uint32_t firmwareSize = (uint32_t)sizeItem->valuedouble;
        uint32_t totalChunks = (uint32_t)chunksItem->valuedouble;
        uint16_t chunkSize = (uint16_t)chunkSizeItem->valueint;
        
        // Parse MD5 hex string to bytes
        uint8_t md5Hash[CAN_OTA_MD5_LENGTH];
        const char* md5Str = md5Item->valuestring;
        for (int i = 0; i < CAN_OTA_MD5_LENGTH; i++) {
            sscanf(&md5Str[i * 2], "%2hhx", &md5Hash[i]);
        }
        
        // Check if binary mode is requested
        cJSON* binaryModeItem = cJSON_GetObjectItem(doc, "binary_mode");
        bool useBinaryMode = (binaryModeItem && cJSON_IsTrue(binaryModeItem));
        
        int result = relayStartToSlave(slaveId, firmwareSize, totalChunks, chunkSize, md5Hash);
        
        if (result >= 0 && useBinaryMode) {
            // Return special response indicating binary mode will be activated
            // The caller should read this response and then switch baudrate
            log_i("Binary OTA mode requested - will switch to 2Mbaud after response");
            // Note: actual mode switch happens after this function returns
            // so the JSON response can be sent first
            return 2;  // Special return value for binary mode
        }
        
        return result;
        
    } else if (strcmp(cmd, "binary_start") == 0) {
        // Binary mode start command - switches to high-speed binary protocol
        // {"action":"binary_start", "canid":X}
        if (binary_ota::enterBinaryMode(slaveId)) {
            return 1;  // Success - but response already sent in binary mode
        }
        return -1;
        
    } else if (strcmp(cmd, "data") == 0) {
        // DATA command: Multiple formats supported:
        // - {"action":"data", "canid":X, "chunk_index":N, "chunk_crc":N, "data":"base64..."}
        // - {"action":"data", "canid":X, "chunk_index":N, "chunk_crc":N, "hex":"hexstring..."}
        // - {"action":"data", "canid":X, "chunk_index":N, "chunk_crc":N, "chunk_size":N, "data":[...]}
        // Also accept legacy names: "chunk", "crc"
        cJSON* chunkItem = cJSON_GetObjectItem(doc, "chunk_index");
        if (!chunkItem) chunkItem = cJSON_GetObjectItem(doc, "chunk");
        
        cJSON* crcItem = cJSON_GetObjectItem(doc, "chunk_crc");
        if (!crcItem) crcItem = cJSON_GetObjectItem(doc, "crc");
        
        // Check for hex format first (preferred - more efficient than base64)
        cJSON* hexItem = cJSON_GetObjectItem(doc, "hex");
        cJSON* dataItem = cJSON_GetObjectItem(doc, "data");
        
        if (!chunkItem || !crcItem || (!dataItem && !hexItem)) {
            log_e("Missing required fields for 'data' command (need chunk_index/chunk, chunk_crc/crc, data/hex)");
            return -1;
        }
        
        uint16_t chunkIndex = (uint16_t)chunkItem->valueint;
        uint32_t crc32 = (uint32_t)crcItem->valuedouble;
        
        // Log only first and every 100th chunk to avoid spam
        if (chunkIndex == 0 || (chunkIndex % 100 == 0)) {
            log_d("Relaying DATA chunk %u to slave 0x%02X", chunkIndex, slaveId);
        }
        
        // Use static buffer to avoid heap fragmentation
        static uint8_t dataBytes[CAN_OTA_CHUNK_SIZE + 16];
        size_t dataLen = 0;
        
        if (hexItem && cJSON_IsString(hexItem)) {
            // HEX encoded string - more efficient than base64
            const char* hexData = hexItem->valuestring;
            size_t hexLen = strlen(hexData);
            dataLen = hexLen / 2;
            
            if (dataLen > CAN_OTA_CHUNK_SIZE) {
                log_e("Hex data too large: %d > %d", dataLen, CAN_OTA_CHUNK_SIZE);
                return -1;
            }
            
            // Decode hex string to bytes
            for (size_t i = 0; i < dataLen; i++) {
                unsigned int byte;
                if (sscanf(&hexData[i * 2], "%2x", &byte) != 1) {
                    log_e("Invalid hex at position %d", i * 2);
                    return -1;
                }
                dataBytes[i] = (uint8_t)byte;
            }
            
            if (chunkIndex == 0) {
                log_d("Decoded hex: %d bytes from %d chars", dataLen, hexLen);
            }
            
        } else if (dataItem && cJSON_IsString(dataItem)) {
            // Base64 encoded string - decode it
            const char* base64Data = dataItem->valuestring;
            size_t base64Len = strlen(base64Data);
            
            // Base64 decode using mbedtls into static buffer
            int ret = mbedtls_base64_decode(dataBytes, CAN_OTA_CHUNK_SIZE + 16, &dataLen, 
                                            (const unsigned char*)base64Data, base64Len);
            if (ret != 0) {
                log_e("Base64 decode failed: %d", ret);
                return -1;
            }
            
            if (chunkIndex == 0) {
                log_d("Decoded base64: %d bytes from %d chars", dataLen, base64Len);
            }
            
        } else if (dataItem && cJSON_IsArray(dataItem)) {
            // Byte array format (legacy)
            dataLen = cJSON_GetArraySize(dataItem);
            if (dataLen > CAN_OTA_CHUNK_SIZE) {
                log_e("Data chunk too large: %d", dataLen);
                return -1;
            }
            
            for (size_t i = 0; i < dataLen; i++) {
                cJSON* byteItem = cJSON_GetArrayItem(dataItem, i);
                dataBytes[i] = (uint8_t)(byteItem ? byteItem->valueint : 0);
            }
        } else {
            log_e("Invalid data format - expected 'hex' string, 'data' base64 string, or byte array");
            return -1;
        }
        
        if (dataLen > CAN_OTA_CHUNK_SIZE) {
            log_e("Decoded data too large: %d > %d", dataLen, CAN_OTA_CHUNK_SIZE);
            return -1;
        }
        
        // Use static buffer in relayDataToSlave - no malloc/free needed
        return relayDataToSlave(slaveId, chunkIndex, dataLen, crc32, dataBytes);
        
    } else if (strcmp(cmd, "verify") == 0) {
        // VERIFY command: {"cmd":"verify", "slaveId":X, "md5":"..."}
        cJSON* md5Item = cJSON_GetObjectItem(doc, "md5");
        if (!md5Item) {
            log_e("Missing 'md5' field for verify command");
            return -1;
        }
        
        uint8_t md5Hash[CAN_OTA_MD5_LENGTH];
        const char* md5Str = md5Item->valuestring;
        for (int i = 0; i < CAN_OTA_MD5_LENGTH; i++) {
            sscanf(&md5Str[i * 2], "%2hhx", &md5Hash[i]);
        }
        
        return relayVerifyToSlave(slaveId, md5Hash);
        
    } else if (strcmp(cmd, "finish") == 0 || strcmp(cmd, "end") == 0) {
        // Accept both "finish" and "end" commands for compatibility
        return relayFinishToSlave(slaveId);
        
    } else if (strcmp(cmd, "abort") == 0) {
        return relayAbortToSlave(slaveId);
        
    } else if (strcmp(cmd, "status") == 0) {
        return relayStatusQueryToSlave(slaveId);
        
    } else {
        log_e("Unknown CAN OTA command: %s", cmd);
        return -1;
    }
#else
    log_w("CAN_SEND_COMMANDS not enabled, cannot relay OTA");
    return -1;
#endif
}

} // namespace can_ota

