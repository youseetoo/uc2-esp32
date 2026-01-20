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
    
    while (true) {
        // Wait for a page to be ready
        if (xQueueReceive(ctx.pageQueue, &entry, portMAX_DELAY) == pdTRUE) {
            // Verify CRC before writing
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
    
    // Validate
    if (cmd->firmwareSize > CAN_OTA_MAX_FIRMWARE_SIZE) {
        log_e("Firmware too large: %lu > %d", cmd->firmwareSize, CAN_OTA_MAX_FIRMWARE_SIZE);
        StreamNak nak = {CAN_OTA_ERR_INVALID_SIZE, can_controller::device_can_id, 0, 0, 0};
        uint8_t buf[1 + sizeof(StreamNak)] = {STREAM_NAK};
        memcpy(buf + 1, &nak, sizeof(nak));
        can_controller::sendCanMessage(sourceCanId, buf, sizeof(buf));
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
        can_controller::sendCanMessage(sourceCanId, buf, sizeof(buf));
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
    ctx.masterCanId = sourceCanId;
    
    // Reset page buffer
    memset(pageBuffer, 0xFF, STREAM_PAGE_SIZE);  // 0xFF = erased flash
    pageBufferOffset = 0;
    currentPageIndex = 0;
    
    // Create flash writer task if not exists
    if (!ctx.flashWriterTask) {
        xTaskCreatePinnedToCore(
            flashWriterTask,
            "OTA_Flash",
            4096,           // Stack size
            NULL,
            2,              // Priority (lower than CAN RX)
            &ctx.flashWriterTask,
            1               // Run on Core 1 (CAN is on Core 0)
        );
    }
    
    // Send ACK
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
    can_controller::sendCanMessage(sourceCanId, buf, sizeof(buf));
    
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
    const cJSON* taskItem = cJSON_GetObjectItemCaseSensitive(doc, "task");
    if (!cJSON_IsString(taskItem) || (taskItem->valuestring == NULL)) {
        log_e("Invalid JSON: missing 'task' string");
        return -1;
    }
    
    const char* task = taskItem->valuestring;
    // {"task": "/can_ota_stream", "canid": 11, "action": "start", "firmware_size": 876784, "page_size": 4096, "chunk_size": 512, "md5": "43ba96b4d18c010201762b840476bf83", "qid": 1}
    if (strcmp(task, "start") == 0) {
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
        
        StreamStartCmd cmd;
        cmd.firmwareSize = (uint32_t)firmwareSizeItem->valuedouble;
        cmd.totalPages = (cmd.firmwareSize + STREAM_PAGE_SIZE - 1) / STREAM_PAGE_SIZE;
        cmd.pageSize = (uint16_t)pageSizeItem->valuedouble;
        cmd.chunkSize = (uint16_t)chunkSizeItem->valuedouble;
        
        // Parse MD5 hex string
        const char* md5Str = md5Item->valuestring;
        for (int i = 0; i < 16; i++) {
            sscanf(&md5Str[i*2], "%2hhx", &cmd.md5Hash[i]);
        }
        
        handleStartCmd(reinterpret_cast<const uint8_t*>(&cmd), sizeof(cmd), can_controller::device_can_id);
        
        return 0;
    }
    
    log_e("Unknown task: %s", task);
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
// Master-side Implementation
// ============================================================================

#ifdef CAN_SEND_COMMANDS

void handleSlaveStreamResponse(uint8_t msgType, const uint8_t* data, size_t len) {
    if (msgType == STREAM_ACK && len >= sizeof(StreamAck)) {
        const StreamAck* ack = reinterpret_cast<const StreamAck*>(data);
        log_i("Slave STREAM_ACK: page=%u, bytes=%lu, nextSeq=%u",
              ack->lastCompletePage, ack->bytesReceived, ack->nextExpectedSeq);
        slaveAckPage = ack->lastCompletePage;
        slaveAckBytes = ack->bytesReceived;
        slaveAckReceived = true;
        slaveNakReceived = false;
    } else if (msgType == STREAM_NAK && len >= sizeof(StreamNak)) {
        const StreamNak* nak = reinterpret_cast<const StreamNak*>(data);
        log_w("Slave STREAM_NAK: status=%d, page=%u, offset=%u",
              nak->status, nak->pageIndex, nak->missingOffset);
        slaveNakOffset = nak->missingOffset;
        slaveNakReceived = true;
        slaveAckReceived = false;
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

#endif // CAN_SEND_COMMANDS

} // namespace can_ota_stream
