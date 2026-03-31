#pragma once
#include <stdint.h>
#include "cJSON.h"
#include "SerialTransport.h"

// CommandTracker: two-phase ACK/DONE response protocol.
//
// Phase 1 (ACK): sent immediately when a command is parsed successfully.
//   {"qid": 42, "status": "ACK"}
//
// Phase 2 (DONE): sent when the action completes (motor finished, laser set, etc.)
//   {"qid": 42, "status": "DONE", ...result_data...}
//
// On error:
//   {"qid": 42, "status": "ERROR", "error": "description"}
//
// Fire-and-forget commands (e.g. /state_get) can use sendImmediate() which
// merges ACK+DONE into a single response.
//
// This works alongside the existing QidRegistry for motor-specific tracking.

namespace CommandTracker
{
    // Send an immediate ACK for a parsed command.
    // Call this right after successful JSON parse, before dispatching.
    void sendACK(int qid, TransportFormat format);

    // Send a DONE response with optional result data.
    // If resultData is not NULL, its fields are merged into the response.
    // resultData is NOT deleted by this function.
    void sendDONE(int qid, TransportFormat format, cJSON *resultData = nullptr);

    // Send an ERROR response.
    void sendERROR(int qid, TransportFormat format, const char *errorMsg);

    // Send a combined ACK+result for fire-and-forget commands.
    // This produces a single response with both status:"DONE" and the result.
    void sendImmediate(int qid, TransportFormat format, cJSON *resultData);
};
