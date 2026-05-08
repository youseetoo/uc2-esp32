/**
 * @file CanOpenOTA.cpp
 * @brief CANopen OTA firmware update via SDO domain transfer (slave side)
 *
 * Implements the OD write extension for 0x2F00 (OTA_FIRMWARE_DATA).
 * Each SDO segment is streamed directly to esp_ota_write(), avoiding
 * full-binary RAM buffering.
 *
 * Flow:
 *   1. Master writes firmware size → 0x2F01 (triggers OTA begin)
 *   2. Master writes CRC32        → 0x2F02 (stored for later verification)
 *   3. Master writes firmware data → 0x2F00 (domain / segmented SDO)
 *      onOtaWriteChunk() is called for each segment → esp_ota_write()
 *   4. When the last segment arrives (dataOffset+count >= dataLength),
 *      CRC32 is verified and boot partition is set.
 */

#include "CanOpenOTA.h"

#ifdef CAN_CONTROLLER_CANOPEN

#include "UC2_OD_Indices.h"
#include <CANopen.h>

extern "C" {
#include "OD.h"
}

#include <esp_ota_ops.h>
#include <esp_log.h>
#include <rom/crc.h>

#define TAG "OTA_SDO"

// ============================================================================
// Static state
// ============================================================================

static esp_ota_handle_t        s_otaHandle = 0;
static const esp_partition_t*  s_otaPartition = nullptr;
static uint32_t                s_crc32Running = 0;
static uint32_t                s_bytesWritten = 0;
static bool                    s_otaStarted = false;

static OD_extension_t s_otaDataExt;
static OD_extension_t s_otaSizeExt;

// ============================================================================
// Internal helpers
// ============================================================================

static void resetOtaState() {
    if (s_otaStarted && s_otaHandle != 0) {
        esp_ota_abort(s_otaHandle);
    }
    s_otaHandle = 0;
    s_otaPartition = nullptr;
    s_crc32Running = 0;
    s_bytesWritten = 0;
    s_otaStarted = false;
    OD_OTA_STATUS = CANOPEN_OTA_IDLE;
    OD_OTA_BYTES_RECEIVED = 0;
    OD_OTA_ERROR_CODE = CANOPEN_OTA_ERR_NONE;
}

static void setError(uint8_t errCode) {
    OD_OTA_STATUS = CANOPEN_OTA_ERROR;
    OD_OTA_ERROR_CODE = errCode;
    ESP_LOGE(TAG, "OTA error: %d", errCode);
    resetOtaState();
    // Keep error code visible after reset
    OD_OTA_STATUS = CANOPEN_OTA_ERROR;
    OD_OTA_ERROR_CODE = errCode;
}

// ============================================================================
// OD write callback for 0x2F01 (firmware size) — triggers OTA begin
// ============================================================================

static ODR_t onOtaSizeWrite(OD_stream_t* stream, const void* buf,
                            OD_size_t count, OD_size_t* countWritten) {
    // Let CANopenNode write the value to RAM first
    ODR_t ret = OD_writeOriginal(stream, buf, count, countWritten);
    if (ret != ODR_OK) return ret;

    uint32_t firmwareSize = OD_OTA_FIRMWARE_SIZE;

    if (firmwareSize == 0) {
        // Size 0 = abort/reset
        resetOtaState();
        ESP_LOGI(TAG, "OTA reset (size=0)");
        return ODR_OK;
    }

    // Validate size
    if (firmwareSize > 0x140000) {  // 1.25 MB max partition size
        setError(CANOPEN_OTA_ERR_SIZE_TOO_LARGE);
        return ODR_HW;
    }

    // Abort any previous OTA
    if (s_otaStarted) {
        resetOtaState();
    }

    // Find OTA partition
    s_otaPartition = esp_ota_get_next_update_partition(NULL);
    if (s_otaPartition == nullptr) {
        setError(CANOPEN_OTA_ERR_NO_PARTITION);
        return ODR_HW;
    }

    // Begin OTA
    esp_err_t err = esp_ota_begin(s_otaPartition, firmwareSize, &s_otaHandle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_begin failed: 0x%x", err);
        setError(CANOPEN_OTA_ERR_BEGIN_FAILED);
        return ODR_HW;
    }

    s_crc32Running = 0;
    s_bytesWritten = 0;
    s_otaStarted = true;
    OD_OTA_STATUS = CANOPEN_OTA_RECEIVING;
    OD_OTA_BYTES_RECEIVED = 0;
    OD_OTA_ERROR_CODE = CANOPEN_OTA_ERR_NONE;

    ESP_LOGI(TAG, "OTA started: size=%lu, partition=%s",
             (unsigned long)firmwareSize, s_otaPartition->label);
    return ODR_OK;
}

// ============================================================================
// OD write callback for 0x2F00 (firmware data domain) — streams to flash
// ============================================================================

