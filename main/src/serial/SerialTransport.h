#pragma once
#include <stdint.h>
#include <stddef.h>
#include "cJSON.h"

// Header byte values for the COBS-framed serial protocol.
// Frame format: 0x00 | HEADER_BYTE | COBS(payload) | 0x00
#define SERIAL_HDR_JSON    0x01
#define SERIAL_HDR_MSGPACK 0x02

// Transport format detected from incoming data
enum class TransportFormat : uint8_t
{
    LEGACY_JSON = 0, // no COBS, newline-delimited JSON (v1 compat)
    COBS_JSON   = 1, // COBS-framed, JSON payload
    COBS_MSGPACK = 2 // COBS-framed, MsgPack payload (future)
};

// SerialTransport: replaces direct Serial reads in SerialProcess::loop().
// Accumulates bytes, detects framing (legacy vs COBS), decodes, and produces cJSON*.
//
// Usage:
//   SerialTransport::setup();
//   // in loop:
//   cJSON *cmd = SerialTransport::readNext();  // NULL if no complete frame yet
//   if (cmd) { ... process ... cJSON_Delete(cmd); }
//
//   // for sending responses:
//   SerialTransport::sendJSON(responseDoc, requestFormat);

namespace SerialTransport
{
    void setup();

    // Try to read and parse one complete command from Serial.
    // Returns a cJSON* if a full message was decoded, or NULL if still accumulating.
    // Caller owns the returned cJSON object and must cJSON_Delete it.
    // outFormat is set to the detected format of the incoming message.
    cJSON *readNext(TransportFormat &outFormat);

    // Send a cJSON response using the same framing as the request.
    // If format is LEGACY_JSON, wraps with ++/-- delimiters.
    // If format is COBS_JSON, COBS-encodes with header byte 0x01.
    void sendJSON(cJSON *doc, TransportFormat format);

    // Send a pre-serialized JSON string (with ++ / -- for legacy, COBS for new)
    void sendJSONString(const char *jsonStr, TransportFormat format);

    // Check if we're in binary modes (OTA etc) via can subsystem
    bool isInBinaryMode();
};
