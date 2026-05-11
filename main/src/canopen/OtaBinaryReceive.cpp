/**
 * @file OtaBinaryReceive.cpp
 * @brief Master-side serial → CAN OTA binary receive bridge.
 */
#include "OtaBinaryReceive.h"
#include "PinConfig.h"

#ifdef CAN_CONTROLLER_CANOPEN

#include "CanOpenOTAStreaming.h"
#include <Arduino.h>
#include <esp_heap_caps.h>
#include <esp_log.h>
#include <rom/crc.h>

#define TAG "OTA_RX"

namespace {

bool      s_active           = false;
uint8_t*  s_buffer           = nullptr;
uint32_t  s_size             = 0;
uint32_t  s_crc32            = 0;
uint8_t   s_nodeId           = 0;
uint32_t  s_bytesReceived    = 0;
uint32_t  s_lastAckBytes     = 0;
uint32_t  s_lastByteMillis   = 0;

constexpr uint32_t ACK_INTERVAL_BYTES = 4096;
constexpr uint32_t RX_TIMEOUT_MS      = 30000;  // abort if no bytes for 30 s

void emitJson(const char* msg) {
    Serial.write(msg);
    Serial.write('\n');
}

void cleanup() {
    if (s_buffer) {
        heap_caps_free(s_buffer);
        s_buffer = nullptr;
    }
    s_active = false;
    s_size = 0;
    s_crc32 = 0;
    s_nodeId = 0;
    s_bytesReceived = 0;
    s_lastAckBytes = 0;
    // TODO: need to free the BUS?
}

}  // namespace

namespace OtaBinaryReceive {

cJSON* begin(uint8_t nodeId, uint32_t size, uint32_t crc32) {
    log_i("OTA begin: nodeId=%u size=%u crc32=0x%08lX", nodeId, (unsigned)size, (unsigned long)crc32);
    cJSON* resp = cJSON_CreateObject();
    if (!resp) return nullptr;

    if (s_active) {
        log_i("OTA receive already in progress, rejecting new request");
        cJSON_AddStringToObject(resp, "ota_status", "error");
        cJSON_AddStringToObject(resp, "error", "ota_in_progress");
        return resp;
    }
    if (size == 0 || size > 8 * 1024 * 1024) {
        log_i("Invalid OTA size: %u", (unsigned)size);
        cJSON_AddStringToObject(resp, "ota_status", "error");
        cJSON_AddStringToObject(resp, "error", "invalid_size");
        return resp;
    }

    // Prefer PSRAM (UC2 CAN HAT v2 has it); fall back to internal heap.
    uint8_t* buf = (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
    if (!buf) {
        log_i("Failed to allocate %u bytes in PSRAM, falling back to internal heap", (unsigned)size);
        buf = (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_8BIT);
    }
    if (!buf) {
        cJSON_AddStringToObject(resp, "ota_status", "error");
        cJSON_AddStringToObject(resp, "error", "out_of_memory");
        return resp;
    }

    s_buffer        = buf;
    s_size          = size;
    s_crc32         = crc32;
    s_nodeId        = nodeId;
    s_bytesReceived = 0;
    s_lastAckBytes  = 0;
    s_lastByteMillis = millis();
    s_active        = true;

    log_i("OTA session ready: node=%u size=%u crc32=0x%08lX",
         nodeId, (unsigned)size, (unsigned long)crc32);

    cJSON_AddStringToObject(resp, "ota_status", "ready");
    cJSON_AddNumberToObject(resp, "size", (double)size);
    cJSON_AddNumberToObject(resp, "nodeId", nodeId);
    return resp;
}

bool isActive() { return s_active; }

void processBytes() {
    if (!s_active) return;

    int avail = Serial.available();
    if (avail > 0) {
        uint32_t remaining = s_size - s_bytesReceived;
        size_t toRead = (size_t)avail;
        if (toRead > remaining) toRead = remaining;

        size_t nRead = Serial.readBytes(s_buffer + s_bytesReceived, toRead);
        s_bytesReceived += nRead;
        if (nRead > 0) s_lastByteMillis = millis();

        // Progress ACK every ACK_INTERVAL_BYTES (or at completion).
        if ((s_bytesReceived - s_lastAckBytes) >= ACK_INTERVAL_BYTES ||
            s_bytesReceived >= s_size) {
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
                     (unsigned)s_bytesReceived, (unsigned)s_size);
            emitJson("{\"ota_status\":\"error\",\"error\":\"rx_timeout\"}");
            cleanup();
            return;
        }
    }

    if (s_bytesReceived < s_size) return;

    // Full payload received — verify CRC32, flash slave, then release.
    uint32_t computed = crc32_le(0, s_buffer, s_size);
    ESP_LOGI(TAG, "OTA receive complete (%u bytes), CRC32=0x%08lX (expected 0x%08lX)",
             (unsigned)s_size, (unsigned long)computed, (unsigned long)s_crc32);

    if (s_crc32 != 0 && computed != s_crc32) {
        char err[96];
        snprintf(err, sizeof(err),
                 "{\"ota_status\":\"error\",\"error\":\"crc_mismatch\","
                 "\"computed\":\"0x%08lX\",\"expected\":\"0x%08lX\"}",
                 (unsigned long)computed, (unsigned long)s_crc32);
        emitJson(err);
        cleanup();
        return;
    }

    emitJson("{\"ota_status\":\"flashing\"}");

    // Snapshot the buffer pointer/size so we can free immediately after the
    // (long-running) flash call returns.
    uint8_t* buf    = s_buffer;
    uint32_t size   = s_size;
    uint32_t crc32  = computed;
    uint8_t  nodeId = s_nodeId;
    s_buffer        = nullptr;        // ownership transferred to local var
    s_active        = false;

    bool ok = CanOpenOTAStreaming::flashSlave(nodeId, buf, size, crc32);

    heap_caps_free(buf);
    cleanup();  // also resets size/crc/nodeId

    if (ok) {
        emitJson("{\"ota_status\":\"success\"}");
    } else {
        emitJson("{\"ota_status\":\"error\",\"error\":\"flash_failed\"}");
    }
}

}  // namespace OtaBinaryReceive

#endif  // CAN_CONTROLLER_CANOPEN
