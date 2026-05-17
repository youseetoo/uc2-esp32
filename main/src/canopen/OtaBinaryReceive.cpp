/**
 * @file OtaBinaryReceive.cpp
 * @brief Master-side serial → CAN OTA binary receive bridge (streaming).
 *
 * Receives a firmware blob from the host over Serial and forwards it to a
 * remote slave over CANopen SDO without ever buffering the full image.
 * The image is consumed in 4 KB chunks; each chunk is pushed to the slave
 * via CANopenModule::sdoDownloadChunk while the next chunk is read from
 * Serial. Memory cost stays constant (~4 KB) regardless of firmware size.
 */
#include "OtaBinaryReceive.h"
#include "PinConfig.h"

#ifdef CAN_CONTROLLER_CANOPEN

#include "CANopenModule.h"
#include "UC2_OD_Indices.h"
#include <Arduino.h>
#include <esp_log.h>
#include <rom/crc.h>

#define TAG "OTA_RX"

namespace {

// Streaming chunk buffer in internal DRAM. 4 KB matches the ESP32 flash
// page size and is small enough to fit on any ESP32 build (no PSRAM
// required). The total firmware size is irrelevant — we never hold more
// than one chunk in RAM.
constexpr size_t   CHUNK_SIZE          = 4096;
constexpr uint32_t ACK_INTERVAL_BYTES  = 4096;
constexpr uint32_t RX_TIMEOUT_MS       = 30000;

uint8_t  s_chunk[CHUNK_SIZE];
size_t   s_chunkPos        = 0;

bool     s_active          = false;
bool     s_streamOpen      = false;
uint32_t s_totalSize       = 0;
uint32_t s_expectedCrc32   = 0;
uint32_t s_runningCrc32    = 0;
uint8_t  s_nodeId          = 0;
uint32_t s_bytesReceived   = 0;
uint32_t s_lastAckBytes    = 0;
uint32_t s_lastByteMillis  = 0;

void emitJson(const char* msg) {
    Serial.write(msg);
    Serial.write('\n');
}

void cleanup() {
    if (s_streamOpen) {
        CANopenModule::sdoDownloadAbort();
        s_streamOpen = false;
    }
    s_active         = false;
    s_totalSize      = 0;
    s_expectedCrc32  = 0;
    s_runningCrc32   = 0;
    s_nodeId         = 0;
    s_bytesReceived  = 0;
    s_lastAckBytes   = 0;
    s_chunkPos       = 0;
}

// Push the buffered chunk to the slave. On the first call also primes the
// slave with CRC32 + size and opens the streaming SDO session for
// OTA_FIRMWARE_DATA. Returns false on error (and emits an error JSON).
bool flushChunk() {
    if (s_chunkPos == 0) {
        log_i("flushChunk called with empty chunk, ignoring");
        return true;
    }

    if (!s_streamOpen) {
        // 1) Tell the slave the expected CRC32 (so it can verify the image).
        // Use a 5 s SDO timeout: the CRC write itself is cheap, but the
        // slave's CO_main_task may be momentarily busy responding to other
        // traffic, and we want to be tolerant of jitter.
        if (!CANopenModule::writeSDO_u32(s_nodeId, UC2_OD::OTA_FIRMWARE_CRC32,
                                         0, s_expectedCrc32,
                                         /*timeoutMs=*/5000)) {
            emitJson("{\"ota_status\":\"error\",\"error\":\"crc_write_failed\"}");
            return false;
        }

        // 2) Tell the slave the firmware size — this triggers esp_ota_begin
        //    on the slave which blocks ~400 ms for the flash partition
        //    erase. The slave can't respond to the SDO until the OD write
        //    handler returns, so the master needs a generous timeout here.
        //    The default 250 ms is way too tight and causes intermittent
        //    "size_write_failed" — depending on master scheduler jitter the
        //    250 logical-ticks-of-1 ms can elapse in 250 ms wall time
        //    (master idle) or 600 ms wall time (master busy). 5000 ms is
        //    plenty for any reasonable flash partition size.
        if (!CANopenModule::writeSDO_u32(s_nodeId, UC2_OD::OTA_FIRMWARE_SIZE,
                                         0, s_totalSize,
                                         /*timeoutMs=*/5000)) {
            emitJson("{\"ota_status\":\"error\",\"error\":\"size_write_failed\"}");
            return false;
        }
        // Small post-write settle: by now esp_ota_begin has already
        // returned (the SDO ACK we just received confirmed that), but give
        // the slave's CAN stack a few RTOS ticks to drain any backlog
        // before we start streaming the block download.
        vTaskDelay(pdMS_TO_TICKS(100));

        // 3) Open the streaming SDO session for the firmware blob.
        if (!CANopenModule::sdoDownloadBegin(s_nodeId,
                UC2_OD::OTA_FIRMWARE_DATA, 0, s_totalSize)) {
            emitJson("{\"ota_status\":\"error\",\"error\":\"sdo_begin_failed\"}");
            return false;
        }
        s_streamOpen = true;
    }

    //CANopenModule::sdoDownloadChunk(s_chunk, s_chunkPos);
    if (!CANopenModule::sdoDownloadChunk(s_chunk, s_chunkPos)) { // TODO: What exactly is this doing? Does it push the chunk to the slave?
        emitJson("{\"ota_status\":\"error\",\"error\":\"sdo_chunk_failed\"}");
        s_streamOpen = false;  // sdoDownloadChunk already aborted on failure
        return false;
    }

    // Update running CRC32 over the bytes actually pushed downstream.
    s_runningCrc32 = crc32_le(s_runningCrc32, s_chunk, s_chunkPos);
    s_chunkPos = 0;
    return true;
}

}  // namespace

