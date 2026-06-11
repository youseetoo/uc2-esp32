/**
 * @file CanOpenOTA.h
 * @brief CANopen OTA firmware update via SDO domain transfer (slave side)
 *
 * Registers an OD_extension_init() callback on UC2_OD::OTA_FIRMWARE_DATA (0x2F00).
 * Incoming SDO segmented/block writes stream directly into esp_ota_write()
 * without buffering the full binary in RAM.
 *
 * Usage:
 *   1. Master writes firmware size to 0x2F01 and CRC32 to 0x2F02
 *   2. Master writes firmware data as SDO domain transfer to 0x2F00
 *   3. Slave streams each segment to esp_ota_write()
 *   4. After transfer completes, slave verifies CRC32
 *   5. On success, sets boot partition and reboots
 */

#pragma once

#include <stdint.h>

// OTA status values (written to OD 0x2F03)
enum CanOpenOtaStatus : uint8_t {
    CANOPEN_OTA_IDLE        = 0x00,
    CANOPEN_OTA_RECEIVING   = 0x01,
    CANOPEN_OTA_VERIFYING   = 0x02,
    CANOPEN_OTA_COMPLETE    = 0x03,
    CANOPEN_OTA_ERROR       = 0xFF,
};

// OTA error codes (written to OD 0x2F05)
enum CanOpenOtaError : uint8_t {
    CANOPEN_OTA_ERR_NONE            = 0x00,
    CANOPEN_OTA_ERR_NO_PARTITION    = 0x01,
    CANOPEN_OTA_ERR_BEGIN_FAILED    = 0x02,
    CANOPEN_OTA_ERR_WRITE_FAILED    = 0x03,
    CANOPEN_OTA_ERR_CRC_MISMATCH   = 0x04,
    CANOPEN_OTA_ERR_COMMIT_FAILED   = 0x05,
    CANOPEN_OTA_ERR_SIZE_ZERO       = 0x06,
    CANOPEN_OTA_ERR_SIZE_TOO_LARGE  = 0x07,
};

namespace CanOpenOTA {

/**
 * @brief Register OD extension callbacks for OTA entries.
 * Call once after CO_CANopenInit() completes.
 */
void registerOdExtensions();

}  // namespace CanOpenOTA
