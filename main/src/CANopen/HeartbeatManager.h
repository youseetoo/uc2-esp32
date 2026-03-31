/**
 * HeartbeatManager.h
 *
 * UC2 CANopen-Lite heartbeat producer (slave) and consumer (master).
 *
 * Producer (slave):
 *   - Sends heartbeat frames at configurable interval
 *   - Frame: COB-ID = 0x700 + ownNodeId, data[0] = own NMT state byte
 *
 * Consumer (master):
 *   - Receives heartbeat frames and dispatches to NMTManager
 *   - Distinguishes boot-up messages (state byte = 0x00) from regular HB
 *
 * This is independent of CANopenNode — uses raw TWAI.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "driver/twai.h"

namespace HeartbeatManager {

// Initialize the heartbeat manager.
// ownNodeId: this device's CANopen node-ID (used for producer COB-ID).
// periodMs:  heartbeat producer period. 0 disables the producer.
void init(uint8_t ownNodeId, uint16_t periodMs = 1000u);

// Update the heartbeat producer period (can be changed at runtime, e.g. via SDO)
void setProducerPeriod(uint16_t periodMs);

// Send one heartbeat frame with the current NMT state.
// Normally called by tick(), but can be called directly (e.g. on state change).
void sendHeartbeat();

// Tick the heartbeat producer — call from main loop or a 10 ms timer task.
// Sends a heartbeat frame if the configured period has elapsed.
void tick();

// Process one incoming CAN frame.
// Returns true if the frame was a heartbeat (boot-up or regular) and was
// dispatched to NMTManager::onBootUp() / NMTManager::onHeartbeat().
bool processFrame(const twai_message_t& frame);

// Decode a heartbeat frame without dispatching it.
// Returns true if frame is a heartbeat; fills nodeId and nmtState.
bool decode(const twai_message_t& frame, uint8_t* nodeId, uint8_t* nmtState);

} // namespace HeartbeatManager
