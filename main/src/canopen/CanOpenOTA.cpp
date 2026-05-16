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
#include "PinConfig.h"

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
static uint32_t                s_nextProgressMark      = 64U * 1024U;
;

static OD_extension_t s_otaDataExt;
static OD_extension_t s_otaSizeExt;

// ============================================================================
// Internal helpers
// ============================================================================

static void resetOtaState() {
    const bool wasActive = s_otaStarted;
    const uint32_t lastBytes = s_bytesWritten;
    if (s_otaStarted && s_otaHandle != 0) {
        esp_ota_abort(s_otaHandle);
    }
    s_otaHandle = 0;
    s_otaPartition = nullptr;
    s_crc32Running = 0;
    s_bytesWritten = 0;
    s_otaStarted = false;
    s_nextProgressMark = 64U * 1024U;
    OD_OTA_STATUS = CANOPEN_OTA_IDLE;
    OD_OTA_BYTES_RECEIVED = 0;
    OD_OTA_ERROR_CODE = CANOPEN_OTA_ERR_NONE;
    if (wasActive) {
        log_i("OTA state reset (was active, %lu bytes written)",
              (unsigned long)lastBytes);
    }
}

static void setError(uint8_t errCode) {
    OD_OTA_STATUS = CANOPEN_OTA_ERROR;
    OD_OTA_ERROR_CODE = errCode;
    log_e("OTA error: %d", errCode);
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
    // THIS is on the slave side, so this is called when the master writes to 0x2F01 on the slave.
    // The master is expected to write the firmware size here first, which triggers the OTA process on the slave.
    log_i("onOtaSizeWrite: count=%u", (unsigned)count);
    ODR_t ret = OD_writeOriginal(stream, buf, count, countWritten);
    if (ret != ODR_OK) return ret;

    uint32_t firmwareSize = OD_OTA_FIRMWARE_SIZE;

    if (firmwareSize == 0) {
        // Size 0 = abort/reset
        resetOtaState();
        log_i("OTA reset (size=0)");
        return ODR_OK;
    }

    // Validate size
    if (firmwareSize > 0x140000) {  // 1.25 MB max partition size
        log_e("Invalid OTA size: %lu", (unsigned long)firmwareSize);
        setError(CANOPEN_OTA_ERR_SIZE_TOO_LARGE);
        return ODR_HW;
    }

    // Abort any previous OTA
    log_i("OTA is still active: %s, bytesWritten=%lu. Resetting state for new OTA.",
          s_otaStarted ? "yes" : "no", (unsigned long)s_bytesWritten);
    if (s_otaStarted) {
        log_i("OTA already in progress, aborting and starting new OTA");
        resetOtaState();
    }

    // Find OTA partition
    s_otaPartition = esp_ota_get_next_update_partition(NULL);
    if (s_otaPartition == nullptr) {
        log_e("No OTA partition found");
        setError(CANOPEN_OTA_ERR_NO_PARTITION);
        return ODR_HW;
    }

    // esp_ota_begin erases the target partition. This blocks the calling
    // thread for 200-500ms. During the erase, flash SPI operations may
    // momentarily starve the CAN peripheral of bus cycles (especially on
    // single-core ESP32-S3 variants sharing the SPI bus). Yield briefly
    // before AND after to let the CAN ctrl task drain its queues.
    vTaskDelay(pdMS_TO_TICKS(10));
    esp_err_t err = esp_ota_begin(s_otaPartition, firmwareSize, &s_otaHandle);
    if (err != ESP_OK) {
        log_e("esp_ota_begin failed: 0x%x", err);
        setError(CANOPEN_OTA_ERR_BEGIN_FAILED);
        return ODR_HW;
    }
    // Let the CAN stack recover from any frames missed during flash erase
    vTaskDelay(pdMS_TO_TICKS(50));

    s_crc32Running = 0;
    s_bytesWritten = 0;
    s_otaStarted = true;
    OD_OTA_STATUS = CANOPEN_OTA_RECEIVING;
    OD_OTA_BYTES_RECEIVED = 0;
    OD_OTA_ERROR_CODE = CANOPEN_OTA_ERR_NONE;

    log_i("OTA started: size=%lu, partition=%s",
          (unsigned long)firmwareSize, s_otaPartition->label);
    return ODR_OK;
}

// ============================================================================
// OD write callback for 0x2F00 (firmware data domain) — streams to flash
// ============================================================================

