/**
 * @file OtaBinaryReceive.h
 * @brief Master-side serial => CAN OTA binary receive bridge.
 *
 * The Python host first sends a JSON preamble:
 *   {"task":"/ota_start","ota":{"nodeId":11,"size":N,"crc32":"0x..."}}
 *
 * On success this module flips SerialProcess into binary mode. It then
 * consumes raw firmware bytes straight from Serial into a PSRAM buffer,
 * emits progress ACKs, verifies CRC32, and finally calls
 * CanOpenOTAStreaming::flashSlave() with the assembled blob.
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

// Drain any pending Serial bytes into the receive buffer. When the full
// payload has arrived, verifies CRC32 and pushes it to the slave via
// CanOpenOTAStreaming::flashSlave(). Safe to call from SerialProcess::loop()
// once per loop iteration.
void processBytes();

}  // namespace OtaBinaryReceive

#endif  // CAN_CONTROLLER_CANOPEN
