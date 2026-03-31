/**
 * SDOHandler.h
 *
 * UC2 CANopen-Lite SDO client/server — lightweight, no external library.
 *
 * Implements CANopen expedited and segmented SDO transfers using raw TWAI.
 * The master uses the SDO client (write/read to slave OD entries).
 * The slave uses the SDO server (responds to master requests).
 *
 * CANopen SDO expedited transfer protocol (CiA 301):
 *
 *   Write (download) — master → slave:
 *     Request  (COB-ID 0x600+n): [cs][idx_lo][idx_hi][sub][d0][d1][d2][d3]
 *       cs = 0x23 (4B), 0x27 (3B), 0x2B (2B), 0x2F (1B)
 *     Response (COB-ID 0x580+n): [60][idx_lo][idx_hi][sub][0][0][0][0]
 *     Abort    (COB-ID 0x580+n): [80][idx_lo][idx_hi][sub][ac0][ac1][ac2][ac3]
 *
 *   Read (upload) — master → slave:
 *     Request  (COB-ID 0x600+n): [40][idx_lo][idx_hi][sub][0][0][0][0]
 *     Response (COB-ID 0x580+n): [4x][idx_lo][idx_hi][sub][d0][d1][d2][d3]
 *       x = 3 (1B), 2 (2B), 1 (3B), 0 (4B)
 *
 * Timeout: SDO_TIMEOUT_MS per transfer; retry up to SDO_MAX_RETRIES times.
 */
#pragma once
#include <stdint.h>
#include <stddef.h>
#include <stdbool.h>
#include "ObjectDictionary.h"
#include "driver/twai.h"

// SDO transfer configuration
#define SDO_TIMEOUT_MS   500u   // per attempt
#define SDO_MAX_RETRIES  3u     // attempts before giving up

namespace SDOHandler {

// ---------------------------------------------------------------------------
// Core SDO client (master → slave)
// ---------------------------------------------------------------------------

// Write 1–4 bytes to a slave OD entry (expedited only for now)
// Blocks until ack received or timeout.  Returns true on success.
bool writeExpedited(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                    const void* data, uint8_t dataLen);

// Read 1–4 bytes from a slave OD entry (expedited).
// On success, writes up to bufLen bytes into buf and sets *bytesRead.
bool readExpedited(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                   void* buf, uint8_t bufLen, uint8_t* bytesRead);

// Convenience typed wrappers
bool writeU8 (uint8_t nodeId, uint16_t idx, uint8_t sub, uint8_t  v);
bool writeU16(uint8_t nodeId, uint16_t idx, uint8_t sub, uint16_t v);
bool writeU32(uint8_t nodeId, uint16_t idx, uint8_t sub, uint32_t v);
bool writeI32(uint8_t nodeId, uint16_t idx, uint8_t sub, int32_t  v);
bool readU8  (uint8_t nodeId, uint16_t idx, uint8_t sub, uint8_t*  out);
bool readU16 (uint8_t nodeId, uint16_t idx, uint8_t sub, uint16_t* out);
bool readI32 (uint8_t nodeId, uint16_t idx, uint8_t sub, int32_t*  out);

// ---------------------------------------------------------------------------
// UC2 convenience SDO commands (master writes to slave UC2 OD entries)
// ---------------------------------------------------------------------------

// Set motor target position, speed, and command on slave
bool motorCommand(uint8_t nodeId, uint8_t axis,
                  int32_t targetPos, int32_t speed, uint8_t cmd);

// Set laser PWM on slave
bool laserSet(uint8_t nodeId, uint8_t channel, uint16_t pwm);

// Set LED on slave
bool ledSet(uint8_t nodeId, uint8_t mode,
            uint8_t r, uint8_t g, uint8_t b, uint16_t ledIndex);

// Read motor actual position from slave
bool motorGetPos(uint8_t nodeId, uint8_t axis, int32_t* posOut);

// Read motor is_running from slave
bool motorIsRunning(uint8_t nodeId, uint8_t axis, bool* runningOut);

// Read node capabilities from slave
bool nodeGetCaps(uint8_t nodeId, uint8_t* capsOut);

// Configure slave heartbeat period (ms)
bool configureHeartbeat(uint8_t nodeId, uint16_t periodMs);

// ---------------------------------------------------------------------------
// SDO server (slave side — process incoming SDO requests from master)
// ---------------------------------------------------------------------------
// Register a callback table of OD entries the slave exposes.
// The slave calls sdoServerInit() once, then sdoServerProcessFrame() from
// the CAN RX handler for every incoming SDO request frame.

// OD entry access mode
enum class OD_Access : uint8_t { RO = 1, WO = 2, RW = 3 };

// Callback invoked when master reads a value from the slave OD.
// Fill buf[0..3] and set *lenOut (1–4). Return true on success.
typedef bool (*OD_ReadCb)(uint16_t index, uint8_t subIndex,
                          uint8_t* buf, uint8_t* lenOut, void* ctx);

// Callback invoked when master writes a value to the slave OD.
// data points to 1–4 bytes, dataLen is the number of valid bytes.
typedef bool (*OD_WriteCb)(uint16_t index, uint8_t subIndex,
                           const uint8_t* data, uint8_t dataLen, void* ctx);

// Initialize the SDO server with application-provided callbacks and node-ID
void sdoServerInit(uint8_t ownNodeId, OD_ReadCb readCb, OD_WriteCb writeCb, void* ctx);

// Process one incoming CAN frame on the slave.
// Returns true if the frame was an SDO request and was handled (response sent).
bool sdoServerProcessFrame(const twai_message_t& frame);

// ---------------------------------------------------------------------------
// Internal helpers used by CanOpenStack — not normally called by app code
// ---------------------------------------------------------------------------

// Send a raw SDO abort frame (abort code per CiA 301 Table 20)
void sendAbort(uint8_t nodeId, uint16_t index, uint8_t subIndex, uint32_t abortCode);

// Wait for one SDO response frame addressed to own node.
// Returns true if received within timeoutMs; frame written to *out.
bool waitSDOResponse(uint8_t nodeId, twai_message_t* out, uint32_t timeoutMs);

} // namespace SDOHandler