static ODR_t onOtaWriteChunk(OD_stream_t* stream, const void* buf,
                             OD_size_t count, OD_size_t* countWritten) {
    if (!s_otaStarted) {
        log_e("OTA data received but OTA not started (write 0x2F01 size first) count=%u",
              (unsigned)count);
        return ODR_DEV_INCOMPAT;
    }

    if (count == 0) {
        // Stack calls us with count=0 to query state; not an error.
        *countWritten = 0;
        return ODR_OK;
    }

    // Write chunk to flash. esp_ota_write does not block long for small
    // counts (<= 256 B) but we still avoid logging here on every call —
    // the SDO segment rate is several kHz and per-segment logs over
    // 921600 baud UART would starve the CANopen task and force bus-off.
    esp_err_t err = esp_ota_write(s_otaHandle, buf, count);
    if (err != ESP_OK) {
        log_e("esp_ota_write failed at offset %lu count=%u err=0x%x",
              (unsigned long)s_bytesWritten, (unsigned)count, err);
        setError(CANOPEN_OTA_ERR_WRITE_FAILED);
        return ODR_HW;
    }

    // Update CRC32 incrementally
    s_crc32Running = crc32_le(s_crc32Running, (const uint8_t*)buf, count);
    s_bytesWritten += count;
    *countWritten = count;

    // Update OD status for TPDO reporting
    OD_OTA_BYTES_RECEIVED = s_bytesWritten;

    // Throttled progress log: every 64 KB to avoid UART blocking the SDO task.
    if (1){ //s_bytesWritten >= s_nextProgressMark) {
        log_i("OTA progress: %lu / %lu bytes",
              (unsigned long)s_bytesWritten,
              (unsigned long)OD_OTA_FIRMWARE_SIZE);
        s_nextProgressMark += 64U * 1024U;
    }

    // Detect end-of-transfer.
    //
    // For a domain entry (0x2F00 has dataLength=0 in the OD), the SDO
    // server only fills in stream->dataLength on the FINAL segment via
    // validateAndWriteToOD() — on intermediate buffer flushes (every
    // ~CO_CONFIG_SDO_SRV_BUFFER_SIZE bytes) it stays 0. Comparing against
    // dataLength alone would therefore terminate the transfer on the very
    // first partial flush (~28 bytes for the default 32-byte server FIFO).
    //
    // The master writes OD_OTA_FIRMWARE_SIZE (0x2F01) BEFORE streaming any
    // data, so we use that as the authoritative total and only consult
    // stream->dataLength as a tie-breaker when it is non-zero (which only
    // happens on the very last segment).
    const uint32_t expectedSize  = OD_OTA_FIRMWARE_SIZE;
    const bool     sizeReached   = (expectedSize > 0)
                                   && (s_bytesWritten >= expectedSize);
    const bool     streamFinished = (stream->dataLength > 0)
                                    && ((stream->dataOffset + count)
                                        >= stream->dataLength);
    if (sizeReached || streamFinished) {
        log_i("OTA transfer complete: %lu bytes", (unsigned long)s_bytesWritten);
        OD_OTA_STATUS = CANOPEN_OTA_VERIFYING;

        // Verify CRC32
        uint32_t expectedCrc = OD_OTA_FIRMWARE_CRC32;
        if (expectedCrc != 0 && s_crc32Running != expectedCrc) {
            log_e("CRC32 mismatch: expected=0x%08lx, got=0x%08lx",
                  (unsigned long)expectedCrc, (unsigned long)s_crc32Running);
            esp_ota_abort(s_otaHandle);
            s_otaHandle = 0;
            s_otaStarted = false;
            setError(CANOPEN_OTA_ERR_CRC_MISMATCH);
            return ODR_HW;
        }
        else{
            log_i("CRC32 verified: 0x%08lx", (unsigned long)s_crc32Running);
        }

        // Commit OTA
        err = esp_ota_end(s_otaHandle);
        s_otaHandle = 0;
        if (err != ESP_OK) {
            log_e("esp_ota_end failed: 0x%x", err);
            s_otaStarted = false;
            setError(CANOPEN_OTA_ERR_COMMIT_FAILED);
            return ODR_HW;
        }

        err = esp_ota_set_boot_partition(s_otaPartition);
        if (err != ESP_OK) {
            log_e("esp_ota_set_boot_partition failed: 0x%x", err);
            s_otaStarted = false;
            setError(CANOPEN_OTA_ERR_COMMIT_FAILED);
            return ODR_HW;
        }

        OD_OTA_STATUS = CANOPEN_OTA_COMPLETE;
        s_otaStarted = false;
        log_i("OTA complete and verified. Boot partition set to %s",
              s_otaPartition->label);

        // Reboot after short delay (let SDO response go out)
        vTaskDelay(pdMS_TO_TICKS(2000));
        esp_restart();

        // Final segment: tell the SDO server the OD variable is complete.
        return ODR_OK;
    }

    // IMPORTANT: return ODR_PARTIAL while more data is expected. Returning
    // ODR_OK before all expectedSize bytes have arrived makes CANopenNode's
    // validateAndWriteToOD() interpret the OD variable as "fully written"
    // and abort the next incoming segment with CO_SDO_AB_DATA_LONG
    // (0x06070012, "Length of service parameter too high").
    // See CO_SDOserver.c lines 575-579 in lib/ESP32_CanOpenNode.
    return ODR_PARTIAL;
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
        log_i("OTA firmware data OD extension registered (0x2F00)");
    } else {
        log_e("OD entry 0x2F00 not found!");
    }

    // Register write callback for 0x2F01 (firmware size — triggers begin)
    OD_entry_t* sizeEntry = OD_find(OD, UC2_OD::OTA_FIRMWARE_SIZE);
    if (sizeEntry) {
        s_otaSizeExt.object = nullptr;
        s_otaSizeExt.read   = nullptr;
        s_otaSizeExt.write  = onOtaSizeWrite;
        OD_extension_init(sizeEntry, &s_otaSizeExt);
        log_i("OTA firmware size OD extension registered (0x2F01)");
    } else {
        log_e("OD entry 0x2F01 not found!");
    }

    // Initialize status
    OD_OTA_STATUS = CANOPEN_OTA_IDLE;
    OD_OTA_BYTES_RECEIVED = 0;
    OD_OTA_ERROR_CODE = CANOPEN_OTA_ERR_NONE;
}

#endif // CAN_CONTROLLER_CANOPEN
