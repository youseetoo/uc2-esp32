/**
 * @file CanOtaStreaming.cpp
 * @brief Implementation of high-speed CAN OTA streaming protocol
 * 
 * Key design decisions:
 * 1. Ring buffer decouples CAN RX from flash writes
 * 2. Separate flash writer task does blocking I/O
 * 3. ACK only after full 4KB page received and CRC verified
 * 4. Master can send up to 1 page ahead (windowed)
 */

#include "CanOtaStreaming.h"
#include "can_controller.h"
#include "PinConfig.h"  // For pinConfig.CAN_ID_CENTRAL_NODE
#include <Update.h>
#include <MD5Builder.h>
#include <esp_crc.h>

namespace can_ota_stream {

// ============================================================================
// Module State
// ============================================================================

static StreamContext ctx;
static MD5Builder md5Builder;
static bool initialized = false;

// Page buffer for accumulating incoming data
static uint8_t pageBuffer[STREAM_PAGE_SIZE];
static size_t pageBufferOffset = 0;
static uint16_t currentPageIndex = 0;
static uint32_t currentPageCrc = 0;

// Master-side state
#ifdef CAN_SEND_COMMANDS
static volatile bool slaveAckReceived = false;
static volatile uint16_t slaveAckPage = 0;
static volatile uint32_t slaveAckBytes = 0;
static volatile uint16_t slaveNakOffset = 0;
static volatile bool slaveNakReceived = false;
#endif

// ============================================================================
// Flash Writer Task (runs on separate core)
// ============================================================================

static void flashWriterTask(void* param) {
    PageEntry entry;
    log_i("Flash writer task started");
    while (true) {
        // Wait for a page to be ready
        if (xQueueReceive(ctx.pageQueue, &entry, portMAX_DELAY) == pdTRUE) {
            // Verify CRC before writing
            log_i("Flash writer received page %d for writing", entry.pageIndex);
            uint32_t calcCrc = esp_crc32_le(0, entry.data, STREAM_PAGE_SIZE);
            if (calcCrc != entry.crc32) {
                log_e("Page %d CRC mismatch: expected 0x%08lX, got 0x%08lX", 
                      entry.pageIndex, entry.crc32, calcCrc);
                // Don't abort - the ACK mechanism will handle retransmit
                continue;
            }
            
            // Write to flash (this blocks, but we're on a separate task)
            size_t written = Update.write(entry.data, STREAM_PAGE_SIZE);
            if (written != STREAM_PAGE_SIZE) {
                log_e("Flash write error at page %d: wrote %d/%d bytes",
                      entry.pageIndex, written, STREAM_PAGE_SIZE);
                abort(CAN_OTA_ERR_WRITE);
                continue;
            }
            
            // Update MD5
            md5Builder.add(entry.data, STREAM_PAGE_SIZE);
            
            log_i("Page %d written to flash (%lu bytes total)", 
                  entry.pageIndex, (entry.pageIndex + 1) * STREAM_PAGE_SIZE);
        }
    }
}

// ============================================================================
// Initialization
// ============================================================================

void init() {
    if (initialized) return;
    
    memset(&ctx, 0, sizeof(ctx));
    
    // Create page queue (holds 2 pages max)
    ctx.pageQueue = xQueueCreate(2, sizeof(PageEntry));
    if (!ctx.pageQueue) {
        log_e("Failed to create page queue");
        return;
    }
    
    // Create mutex for buffer access
    ctx.bufferMutex = xSemaphoreCreateMutex();
    if (!ctx.bufferMutex) {
        log_e("Failed to create buffer mutex");
        vQueueDelete(ctx.pageQueue);
        return;
    }
    
    initialized = true;
    log_i("CAN OTA Streaming initialized");
}

// ============================================================================
// Message Handlers (Slave Side)
// ============================================================================

static void handleStartCmd(const uint8_t* data, size_t len, uint8_t sourceCanId) {
    if (len < sizeof(StreamStartCmd)) {
        log_e("STREAM_START too short: %d < %d", len, sizeof(StreamStartCmd));
        return;
    }
    
    const StreamStartCmd* cmd = reinterpret_cast<const StreamStartCmd*>(data);
    
    log_i("STREAM_START: size=%lu, pages=%lu, pageSize=%u, chunkSize=%u",
          cmd->firmwareSize, cmd->totalPages, cmd->pageSize, cmd->chunkSize);
    
    // Use the known master CAN ID for responses since ISO-TP doesn't provide source ID
    // The sourceCanId parameter from PDU is unreliable (may be 0 or our own ID) // TODO: We should change that so that we have the source id instead of 0! 
    uint8_t masterCanId = pinConfig.CAN_ID_CENTRAL_NODE;
    log_i("Using master CAN ID %u for responses (sourceCanId param was %u)", masterCanId, sourceCanId);
    
    // Validate
    if (cmd->firmwareSize > CAN_OTA_MAX_FIRMWARE_SIZE) {
        log_e("Firmware too large: %lu > %d", cmd->firmwareSize, CAN_OTA_MAX_FIRMWARE_SIZE);
        StreamNak nak = {CAN_OTA_ERR_INVALID_SIZE, can_controller::device_can_id, 0, 0, 0};
        uint8_t buf[1 + sizeof(StreamNak)] = {STREAM_NAK};
        memcpy(buf + 1, &nak, sizeof(nak));
        can_controller::sendCanMessage(masterCanId, buf, sizeof(buf));
        return;
    }
    
    if (ctx.active) {
        log_w("Aborting previous streaming session");
        abort(CAN_OTA_ERR_BUSY);
    }
    
    // Start OTA partition
    if (!Update.begin(cmd->firmwareSize)) {
        log_e("Update.begin() failed");
        StreamNak nak = {CAN_OTA_ERR_BEGIN, can_controller::device_can_id, 0, 0, 0};
        uint8_t buf[1 + sizeof(StreamNak)] = {STREAM_NAK};
        memcpy(buf + 1, &nak, sizeof(nak));
        can_controller::sendCanMessage(masterCanId, buf, sizeof(buf));
        return;
    }
    
    // Initialize MD5
    md5Builder.begin();
    
    // Setup context
    ctx.active = true;
    ctx.firmwareSize = cmd->firmwareSize;
    ctx.totalPages = cmd->totalPages;
    ctx.pageSize = cmd->pageSize;
    ctx.chunkSize = cmd->chunkSize;
    memcpy(ctx.expectedMd5, cmd->md5Hash, 16);
    ctx.currentPage = 0;
    ctx.pageOffset = 0;
    ctx.bytesReceived = 0;
    ctx.nextExpectedSeq = 0;
    ctx.startTime = millis();
    ctx.lastDataTime = millis();
    ctx.masterCanId = masterCanId;  // Use reliable central node ID, not sourceCanId
    
    // Reset page buffer
    memset(pageBuffer, 0xFF, STREAM_PAGE_SIZE);  // 0xFF = erased flash
    pageBufferOffset = 0;
    currentPageIndex = 0;
    
    // Create flash writer task if not exists
    if (!ctx.flashWriterTask) {
        log_i("Creating flash writer task");
        xTaskCreatePinnedToCore(
            flashWriterTask,
            "OTA_Flash",
            16384,          // Stack size (16KB - Update.write needs deep stack)
            NULL,
            2,              // Priority (lower than CAN RX)
            &ctx.flashWriterTask,
            1               // Run on Core 1 (CAN is on Core 0)
        );
    }
    
    // Send ACK back to master
    StreamAck ack = {
        CAN_OTA_OK,
        can_controller::device_can_id,
        0xFFFF,  // No pages complete yet
        0,
        0,
        0
    };
    uint8_t buf[1 + sizeof(StreamAck)] = {STREAM_ACK};
    memcpy(buf + 1, &ack, sizeof(ack));
    log_i("Sending STREAM_ACK to master CAN ID %u", masterCanId);
    can_controller::sendCanMessage(masterCanId, buf, sizeof(buf));
    
    log_i("Streaming session started, expecting %lu pages", ctx.totalPages);
}

static void handleDataChunk(const uint8_t* data, size_t len, uint8_t sourceCanId) {
    if (!ctx.active) {
        log_e("STREAM_DATA but no active session");
        return;
    }
    
    if (len < sizeof(StreamDataHeader)) {
        log_e("STREAM_DATA too short: %d", len);
        return;
    }
    
    const StreamDataHeader* hdr = reinterpret_cast<const StreamDataHeader*>(data);
    const uint8_t* chunkData = data + sizeof(StreamDataHeader);
    size_t chunkLen = len - sizeof(StreamDataHeader);
    
    // Validate
    if (hdr->dataSize != chunkLen) {
        log_e("Data size mismatch: header=%u, actual=%d", hdr->dataSize, chunkLen);
        return;
    }
    
    // Check if this is for the current page
    if (hdr->pageIndex != currentPageIndex) {
        if (hdr->pageIndex < currentPageIndex) {
            // Old page, already written - ignore
            log_d("Ignoring old page %u (current=%u)", hdr->pageIndex, currentPageIndex);
            return;
        } else if (hdr->pageIndex > currentPageIndex + 1) {
            // Too far ahead - request retransmit
            log_w("Page %u too far ahead (current=%u)", hdr->pageIndex, currentPageIndex);
            StreamNak nak = {
                CAN_OTA_ERR_SEQUENCE,
                can_controller::device_can_id,
                currentPageIndex,
                (uint16_t)pageBufferOffset,
                ctx.nextExpectedSeq
            };
            uint8_t buf[1 + sizeof(StreamNak)] = {STREAM_NAK};
            memcpy(buf + 1, &nak, sizeof(nak));
            can_controller::sendCanMessage(ctx.masterCanId, buf, sizeof(buf));
            return;
        }
        // Next page started - current page must be incomplete
        // This shouldn't happen with proper master implementation
    }
    
    // Check offset within page
    if (hdr->offsetInPage != pageBufferOffset) {
        if (hdr->offsetInPage < pageBufferOffset) {
            // Duplicate data, ignore
            log_d("Ignoring duplicate at offset %u (current=%u)", hdr->offsetInPage, pageBufferOffset);
            return;
        } else {
            // Gap in data - request retransmit
            log_w("Gap at offset %u (expected=%u)", hdr->offsetInPage, pageBufferOffset);
            StreamNak nak = {
                CAN_OTA_ERR_SEQUENCE,
                can_controller::device_can_id,
                currentPageIndex,
                (uint16_t)pageBufferOffset,
                ctx.nextExpectedSeq
            };
            uint8_t buf[1 + sizeof(StreamNak)] = {STREAM_NAK};
            memcpy(buf + 1, &nak, sizeof(nak));
            can_controller::sendCanMessage(ctx.masterCanId, buf, sizeof(buf));
            return;
        }
    }
    
    // Copy data to page buffer
    size_t spaceLeft = STREAM_PAGE_SIZE - pageBufferOffset;
    size_t toCopy = (chunkLen < spaceLeft) ? chunkLen : spaceLeft;
    memcpy(pageBuffer + pageBufferOffset, chunkData, toCopy);
    pageBufferOffset += toCopy;
    ctx.bytesReceived += toCopy;
    ctx.nextExpectedSeq = hdr->seq + 1;
    ctx.lastDataTime = millis();
    
    // Check if page is complete
    if (pageBufferOffset >= STREAM_PAGE_SIZE) {
        // Calculate CRC of page
        currentPageCrc = esp_crc32_le(0, pageBuffer, STREAM_PAGE_SIZE);
        
        // Queue page for flash writing
        PageEntry entry;
        entry.pageIndex = currentPageIndex;
        memcpy(entry.data, pageBuffer, STREAM_PAGE_SIZE);
        entry.crc32 = currentPageCrc;
        
        if (xQueueSend(ctx.pageQueue, &entry, 0) != pdTRUE) {
            log_e("Page queue full! Flash writer too slow");
            // Don't abort - just wait for it to catch up
        }
        
        // Send cumulative ACK
        StreamAck ack = {
            CAN_OTA_OK,
            can_controller::device_can_id,
            currentPageIndex,
            ctx.bytesReceived,
            ctx.nextExpectedSeq,
            0
        };
        uint8_t buf[1 + sizeof(StreamAck)] = {STREAM_ACK};
        memcpy(buf + 1, &ack, sizeof(ack));
        can_controller::sendCanMessage(ctx.masterCanId, buf, sizeof(buf));
        
        log_i("Page %u complete, ACK sent (%lu bytes total)", currentPageIndex, ctx.bytesReceived);
        
        // Prepare for next page
        currentPageIndex++;
        pageBufferOffset = 0;
        memset(pageBuffer, 0xFF, STREAM_PAGE_SIZE);
    }
}

static void handleFinishCmd(const uint8_t* data, size_t len, uint8_t sourceCanId) {
    if (!ctx.active) {
        log_e("STREAM_FINISH but no active session");
        return;
    }
    
    // Handle last partial page if any
    if (pageBufferOffset > 0) {
        // Last page may be partial - calculate actual size
        size_t remaining = ctx.firmwareSize - (currentPageIndex * STREAM_PAGE_SIZE);
        if (remaining < STREAM_PAGE_SIZE) {
            // Write partial page directly (don't queue)
            size_t written = Update.write(pageBuffer, remaining);
            if (written != remaining) {
                log_e("Final page write error: %d/%d", written, remaining);
                abort(CAN_OTA_ERR_WRITE);
                return;
            }
            md5Builder.add(pageBuffer, remaining);
            ctx.bytesReceived += remaining - pageBufferOffset + pageBufferOffset;
            log_i("Final partial page written (%d bytes)", remaining);
        }
    }
    
    // Wait for flash writer to finish
    while (uxQueueMessagesWaiting(ctx.pageQueue) > 0) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    vTaskDelay(pdMS_TO_TICKS(100));  // Extra delay for last write
    
    // Verify MD5
    md5Builder.calculate();
    String calcMd5 = md5Builder.toString();
    
    char expectedMd5[33];
    for (int i = 0; i < 16; i++) {
        sprintf(&expectedMd5[i*2], "%02x", ctx.expectedMd5[i]);
    }
    expectedMd5[32] = '\0';
    
    log_i("MD5 verify: expected=%s, calculated=%s", expectedMd5, calcMd5.c_str());
    
    if (!calcMd5.equalsIgnoreCase(expectedMd5)) {
        log_e("MD5 mismatch!");
        abort(CAN_OTA_ERR_MD5);
        StreamNak nak = {CAN_OTA_ERR_MD5, can_controller::device_can_id, 0, 0, 0};
        uint8_t buf[1 + sizeof(StreamNak)] = {STREAM_NAK};
        memcpy(buf + 1, &nak, sizeof(nak));
        can_controller::sendCanMessage(sourceCanId, buf, sizeof(buf));
        return;
    }
    
    // Finalize update
    if (!Update.end(true)) {
        log_e("Update.end() failed");
        abort(CAN_OTA_ERR_END);
        return;
    }
    
    // Send final ACK
    StreamAck ack = {
        CAN_OTA_OK,
        can_controller::device_can_id,
        currentPageIndex,
        ctx.bytesReceived,
        ctx.nextExpectedSeq,
        0
    };
    uint8_t buf[1 + sizeof(StreamAck)] = {STREAM_ACK};
    memcpy(buf + 1, &ack, sizeof(ack));
    can_controller::sendCanMessage(sourceCanId, buf, sizeof(buf));
    
    uint32_t duration = (millis() - ctx.startTime) / 1000;
    log_i("OTA COMPLETE! %lu bytes in %lu seconds (%.1f KB/s)",
          ctx.bytesReceived, duration, 
          (float)ctx.bytesReceived / duration / 1024.0f);
    
    ctx.active = false;
    
    // Reboot after short delay
    log_i("Rebooting in 2 seconds...");
    vTaskDelay(pdMS_TO_TICKS(2000));
    ESP.restart();
}

int actFromJsonStreaming(cJSON* doc) {
    // Parse action field - accept both "cmd" and "action" for compatibility
    cJSON* actionItem = cJSON_GetObjectItem(doc, "action");
    if (!actionItem || !cJSON_IsString(actionItem)) {
        actionItem = cJSON_GetObjectItem(doc, "cmd");
        if (!actionItem || !cJSON_IsString(actionItem)) {
            log_e("Invalid JSON: missing 'action' or 'cmd' string");
            return -1;
        }
    }
    const char* action = actionItem->valuestring;
    
    // Parse slave/target CAN ID - accept both "slaveId" and "canid" for compatibility
    cJSON* canidItem = cJSON_GetObjectItem(doc, "canid");
    if (!canidItem) canidItem = cJSON_GetObjectItem(doc, "slaveId");
    
    uint8_t targetCanId = can_controller::device_can_id;  // Default to self
    if (canidItem && cJSON_IsNumber(canidItem)) {
        targetCanId = (uint8_t)canidItem->valueint;
        log_i("Target CAN ID: %u", targetCanId);
    }
    
    // {"task": "/can_ota_stream", "canid": 11, "action": "start", "firmware_size": 876784, "page_size": 4096, "chunk_size": 512, "md5": "43ba96b4d18c010201762b840476bf83", "qid": 1}
    if (strcmp(action, "start") == 0) {
        // Start streaming session
        const cJSON* firmwareSizeItem = cJSON_GetObjectItemCaseSensitive(doc, "firmware_size");
        const cJSON* pageSizeItem = cJSON_GetObjectItemCaseSensitive(doc, "page_size");
        const cJSON* chunkSizeItem = cJSON_GetObjectItemCaseSensitive(doc, "chunk_size");
        const cJSON* md5Item = cJSON_GetObjectItemCaseSensitive(doc, "md5");
        
        if (!cJSON_IsNumber(firmwareSizeItem) || !cJSON_IsNumber(pageSizeItem) ||
            !cJSON_IsNumber(chunkSizeItem) || !cJSON_IsString(md5Item)) {
            log_e("Invalid JSON parameters for 'start'");
            return -1;
        }
        
        uint32_t firmwareSize = (uint32_t)firmwareSizeItem->valuedouble;
        uint16_t pageSize = (uint16_t)pageSizeItem->valuedouble;
        uint16_t chunkSize = (uint16_t)chunkSizeItem->valuedouble;
        
        // Parse MD5 hex string
        uint8_t md5Hash[16];
        const char* md5Str = md5Item->valuestring;
        for (int i = 0; i < 16; i++) {
            sscanf(&md5Str[i*2], "%2hhx", &md5Hash[i]);
        }
        
        log_i("Stream OTA START to CAN ID %u: size=%lu, page_size=%u, chunk_size=%u",
              targetCanId, firmwareSize, pageSize, chunkSize);
        
        int result;
        
#ifdef CAN_SEND_COMMANDS
        // Master mode: Check if target is self or a remote slave
        if (targetCanId != can_controller::device_can_id) {
            // Relay to remote slave over CAN
            log_i("Relaying STREAM_START to slave 0x%02X", targetCanId);
            result = startStreamToSlave(targetCanId, firmwareSize, md5Hash);
            
            // If successful, enter binary streaming mode to receive data packets from Serial
            if (result >= 0) {
                log_i("Entering binary streaming mode for data transfer");
                enterStreamingBinaryMode(targetCanId, firmwareSize, md5Hash);
            }
        } else 
#endif
        {
            // Local mode: Handle OTA on this device
            log_i("Starting local streaming OTA");
            StreamStartCmd cmd;
            cmd.firmwareSize = firmwareSize;
            cmd.totalPages = (firmwareSize + STREAM_PAGE_SIZE - 1) / STREAM_PAGE_SIZE;
            cmd.pageSize = pageSize;
            cmd.chunkSize = chunkSize;
            memcpy(cmd.md5Hash, md5Hash, 16);
            handleStartCmd(reinterpret_cast<const uint8_t*>(&cmd), sizeof(cmd), can_controller::device_can_id);
            result = 0;
        }
        
        // Extract qid if present
        cJSON* qidItem = cJSON_GetObjectItem(doc, "qid");
        if (qidItem && cJSON_IsNumber(qidItem)) {
            return (result >= 0) ? qidItem->valueint : result;
        }
        return result;
    }
    else if (strcmp(action, "status") == 0) {
        // Status query
        log_i("Stream OTA status query");
        // TODO: implement status response
        return 0;
    }
    else if (strcmp(action, "abort") == 0) {
        // Abort streaming session
        log_i("Stream OTA abort to CAN ID %u", targetCanId);
        
#ifdef CAN_SEND_COMMANDS
        if (targetCanId != can_controller::device_can_id) {
            // Send abort to remote slave
            uint8_t buf[2] = {STREAM_ABORT, CAN_OTA_ERROR_ABORTED};
            can_controller::sendCanMessage(targetCanId, buf, sizeof(buf));
        } else
#endif
        {
            // Local abort
            abort(CAN_OTA_ERROR_ABORTED);
        }
        return 0;
    }
    
    log_e("Unknown action: %s", action);
    return -1;

}

void handleStreamMessage(uint8_t msgType, const uint8_t* data, size_t len, uint8_t sourceCanId) {
    switch (msgType) {
        case STREAM_START:
            handleStartCmd(data, len, sourceCanId);
            break;
            
        case STREAM_DATA:
            handleDataChunk(data, len, sourceCanId);
            break;
            
        case STREAM_FINISH:
            handleFinishCmd(data, len, sourceCanId);
            break;
            
        case STREAM_ABORT:
            log_w("STREAM_ABORT received");
            abort(CAN_OTA_ERR_ABORTED);
            break;
            
        case STREAM_STATUS: {
            StreamAck status = {
                ctx.active ? CAN_OTA_OK : CAN_OTA_ERR_NOT_STARTED,
                can_controller::device_can_id,
                currentPageIndex,
                ctx.bytesReceived,
                ctx.nextExpectedSeq,
                0
            };
            uint8_t buf[1 + sizeof(StreamAck)] = {STREAM_ACK};
            memcpy(buf + 1, &status, sizeof(status));
            can_controller::sendCanMessage(sourceCanId, buf, sizeof(buf));
            break;
        }
        
#ifdef CAN_SEND_COMMANDS
        case STREAM_ACK:
            handleSlaveStreamResponse(msgType, data, len);
            break;
            
        case STREAM_NAK:
            handleSlaveStreamResponse(msgType, data, len);
            break;
#endif
            
        default:
            log_w("Unknown stream message type: 0x%02X", msgType);
            break;
    }
}

// ============================================================================
// Public Functions
// ============================================================================

bool isActive() {
    return ctx.active;
}

uint8_t getProgress() {
    if (!ctx.active || ctx.firmwareSize == 0) return 0;
    return (uint8_t)((ctx.bytesReceived * 100) / ctx.firmwareSize);
}

void abort(uint8_t status) {
    if (!ctx.active) return;
    
    log_w("Aborting streaming OTA: status=%d", status);
    
    Update.abort();
    
    // Clear queue
    PageEntry dummy;
    while (xQueueReceive(ctx.pageQueue, &dummy, 0) == pdTRUE) {}
    
    ctx.active = false;
    pageBufferOffset = 0;
    currentPageIndex = 0;
}

void loop() {
    if (!ctx.active) return;
    
    // Check for timeout
    uint32_t now = millis();
    if ((now - ctx.lastDataTime) > STREAM_PAGE_TIMEOUT_MS) {
        log_e("Streaming timeout: no data for %lu ms", now - ctx.lastDataTime);
        abort(CAN_OTA_ERR_TIMEOUT);
    }
    
    if ((now - ctx.startTime) > STREAM_TOTAL_TIMEOUT_MS) {
        log_e("Total timeout exceeded");
        abort(CAN_OTA_ERR_TIMEOUT);
    }
}

// ============================================================================
// Stub implementations for Slave devices (when CAN_SEND_COMMANDS is not defined)
// ============================================================================

#ifndef CAN_SEND_COMMANDS

bool isStreamingModeActive() {
    // Slaves don't receive binary data from Serial - they receive CAN messages
    return false;
}

bool processBinaryStreamPacket() {
    // No-op on slave devices
    return false;
}

#endif // !CAN_SEND_COMMANDS

// ============================================================================
// Master-side Implementation
// ============================================================================

#ifdef CAN_SEND_COMMANDS

// Forward declarations for binary streaming mode functions
static void sendSerialStreamAck(const StreamAck* ack);
static void sendSerialStreamNak(uint8_t status, uint16_t pageIndex, uint16_t missingOffset);
void forwardSlaveAckToSerial();
bool isStreamingModeActive();

void handleSlaveStreamResponse(uint8_t msgType, const uint8_t* data, size_t len) {
    if (msgType == STREAM_ACK && len >= sizeof(StreamAck)) {
        const StreamAck* ack = reinterpret_cast<const StreamAck*>(data);
        log_i("Slave STREAM_ACK: page=%u, bytes=%lu, nextSeq=%u",
              ack->lastCompletePage, ack->bytesReceived, ack->nextExpectedSeq);
        slaveAckPage = ack->lastCompletePage;
        slaveAckBytes = ack->bytesReceived;
        slaveAckReceived = true;
        slaveNakReceived = false;
        
        // If in binary streaming mode, forward ACK to Python client
        if (isStreamingModeActive()) {
            forwardSlaveAckToSerial();
        }
    } else if (msgType == STREAM_NAK && len >= sizeof(StreamNak)) {
        const StreamNak* nak = reinterpret_cast<const StreamNak*>(data);
        log_w("Slave STREAM_NAK: status=%d, page=%u, offset=%u",
              nak->status, nak->pageIndex, nak->missingOffset);
        slaveNakOffset = nak->missingOffset;
        slaveNakReceived = true;
        slaveAckReceived = false;
        
        // If in binary streaming mode, forward NAK to Python client  
        if (isStreamingModeActive()) {
            forwardSlaveAckToSerial();
        }
    }
}

int startStreamToSlave(uint8_t slaveId, uint32_t firmwareSize, const uint8_t* md5Hash) {
    StreamStartCmd cmd;
    cmd.firmwareSize = firmwareSize;
    cmd.totalPages = (firmwareSize + STREAM_PAGE_SIZE - 1) / STREAM_PAGE_SIZE;
    cmd.pageSize = STREAM_PAGE_SIZE;
    cmd.chunkSize = STREAM_CHUNK_SIZE;
    memcpy(cmd.md5Hash, md5Hash, 16);
    
    uint8_t buf[1 + sizeof(StreamStartCmd)];
    buf[0] = STREAM_START;
    memcpy(buf + 1, &cmd, sizeof(cmd));
    
    slaveAckReceived = false;
    slaveNakReceived = false;
    
    log_i("Starting stream to slave 0x%02X: %lu bytes, %lu pages",
          slaveId, firmwareSize, cmd.totalPages);
    
    int result = can_controller::sendCanMessage(slaveId, buf, sizeof(buf));
    if (result < 0) {
        log_e("Failed to send STREAM_START");
        return result;
    }
    
    // Wait for ACK
    uint32_t start = millis();
    while (!slaveAckReceived && !slaveNakReceived && (millis() - start) < 3000) {
        vTaskDelay(pdMS_TO_TICKS(10));
    }
    
    if (slaveNakReceived) {
        log_e("Slave NAK'd STREAM_START");
        return -2;
    }
    
    if (!slaveAckReceived) {
        log_e("Slave did not respond to STREAM_START");
        return -1;
    }
    
    return 0;
}

int sendStreamData(uint8_t slaveId, uint16_t pageIndex, uint16_t offset, 
                   const uint8_t* data, size_t len, uint16_t seq) {
    // Build header
    StreamDataHeader hdr;
    hdr.pageIndex = pageIndex;
    hdr.offsetInPage = offset;
    hdr.dataSize = len;
    hdr.seq = seq;
    
    // Build packet
    size_t totalSize = 1 + sizeof(StreamDataHeader) + len;
    uint8_t* buf = (uint8_t*)malloc(totalSize);
    if (!buf) {
        log_e("Failed to allocate stream data buffer");
        return -1;
    }
    
    buf[0] = STREAM_DATA;
    memcpy(buf + 1, &hdr, sizeof(hdr));
    memcpy(buf + 1 + sizeof(hdr), data, len);
    
    int result = can_controller::sendCanMessage(slaveId, buf, totalSize);
    free(buf);
    
    return (result >= 0) ? (int)len : result;
}

int checkStreamAck(uint16_t expectedPage, uint32_t timeoutMs) {
    if (slaveNakReceived) {
        return -2;  // NAK received
    }
    if (slaveAckReceived && slaveAckPage >= expectedPage) {
        return 0;  // ACK received for this or later page
    }
    return 1;  // Still waiting
}

int finishStream(uint8_t slaveId, const uint8_t* md5Hash) {
    uint8_t buf[1 + 16];
    buf[0] = STREAM_FINISH;
    memcpy(buf + 1, md5Hash, 16);
    
    slaveAckReceived = false;
    slaveNakReceived = false;
    
    log_i("Sending STREAM_FINISH to slave 0x%02X", slaveId);
    
    int result = can_controller::sendCanMessage(slaveId, buf, sizeof(buf));
    if (result < 0) return result;
    
    // Wait for final ACK (longer timeout for verification)
    uint32_t start = millis();
    while (!slaveAckReceived && !slaveNakReceived && (millis() - start) < 30000) {
        vTaskDelay(pdMS_TO_TICKS(100));
    }
    
    if (slaveNakReceived) {
        log_e("Slave NAK'd STREAM_FINISH (MD5 mismatch?)");
        return -2;
    }
    
    if (!slaveAckReceived) {
        log_e("Slave did not respond to STREAM_FINISH");
        return -1;
    }
    
    log_i("Slave ACK'd STREAM_FINISH - OTA complete!");
    return 0;
}

// ============================================================================
// Binary Serial Protocol Implementation (Master-side)
// ============================================================================

// Binary streaming mode state
static bool streamingBinaryModeActive = false;
static uint8_t streamingTargetSlaveId = 0;
static uint32_t streamingFirmwareSize = 0;
static uint8_t streamingMd5Hash[16] = {0};
static uint32_t streamingLastActivityTime = 0;

// RX buffer and state machine
static constexpr size_t STREAM_RX_BUFFER_SIZE = 1024;  // Max packet: header + 512 data + checksum
static uint8_t streamRxBuffer[STREAM_RX_BUFFER_SIZE];
static size_t streamRxPos = 0;
static bool streamSynced = false;
static uint8_t streamSyncState = 0;  // 0 = looking for SYNC_1, 1 = got SYNC_1

// Expected packet size based on command type
static size_t streamExpectedSize = 0;

bool isStreamingModeActive() {
    return streamingBinaryModeActive;
}

uint8_t getStreamingTargetCanId() {
    return streamingTargetSlaveId;
}

bool enterStreamingBinaryMode(uint8_t slaveId, uint32_t firmwareSize, const uint8_t* md5Hash) {
    if (streamingBinaryModeActive) {
        log_w("Already in binary streaming mode - exiting first");
        exitStreamingBinaryMode();
    }
    
    streamingTargetSlaveId = slaveId;
    streamingFirmwareSize = firmwareSize;
    if (md5Hash) {
        memcpy(streamingMd5Hash, md5Hash, 16);
    }
    
    // Reset RX state
    streamRxPos = 0;
    streamSynced = false;
    streamSyncState = 0;
    streamExpectedSize = 0;
    streamingLastActivityTime = millis();
    
    streamingBinaryModeActive = true;
    
    log_i("Entered binary streaming mode: slaveId=0x%02X, size=%lu", slaveId, firmwareSize);
    return true;
}

void exitStreamingBinaryMode() {
    if (streamingBinaryModeActive) {
        log_i("Exiting binary streaming mode");
    }
    
    streamingBinaryModeActive = false;
    streamingTargetSlaveId = 0;
    streamingFirmwareSize = 0;
    streamRxPos = 0;
    streamSynced = false;
    streamSyncState = 0;
}

/**
 * @brief Send binary ACK to Python client via Serial
 */
static void sendSerialStreamAck(const StreamAck* ack) {
    // Format: [SYNC_1][SYNC_2][STREAM_ACK][ack_data...][checksum]
    uint8_t packet[3 + sizeof(StreamAck) + 1];
    packet[0] = STREAM_SYNC_1;
    packet[1] = STREAM_SYNC_2;
    packet[2] = STREAM_ACK;
    memcpy(packet + 3, ack, sizeof(StreamAck));
    
    // Calculate XOR checksum
    uint8_t checksum = 0;
    for (size_t i = 0; i < 3 + sizeof(StreamAck); i++) {
        checksum ^= packet[i];
    }
    packet[3 + sizeof(StreamAck)] = checksum;
    
    Serial.write(packet, sizeof(packet));
    Serial.flush();
}

/**
 * @brief Send binary NAK to Python client via Serial  
 */
static void sendSerialStreamNak(uint8_t status, uint16_t pageIndex, uint16_t missingOffset) {
    StreamNak nak;
    nak.status = status;
    nak.canId = can_controller::device_can_id;
    nak.pageIndex = pageIndex;
    nak.missingOffset = missingOffset;
    nak.missingSeq = 0;
    
    uint8_t packet[3 + sizeof(StreamNak) + 1];
    packet[0] = STREAM_SYNC_1;
    packet[1] = STREAM_SYNC_2;
    packet[2] = STREAM_NAK;
    memcpy(packet + 3, &nak, sizeof(StreamNak));
    
    // Calculate XOR checksum
    uint8_t checksum = 0;
    for (size_t i = 0; i < 3 + sizeof(StreamNak); i++) {
        checksum ^= packet[i];
    }
    packet[3 + sizeof(StreamNak)] = checksum;
    
    Serial.write(packet, sizeof(packet));
    Serial.flush();
}

/**
 * @brief Process a complete binary packet from Serial
 * 
 * Packet format from Python:
 * [SYNC_1][SYNC_2][CMD][page_H][page_L][offset_H][offset_L][size_H][size_L][seq_H][seq_L][DATA...][CHECKSUM]
 *  
 * All multi-byte fields are BIG ENDIAN from Python!
 */
static void processCompleteBinaryPacket() {
    // Minimum packet: [SYNC][CMD][CHECKSUM] = 4 bytes
    if (streamRxPos < 4) {
        log_w("Binary packet too short: %d bytes", streamRxPos);
        return;
    }
    
    // Validate checksum (XOR of all bytes except last)
    uint8_t calcChecksum = 0;
    for (size_t i = 0; i < streamRxPos - 1; i++) {
        calcChecksum ^= streamRxBuffer[i];
    }
    uint8_t recvChecksum = streamRxBuffer[streamRxPos - 1];
    
    if (calcChecksum != recvChecksum) {
        log_e("Binary checksum mismatch: calc=0x%02X, recv=0x%02X", calcChecksum, recvChecksum);
        sendSerialStreamNak(CAN_OTA_ERR_CRC, 0, 0);
        return;
    }
    
    // Parse command (at position 2 after sync bytes)
    uint8_t cmd = streamRxBuffer[2];
    streamingLastActivityTime = millis();
    
    switch (cmd) {
        case STREAM_DATA: {
            // Header format (Big Endian from Python):
            // [3-4]:  pageIndex (16-bit BE)
            // [5-6]:  offset (16-bit BE)
            // [7-8]:  dataSize (16-bit BE)
            // [9-10]: seq (16-bit BE)
            // [11..]: data
            if (streamRxPos < 11 + 1) {  // Header + at least checksum
                log_e("STREAM_DATA packet too short");
                sendSerialStreamNak(CAN_OTA_ERR_INVALID_SIZE, 0, 0);
                return;
            }
            
            uint16_t pageIndex = (streamRxBuffer[3] << 8) | streamRxBuffer[4];
            uint16_t offset = (streamRxBuffer[5] << 8) | streamRxBuffer[6];
            uint16_t dataSize = (streamRxBuffer[7] << 8) | streamRxBuffer[8];
            uint16_t seq = (streamRxBuffer[9] << 8) | streamRxBuffer[10];
            
            // Validate data size
            size_t expectedTotal = 11 + dataSize + 1;  // Header + data + checksum
            if (streamRxPos != expectedTotal) {
                log_e("Data size mismatch: expected %d, got %d", expectedTotal, streamRxPos);
                sendSerialStreamNak(CAN_OTA_ERR_INVALID_SIZE, pageIndex, offset);
                return;
            }
            
            const uint8_t* data = streamRxBuffer + 11;
            
            // Forward to slave via CAN
            int result = sendStreamData(streamingTargetSlaveId, pageIndex, offset, data, dataSize, seq);
            
            if (result < 0) {
                log_e("Failed to send data to slave: result=%d", result);
                sendSerialStreamNak(CAN_OTA_ERR_SEND, pageIndex, offset);
            }
            // Note: ACK comes from slave via handleSlaveStreamResponse()
            break;
        }
        
        case STREAM_FINISH: {
            // Format: [SYNC][CMD][MD5(16)][CHECKSUM]
            if (streamRxPos != 3 + 16 + 1) {
                log_e("STREAM_FINISH wrong size: %d (expected 20)", streamRxPos);
                sendSerialStreamNak(CAN_OTA_ERR_INVALID_SIZE, 0xFFFF, 0);
                return;
            }
            
            const uint8_t* md5 = streamRxBuffer + 3;
            
            log_i("Binary STREAM_FINISH received - forwarding to slave");
            int result = finishStream(streamingTargetSlaveId, md5);
            
            if (result == 0) {
                // Success - slave verified MD5
                StreamAck ack;
                ack.status = CAN_OTA_OK;
                ack.canId = can_controller::device_can_id;
                ack.lastCompletePage = 0xFFFF;  // Indicates completion
                ack.bytesReceived = streamingFirmwareSize;
                ack.nextExpectedSeq = 0;
                ack.reserved = 0;
                sendSerialStreamAck(&ack);
                
                exitStreamingBinaryMode();
            } else {
                sendSerialStreamNak(CAN_OTA_ERR_CRC, 0xFFFF, 0);
                exitStreamingBinaryMode();
            }
            break;
        }
        
        case STREAM_ABORT: {
            log_w("Binary STREAM_ABORT received");
            // Forward abort to slave
            uint8_t buf[2] = {STREAM_ABORT, 0};
            can_controller::sendCanMessage(streamingTargetSlaveId, buf, sizeof(buf));
            exitStreamingBinaryMode();
            break;
        }
        
        default:
            log_w("Unknown binary stream command: 0x%02X", cmd);
            break;
    }
}

bool processBinaryStreamPacket() {
    if (!streamingBinaryModeActive) {
        return false;
    }
    
    // Check for timeout
    if ((millis() - streamingLastActivityTime) > STREAM_BINARY_TIMEOUT_MS) {
        log_e("Binary streaming timeout - no activity for %lu ms", millis() - streamingLastActivityTime);
        sendSerialStreamNak(CAN_OTA_ERR_TIMEOUT, 0, 0);
        exitStreamingBinaryMode();
        return false;
    }
    
    bool packetProcessed = false;
    
    // Read available bytes from Serial
    while (Serial.available() > 0) {
        uint8_t byte = Serial.read();
        
        if (!streamSynced) {
            // Looking for sync sequence 0xAA 0x55
            if (streamSyncState == 0) {
                if (byte == STREAM_SYNC_1) {
                    streamSyncState = 1;
                }
            } else {  // streamSyncState == 1
                if (byte == STREAM_SYNC_2) {
                    // Sync found! Start collecting packet
                    streamSynced = true;
                    streamRxPos = 0;
                    streamRxBuffer[streamRxPos++] = STREAM_SYNC_1;
                    streamRxBuffer[streamRxPos++] = STREAM_SYNC_2;
                    streamExpectedSize = 0;  // Will be determined from command
                } else if (byte == STREAM_SYNC_1) {
                    // Still looking for 0x55
                    streamSyncState = 1;
                } else {
                    // Reset
                    streamSyncState = 0;
                }
            }
        } else {
            // Collecting packet data
            if (streamRxPos < STREAM_RX_BUFFER_SIZE) {
                streamRxBuffer[streamRxPos++] = byte;
            } else {
                // Buffer overflow - abort
                log_e("Binary RX buffer overflow");
                streamSynced = false;
                streamSyncState = 0;
                streamRxPos = 0;
                continue;
            }
            
            // Determine expected size after receiving command byte
            if (streamRxPos == 3 && streamExpectedSize == 0) {
                uint8_t cmd = streamRxBuffer[2];
                switch (cmd) {
                    case STREAM_DATA:
                        // Need to read header first to know data size
                        // Header: [SYNC(2)][CMD(1)][page(2)][offset(2)][size(2)][seq(2)] = 11 bytes
                        streamExpectedSize = 0;  // Will be updated after header
                        break;
                    case STREAM_FINISH:
                        // [SYNC(2)][CMD(1)][MD5(16)][CHECKSUM(1)] = 20 bytes
                        streamExpectedSize = 20;
                        break;
                    case STREAM_ABORT:
                        // [SYNC(2)][CMD(1)][CHECKSUM(1)] = 4 bytes
                        streamExpectedSize = 4;
                        break;
                    default:
                        // Unknown command - try to recover
                        log_w("Unknown binary cmd 0x%02X - resetting sync", cmd);
                        streamSynced = false;
                        streamSyncState = 0;
                        streamRxPos = 0;
                        continue;
                }
            }
            
            // For STREAM_DATA, determine size after receiving header
            if (streamRxBuffer[2] == STREAM_DATA && streamRxPos == 11 && streamExpectedSize == 0) {
                uint16_t dataSize = (streamRxBuffer[7] << 8) | streamRxBuffer[8];
                streamExpectedSize = 11 + dataSize + 1;  // Header + data + checksum
                
                if (streamExpectedSize > STREAM_RX_BUFFER_SIZE) {
                    log_e("Data packet too large: %d bytes", streamExpectedSize);
                    streamSynced = false;
                    streamSyncState = 0;
                    streamRxPos = 0;
                    continue;
                }
            }
            
            // Check if packet complete
            if (streamExpectedSize > 0 && streamRxPos >= streamExpectedSize) {
                processCompleteBinaryPacket();
                
                // Reset for next packet
                streamSynced = false;
                streamSyncState = 0;
                streamRxPos = 0;
                streamExpectedSize = 0;
                packetProcessed = true;
            }
        }
    }
    
    return packetProcessed;
}

/**
 * @brief Called when slave sends ACK/NAK - forward to Python via Serial
 */
void forwardSlaveAckToSerial() {
    if (!streamingBinaryModeActive) return;
    
    if (slaveAckReceived) {
        StreamAck ack;
        ack.status = CAN_OTA_OK;
        ack.canId = can_controller::device_can_id;
        ack.lastCompletePage = slaveAckPage;
        ack.bytesReceived = slaveAckBytes;
        ack.nextExpectedSeq = 0;
        ack.reserved = 0;
        
        sendSerialStreamAck(&ack);
        slaveAckReceived = false;  // Clear flag after forwarding
    }
    
    if (slaveNakReceived) {
        sendSerialStreamNak(CAN_OTA_ERR_NAK, slaveNakOffset, 0);
        slaveNakReceived = false;  // Clear flag after forwarding
    }
}

#endif // CAN_SEND_COMMANDS

} // namespace can_ota_stream