namespace OtaBinaryReceive {

cJSON* begin(uint8_t nodeId, uint32_t size, uint32_t crc32) {
    // This is most likiley fired from the master
    log_i("OTA begin: nodeId=%u size=%u crc32=0x%08lX",
          nodeId, (unsigned)size, (unsigned long)crc32);
    cJSON* resp = cJSON_CreateObject();
    if (!resp) return nullptr;

    if (s_active) {
        // A previous run is wedged (e.g. host died mid-transfer and we
        // never saw a final byte; or the master rebooted before cleanup
        // ran). Reset to a clean state so we don't reject this request.
        log_i("OTA: previous session still marked active, forcing cleanup");
        cleanup();
    }
    if (CANopenModule::sdoDownloadActive()) {
        // SDO mutex still held from a wedged previous stream — release it
        // so sdoDownloadBegin() below can take the mutex.
        log_i("OTA: stale SDO stream detected, aborting it");
        CANopenModule::sdoDownloadAbort();
    }
    if (size == 0 || size > 8 * 1024 * 1024) {
        log_e("Invalid OTA size: %u", (unsigned)size);
        cJSON_AddStringToObject(resp, "ota_status", "error");
        cJSON_AddStringToObject(resp, "error", "invalid_size");
        return resp;
    }

    // Confirm the slave is reachable BEFORE telling the host "ready".
    // Otherwise the host streams the first 4 KB into the master while the
    // master can't yet reach the slave — by the time flushChunk runs the
    // host has already disconnected on size_write_failed.
    if (!CANopenModule::waitForNodeReachable(nodeId, 10000)) {
        log_e("OTA: slave 0x%02X not reachable after 10s", nodeId);
        cJSON_AddStringToObject(resp, "ota_status", "error");
        cJSON_AddStringToObject(resp, "error", "slave_unreachable");
        return resp;
    }

    // Streaming session — no large allocation. The 4 KB s_chunk buffer
    // lives in BSS  and is reused for every transfer.
    s_totalSize       = size;
    s_expectedCrc32   = crc32;
    s_runningCrc32    = 0;
    s_nodeId          = nodeId;
    s_bytesReceived   = 0;
    s_lastAckBytes    = 0;
    s_lastByteMillis  = millis();
    s_chunkPos        = 0;
    s_streamOpen      = false;
    s_active          = true;

    log_i("OTA session ready (streaming, %u-byte chunks)", (unsigned)CHUNK_SIZE);

    cJSON_AddStringToObject(resp, "ota_status", "ready");
    cJSON_AddNumberToObject(resp, "size",      (double)size);
    cJSON_AddNumberToObject(resp, "nodeId",    nodeId);
    cJSON_AddNumberToObject(resp, "chunkSize", (double)CHUNK_SIZE);
    return resp;
}

bool isActive() { return s_active; }

void processBytes() {
    if (!s_active) return;

    // Drain as much as possible this call - NOT just one FIFO-worth.
    // The host writes 4096 B at once; if we only drain ~256 B per main-loop
    // iteration, the UART ring buffer overflows and bytes are lost silently
    // (firmware ends up corrupt, only caught by the slave-side CRC).
    bool didRead = false;
    while (Serial.available() > 0 && s_bytesReceived < s_totalSize) {
        uint32_t remaining = s_totalSize - s_bytesReceived;
        size_t   space     = CHUNK_SIZE - s_chunkPos;
        size_t   avail     = (size_t)Serial.available();
        size_t   toRead    = avail;
        if (toRead > remaining) toRead = remaining;
        if (toRead > space)     toRead = space;
        if (toRead == 0) break;  // chunk is full, must flush before reading more

        size_t nRead = Serial.readBytes(s_chunk + s_chunkPos, toRead);
        if (nRead == 0) break;
        s_chunkPos      += nRead;
        s_bytesReceived += nRead;
        s_lastByteMillis = millis();
        didRead = true;

        // Flush as soon as the chunk is full or the last firmware byte landed.
        bool isLast = (s_bytesReceived >= s_totalSize);
        if (s_chunkPos >= CHUNK_SIZE || (isLast && s_chunkPos > 0)) {
            if (!flushChunk()) {
                log_i("Flushing chunk failed, aborting OTA session");
                cleanup();
                return;
            }
            // After flushChunk we can keep draining the UART for the next
            // chunk in the same call - avoids waiting for the next main-loop
            // iteration just to read bytes that are already in the FIFO.
        }
    }

    if (didRead) {
        // Progress ACK every ACK_INTERVAL_BYTES (or at completion).
        if ((s_bytesReceived - s_lastAckBytes) >= ACK_INTERVAL_BYTES ||
            s_bytesReceived >= s_totalSize) {
            s_lastAckBytes = s_bytesReceived;
            char ack[48];
            snprintf(ack, sizeof(ack), "{\"ota_rx\":%lu}",
                     (unsigned long)s_bytesReceived);
            emitJson(ack);
        }
    } else {
        // Watchdog: abort if the host stalls mid-transfer.
        if ((millis() - s_lastByteMillis) > RX_TIMEOUT_MS) {
            ESP_LOGE(TAG, "OTA receive timeout after %u/%u bytes",
                     (unsigned)s_bytesReceived, (unsigned)s_totalSize);
            emitJson("{\"ota_status\":\"error\",\"error\":\"rx_timeout\"}");
            cleanup();
            return;
        }
    }

    if (s_bytesReceived < s_totalSize) return;

    // All bytes received and queued. Verify the master-side CRC32 (catches
    // host→master serial corruption before we waste minutes verifying on
    // the slave).
    if (s_expectedCrc32 != 0 && s_runningCrc32 != s_expectedCrc32) {
        char err[112];
        snprintf(err, sizeof(err),
                 "{\"ota_status\":\"error\",\"error\":\"crc_mismatch\","
                 "\"computed\":\"0x%08lX\",\"expected\":\"0x%08lX\","
                 "\"comment\":\"Host→master serial corruption?\"}",
                 (unsigned long)s_runningCrc32,
                 (unsigned long)s_expectedCrc32);
        emitJson(err);
        cleanup();
        return;
    }

    emitJson("{\"ota_status\":\"flashing\"}");

    // Finalise the SDO session — the slave now runs its own CRC check,
    // calls esp_ota_end + set_boot_partition + reboot.
    bool ok = CANopenModule::sdoDownloadEnd();
    s_streamOpen = false;

    if (ok) {
        emitJson("{\"ota_status\":\"success\"}");
    } else {
        emitJson("{\"ota_status\":\"error\",\"error\":\"sdo_end_failed\"}");
    }

    cleanup();
}

}  // namespace OtaBinaryReceive

#endif  // CAN_CONTROLLER_CANOPEN
