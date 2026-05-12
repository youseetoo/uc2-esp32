/**
 * @file OtaBinaryReceive.h
 * @brief Master-side serial => CAN OTA binary receive bridge (streaming).
 *
 * The Python host first sends a JSON preamble:
 *   {"task":"/ota_start","ota":{"nodeId":11,"size":N,"crc32":"0x..."}}
 *
 * On success this module flips SerialProcess into binary mode. It then
 * consumes raw firmware bytes from Serial in 4 KB chunks and forwards
 * each chunk to the slave via CANopenModule::sdoDownloadChunk(). The full
 * image is never buffered — memory cost is constant (~4 KB) regardless
 * of firmware size, so OTA works on ESP32 boards without PSRAM.
 */
#pragma once

#ifdef CAN_CONTROLLER_CANOPEN

#include <cJSON.h>
#include <stdint.h>
#include <stddef.h>

namespace OtaBinaryReceive {

// Begin a binary OTA receive session. Returns a JSON response document
// that must be serialised and freed by the caller. On success the master
// is in binary mode until receive completes or aborts.
cJSON* begin(uint8_t nodeId, uint32_t size, uint32_t crc32);

// True while the master is consuming raw firmware bytes from Serial.
bool isActive();

// Drain any pending Serial bytes into the chunk buffer. As each chunk
// fills it is pushed to the slave via the streaming SDO API. When the
// full payload has arrived, verifies CRC32 and finalises the SDO
// session. Safe to call from SerialProcess::loop() once per loop
// iteration.
void processBytes();

}  // namespace OtaBinaryReceive

#endif  // CAN_CONTROLLER_CANOPEN
