/**
 * @file CanOpenOTAStreaming.h
 * @brief Master-side OTA streaming to a slave via CANopen SDO domain transfer
 *
 * The master reads firmware binary (from serial JSON or internal buffer) and
 * writes it to the slave's OD 0x2F00 via canopen_sdo_write_domain().
 * CANopenNode handles segmented/block transfer automatically.
 *
 * T4.4 4d: C-ified (was namespace CanOpenOTAStreaming; C API canopen_ota_*
 * defined in canopen_ota.c, section 3).
 */

#pragma once

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>
#include "cJSON.h"

#ifdef CAN_CONTROLLER_CANOPEN

#ifdef __cplusplus
extern "C" {
#endif

/**
 * @brief Flash a slave node's firmware via SDO domain transfer.
 *
 * @deprecated For serial-fed OTA on memory-constrained masters use
 * ota_begin()/ota_processBytes() (streams 4 KB chunks via
 * canopen_sdo_download_*).
 * canopen_ota_flash_slave() requires the full firmware image in RAM and is
 * kept only
 * for tests where the host owns the buffer (e.g. flashing from SD).
 *
 * Steps:
 *   1. Write firmware size to slave 0x2F01
 *   2. Write CRC32 to slave 0x2F02
 *   3. Write firmware data to slave 0x2F00 (domain SDO)
 *   4. Read back 0x2F03 status to confirm success
 *
 * @param nodeId   Target slave node ID
 * @param data     Firmware binary data
 * @param dataSize Firmware binary size
 * @param crc32    Pre-computed CRC32 of the firmware
 * @return true on success
 */
bool canopen_ota_flash_slave(uint8_t nodeId, const uint8_t* data, size_t dataSize, uint32_t crc32);

/**
 * @brief Handle JSON command for OTA streaming.
 *
 * JSON format:
 *   {"task": "/ota_start", "ota": {"nodeId": 11, "size": 1048576, "crc32": "..."}}
 *
 * After receiving the start command, enters binary receive mode to accept
 * firmware data over serial, then streams it to the slave via SDO.
 */
int canopen_ota_act_from_json(cJSON* doc);

/**
 * @brief Read OTA status from a slave node.
 * @param nodeId Target slave node ID
 * @return OTA status byte from 0x2F03, or 0xFF on error
 */
uint8_t canopen_ota_read_slave_status(uint8_t nodeId);

#ifdef __cplusplus
}  // extern "C"
#endif

#endif // CAN_CONTROLLER_CANOPEN
