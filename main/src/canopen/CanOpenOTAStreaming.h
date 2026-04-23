/**
 * @file CanOpenOTAStreaming.h
 * @brief Master-side OTA streaming to a slave via CANopen SDO domain transfer
 *
 * The master reads firmware binary (from serial JSON or internal buffer) and
 * writes it to the slave's OD 0x2F00 via CANopenModule::writeSDODomain().
 * CANopenNode handles segmented/block transfer automatically.
 */

#pragma once

#include <stdint.h>
#include "cJSON.h"

#ifdef CAN_CONTROLLER_CANOPEN

namespace CanOpenOTAStreaming {

/**
 * @brief Flash a slave node's firmware via SDO domain transfer.
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
bool flashSlave(uint8_t nodeId, const uint8_t* data, size_t dataSize, uint32_t crc32);

/**
 * @brief Handle JSON command for OTA streaming.
 *
 * JSON format:
 *   {"task": "/ota_start", "ota": {"nodeId": 11, "size": 1048576, "crc32": "..."}}
 *
 * After receiving the start command, enters binary receive mode to accept
 * firmware data over serial, then streams it to the slave via SDO.
 */
int actFromJson(cJSON* doc);

/**
 * @brief Read OTA status from a slave node.
 * @param nodeId Target slave node ID
 * @return OTA status byte from 0x2F03, or 0xFF on error
 */
uint8_t readSlaveStatus(uint8_t nodeId);

}  // namespace CanOpenOTAStreaming

#endif // CAN_CONTROLLER_CANOPEN
