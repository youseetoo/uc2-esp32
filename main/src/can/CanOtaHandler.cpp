/**
 * @file CanOtaHandler.cpp
 * @brief Implementation of CAN-based OTA firmware update handler
 * 
 * This module receives firmware updates over CAN bus using ISO-TP protocol.
 * Works in conjunction with can_controller.cpp for message routing.
 */

#include "CanOtaHandler.h"

#ifdef CAN_BUS_ENABLED

#include <Arduino.h>
#include <Update.h>
#include <esp_ota_ops.h>
#include <MD5Builder.h>
#include <rom/crc.h>  // For crc32_le
#include "cJSON.h"
#include "can_controller.h"
#include "can_messagetype.h"  // For OTA_CAN_* enum values

namespace can_ota {

// ============================================================================
// Static Variables
// ============================================================================

static CanOtaContext otaContext;
static MD5Builder md5Builder;
static bool isInitialized = false;

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
    return can_controller::sendIsoTpData(receiverID, data, size);
}

// ============================================================================
// Initialization
// ============================================================================

void init() {
    memset(&otaContext, 0, sizeof(CanOtaContext));
    otaContext.state = CAN_OTA_STATE_IDLE;
    isInitialized = true;
    log_i("CAN OTA handler initialized");
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
    
    // Check if OTA already in progress
    if (otaContext.state != CAN_OTA_STATE_IDLE) {
        log_w("OTA already in progress, state=%d", otaContext.state);
        sendNak(CAN_OTA_ERR_BUSY, 0, 0, 0, sourceCanId);
        return;
    }
    
    // Validate firmware size
    if (cmd->firmwareSize == 0 || cmd->firmwareSize > CAN_OTA_MAX_FIRMWARE_SIZE) {
        log_e("Invalid firmware size: %lu (max=%lu)", cmd->firmwareSize, CAN_OTA_MAX_FIRMWARE_SIZE);
        sendNak(CAN_OTA_ERR_INVALID_SIZE, 0, 0, 0, sourceCanId);
        return;
    }
    
    // Validate chunk count consistency
    uint32_t expectedChunks = CAN_OTA_CALC_CHUNKS(cmd->firmwareSize, cmd->chunkSize);
    if (cmd->totalChunks != expectedChunks) {
        log_e("Chunk count mismatch: received=%lu, expected=%lu", cmd->totalChunks, expectedChunks);
        sendNak(CAN_OTA_ERR_INVALID_SIZE, 0, 0, 0, sourceCanId);
        return;
    }
    
    // Get next OTA partition
    const esp_partition_t* partition = esp_ota_get_next_update_partition(NULL);
    if (partition == NULL) {
        log_e("No OTA partition available");
        sendNak(CAN_OTA_ERR_PARTITION, 0, 0, 0, sourceCanId);
        return;
    }
    
    log_i("Target partition: %s (size=%lu)", partition->label, partition->size);
    
    // Check partition size
    if (cmd->firmwareSize > partition->size) {
        log_e("Firmware too large for partition: %lu > %lu", cmd->firmwareSize, partition->size);
        sendNak(CAN_OTA_ERR_INVALID_SIZE, 0, 0, 0, sourceCanId);
        return;
    }
    
    // Begin update
    if (!Update.begin(cmd->firmwareSize, U_FLASH)) {
        log_e("Update.begin() failed: %s", Update.errorString());
        sendNak(CAN_OTA_ERR_BEGIN, 0, 0, 0, sourceCanId);
        return;
    }
    
    // Initialize MD5 builder
    md5Builder.begin();
    
    // Store context
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
    
    log_i("OTA started successfully, waiting for chunks...");
    sendAck(CAN_OTA_OK, sourceCanId);
}