static ODR_t onOtaWriteChunk(OD_stream_t* stream, const void* buf,
                             OD_size_t count, OD_size_t* countWritten) {
    if (!s_otaStarted) {
        ESP_LOGE(TAG, "OTA data received but OTA not started (write size first)");
        return ODR_DEV_INCOMPAT;
    }

    if (count == 0) {
        *countWritten = 0;
        return ODR_OK;
    }

    // Write chunk to flash
    esp_err_t err = esp_ota_write(s_otaHandle, buf, count);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "esp_ota_write failed at offset %lu: 0x%x",
                 (unsigned long)s_bytesWritten, err);
        setError(CANOPEN_OTA_ERR_WRITE_FAILED);
        return ODR_HW;
    }

    // Update CRC32 incrementally
    s_crc32Running = crc32_le(s_crc32Running, (const uint8_t*)buf, count);
    s_bytesWritten += count;
    *countWritten = count;

    // Update OD status for TPDO reporting
    OD_OTA_BYTES_RECEIVED = s_bytesWritten;

    // Log progress every 64 KB
    if ((s_bytesWritten % (64 * 1024)) < count) {
        ESP_LOGI(TAG, "OTA progress: %lu / %lu bytes",
                 (unsigned long)s_bytesWritten, (unsigned long)OD_OTA_FIRMWARE_SIZE);
    }

    // Check if this was the last segment
    if (stream->dataOffset + count >= stream->dataLength) {
        ESP_LOGI(TAG, "OTA transfer complete: %lu bytes", (unsigned long)s_bytesWritten);
        OD_OTA_STATUS = CANOPEN_OTA_VERIFYING;

        // Verify CRC32
        uint32_t expectedCrc = OD_OTA_FIRMWARE_CRC32;
        if (expectedCrc != 0 && s_crc32Running != expectedCrc) {
            ESP_LOGE(TAG, "CRC32 mismatch: expected=0x%08lx, got=0x%08lx",
                     (unsigned long)expectedCrc, (unsigned long)s_crc32Running);
            esp_ota_abort(s_otaHandle);
            s_otaHandle = 0;
            s_otaStarted = false;
            setError(CANOPEN_OTA_ERR_CRC_MISMATCH);
            return ODR_HW;
        }

        // Commit OTA
        err = esp_ota_end(s_otaHandle);
        s_otaHandle = 0;
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_end failed: 0x%x", err);
            s_otaStarted = false;
            setError(CANOPEN_OTA_ERR_COMMIT_FAILED);
            return ODR_HW;
        }

        err = esp_ota_set_boot_partition(s_otaPartition);
        if (err != ESP_OK) {
            ESP_LOGE(TAG, "esp_ota_set_boot_partition failed: 0x%x", err);
            s_otaStarted = false;
            setError(CANOPEN_OTA_ERR_COMMIT_FAILED);
            return ODR_HW;
        }

        OD_OTA_STATUS = CANOPEN_OTA_COMPLETE;
        s_otaStarted = false;
        ESP_LOGI(TAG, "OTA verified. CRC32=0x%08lx. Rebooting in 2s...",
                 (unsigned long)s_crc32Running);

        // Reboot after short delay (let SDO response go out)
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();
    }

    return ODR_OK;
}

// ============================================================================
// Public API
// ============================================================================

void CanOpenOTA::registerOdExtensions() {
    // Register write callback for 0x2F00 (firmware data domain)
    OD_entry_t* dataEntry = OD_find(OD, UC2_OD::OTA_FIRMWARE_DATA);
    if (dataEntry) {
        s_otaDataExt.object = nullptr;
        s_otaDataExt.read   = nullptr;  // write-only
        s_otaDataExt.write  = onOtaWriteChunk;
        OD_extension_init(dataEntry, &s_otaDataExt);
        ESP_LOGI(TAG, "OTA firmware data OD extension registered (0x2F00)");
    } else {
        ESP_LOGE(TAG, "OD entry 0x2F00 not found!");
    }

    // Register write callback for 0x2F01 (firmware size — triggers begin)
    OD_entry_t* sizeEntry = OD_find(OD, UC2_OD::OTA_FIRMWARE_SIZE);
    if (sizeEntry) {
        s_otaSizeExt.object = nullptr;
        s_otaSizeExt.read   = nullptr;
        s_otaSizeExt.write  = onOtaSizeWrite;
        OD_extension_init(sizeEntry, &s_otaSizeExt);
        ESP_LOGI(TAG, "OTA firmware size OD extension registered (0x2F01)");
    } else {
        ESP_LOGE(TAG, "OD entry 0x2F01 not found!");
    }

    // Initialize status
    OD_OTA_STATUS = CANOPEN_OTA_IDLE;
    OD_OTA_BYTES_RECEIVED = 0;
    OD_OTA_ERROR_CODE = CANOPEN_OTA_ERR_NONE;
}

#endif // CAN_CONTROLLER_CANOPEN