void handleDataChunk(const uint8_t* data, size_t len, uint8_t sourceCanId) {
    // Validate state
    if (otaContext.state != CAN_OTA_STATE_STARTED && 
        otaContext.state != CAN_OTA_STATE_RECEIVING) {
        log_e("Unexpected DATA in state %d", otaContext.state);
        sendNak(CAN_OTA_ERR_NOT_STARTED, 0, 0, 0, sourceCanId);
        return;
    }
    
    // Parse header
    if (len < sizeof(CanOtaDataHeader)) {
        log_e("Data too small: %d (need >= %d)", len, sizeof(CanOtaDataHeader));
        sendNak(CAN_OTA_ERR_INVALID_SIZE, otaContext.nextExpectedChunk, 0, 0, sourceCanId);
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
        sendNak(CAN_OTA_ERR_SEQUENCE, otaContext.nextExpectedChunk, 0, 0, sourceCanId);
        return;
    }
    
    // Verify chunk size in header matches actual payload
    if (header->chunkSize != chunkDataLen) {
        log_e("Chunk size mismatch: header=%u, actual=%d", header->chunkSize, chunkDataLen);
        sendNak(CAN_OTA_ERR_INVALID_SIZE, header->chunkIndex, 0, 0, sourceCanId);
        return;
    }
    
    // Verify CRC32
    uint32_t calculatedCrc = crc32_le(0, chunkData, chunkDataLen);
    if (calculatedCrc != header->crc32) {
        log_e("CRC mismatch at chunk %u: expected=0x%08lX, calc=0x%08lX", 
              header->chunkIndex, header->crc32, calculatedCrc);
        sendNak(CAN_OTA_ERR_CRC, header->chunkIndex, header->crc32, calculatedCrc, sourceCanId);
        return;
    }
    
    // Write to flash
    size_t written = Update.write(const_cast<uint8_t*>(chunkData), header->chunkSize);
    if (written != header->chunkSize) {
        log_e("Flash write error: expected=%u, written=%d", header->chunkSize, written);
        abortOta(CAN_OTA_ERR_WRITE);
        sendNak(CAN_OTA_ERR_WRITE, header->chunkIndex, 0, 0, sourceCanId);
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
    
    sendAck(CAN_OTA_OK, sourceCanId);
}

void handleVerifyCommand(const CanOtaVerifyCommand* cmd, uint8_t sourceCanId) {
    if (otaContext.state != CAN_OTA_STATE_VERIFYING) {
        log_e("Unexpected VERIFY in state %d", otaContext.state);
        sendNak(CAN_OTA_ERR_NOT_STARTED, 0, 0, 0, sourceCanId);
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
        sendAck(CAN_OTA_OK, sourceCanId);
    } else {
        log_e("MD5 verification FAILED!");
        abortOta(CAN_OTA_ERR_MD5);
        sendNak(CAN_OTA_ERR_MD5, 0, 0, 0, sourceCanId);
    }
}

void handleFinishCommand(uint8_t sourceCanId) {
    if (otaContext.state != CAN_OTA_STATE_FINISHING) {
        log_e("Unexpected FINISH in state %d", otaContext.state);
        sendNak(CAN_OTA_ERR_NOT_STARTED, 0, 0, 0, sourceCanId);
        return;
    }
    
    // Finalize update
    if (!Update.end(true)) {
        log_e("Update.end() failed: %s", Update.errorString());
        abortOta(CAN_OTA_ERR_END);
        sendNak(CAN_OTA_ERR_END, 0, 0, 0, sourceCanId);
        return;
    }
    
    uint32_t elapsed = millis() - otaContext.startTime;
    float kbps = (otaContext.bytesReceived * 1000.0f) / (elapsed * 1024.0f);
    log_i("OTA complete! %lu bytes in %lu ms (%.1f KB/s)", 
          otaContext.bytesReceived, elapsed, kbps);
    
    sendAck(CAN_OTA_OK, sourceCanId);
    
    log_i("Rebooting in 1 second...");
    delay(1000);
    ESP.restart();
}

void handleAbortCommand(uint8_t sourceCanId) {
    log_i("OTA abort requested by 0x%02X", sourceCanId);
    abortOta(CAN_OTA_ERR_ABORTED);
    sendAck(CAN_OTA_OK, sourceCanId);
}

void handleStatusQuery(uint8_t sourceCanId) {
    log_d("Status query from 0x%02X", sourceCanId);
    sendStatusResponse(sourceCanId);
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
    return sendCanMessage(slaveId, buffer, sizeof(buffer));
}

int relayDataToSlave(uint8_t slaveId, uint16_t chunkIndex, uint16_t chunkSize, 
                     uint32_t crc32, const uint8_t* data) {
    // Allocate buffer for header + data
    size_t totalSize = 1 + sizeof(CanOtaDataHeader) + chunkSize;
    uint8_t* buffer = (uint8_t*)malloc(totalSize);
    if (!buffer) {
        log_e("Failed to allocate buffer for data relay");
        return -1;
    }
    
    buffer[0] = OTA_CAN_DATA;
    
    CanOtaDataHeader header;
    header.chunkIndex = chunkIndex;
    header.chunkSize = chunkSize;
    header.crc32 = crc32;
    memcpy(&buffer[1], &header, sizeof(CanOtaDataHeader));
    memcpy(&buffer[1 + sizeof(CanOtaDataHeader)], data, chunkSize);
    
    int result = sendCanMessage(slaveId, buffer, totalSize);
    free(buffer);
    
    return result;
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
    } else if (messageType == OTA_CAN_NAK && len >= sizeof(CanOtaNak)) {
        const CanOtaNak* nak = reinterpret_cast<const CanOtaNak*>(data);
        log_w("Slave NAK: status=%d, canId=0x%02X, errorChunk=%u",
              nak->status, nak->canId, nak->errorChunk);
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
        
        return relayStartToSlave(slaveId, firmwareSize, totalChunks, chunkSize, md5Hash);
        
    } else if (strcmp(cmd, "data") == 0) {
        // DATA command: {"cmd":"data", "slaveId":X, "chunk":N, "crc":N, "data":"base64..."}
        cJSON* chunkItem = cJSON_GetObjectItem(doc, "chunk");
        cJSON* crcItem = cJSON_GetObjectItem(doc, "crc");
        cJSON* dataItem = cJSON_GetObjectItem(doc, "data");
        
        if (!chunkItem || !crcItem || !dataItem) {
            log_e("Missing required fields for 'data' command");
            return -1;
        }
        
        uint16_t chunkIndex = (uint16_t)chunkItem->valueint;
        uint32_t crc32 = (uint32_t)crcItem->valuedouble;
        
        // Data is expected as raw bytes array in JSON (will need base64 decode in practice)
        // For now, assume data is passed as byte array
        size_t dataLen = cJSON_GetArraySize(dataItem);
        if (dataLen > CAN_OTA_CHUNK_SIZE) {
            log_e("Data chunk too large: %d", dataLen);
            return -1;
        }
        
        uint8_t* dataBytes = (uint8_t*)malloc(dataLen);
        if (!dataBytes) return -1;
        
        for (size_t i = 0; i < dataLen; i++) {
            cJSON* byteItem = cJSON_GetArrayItem(dataItem, i);
            dataBytes[i] = (uint8_t)(byteItem ? byteItem->valueint : 0);
        }
        
        int result = relayDataToSlave(slaveId, chunkIndex, dataLen, crc32, dataBytes);
        free(dataBytes);
        return result;
        
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
        
    } else if (strcmp(cmd, "finish") == 0) {
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

#endif // CAN_BUS_ENABLED
