/**
 * NMTManager.h
 *
 * UC2 CANopen-Lite NMT (Network Management) manager.
 *
 * Master side:
 *   - Sends NMT commands (START, STOP, PRE-OPERATIONAL, RESET) to slaves
 *   - Tracks per-node NMT state via heartbeat frames
 *   - Fires a callback when any node comes online, goes offline, or changes state
 *   - Auto-starts newly booted slaves (on NMT boot-up message)
 *
 * Slave side:
 *   - Processes incoming NMT commands and transitions own state machine
 *   - Exposes own NMT state for HeartbeatManager to broadcast
 *
 * Call NMTManager::update() frequently from the main loop (or a timer task)
 * to detect heartbeat timeouts.
 */
#pragma once
#include <stdint.h>
#include <stdbool.h>
#include "ObjectDictionary.h"

// Maximum number of tracked slave nodes
#define UC2_NMT_MAX_NODES 16

// Node NMT states (mirrors UC2_NMT_STATE_* values from ObjectDictionary.h)
enum class UC2_NMT_State : uint8_t {
    UNKNOWN         = 0xFFu,
    INITIALIZING    = 0x00u,
    PRE_OPERATIONAL = 0x7Fu,
    OPERATIONAL     = 0x05u,
    STOPPED         = 0x04u,
    OFFLINE         = 0xFEu,   // heartbeat timeout — not a real CANopen state
};

// Per-node routing information built from TPDO2 + NMT heartbeat
struct UC2_NodeEntry {
    uint8_t       nodeId;
    UC2_NMT_State state;
    uint8_t       capabilities;     // UC2_CAP_* bitmask (from TPDO2 or SDO)
    uint32_t      lastHeartbeatMs;  // millis() timestamp of last heartbeat
    uint16_t      heartbeatTimeoutMs;
    bool          online;
};

namespace NMTManager {

// Callback invoked when a node changes state (comes online, goes offline, etc.)
// Called from update() — do not call blocking functions from the callback.
typedef void (*NodeStatusCallback)(uint8_t nodeId, UC2_NMT_State newState, bool online);

// ============================================================================
// Initialization
// ============================================================================

void init();

// Register a node that should send heartbeats; timeoutMs = 0 uses default (3 s)
void registerExpectedNode(uint8_t nodeId, uint16_t timeoutMs = 3000u);

// Set callback for node state changes
void setNodeStatusCallback(NodeStatusCallback cb);

// ============================================================================
// Master-side API
// ============================================================================

// Periodic update — detect heartbeat timeouts, fire callbacks.
// Call from main loop or a 100 ms timer task.
void update();

// Send NMT command to a specific node (nodeId=0 addresses all nodes)
void sendStart(uint8_t nodeId = 0u);
void sendStop(uint8_t nodeId = 0u);
void sendPreOperational(uint8_t nodeId = 0u);
void sendReset(uint8_t nodeId = 0u);
void sendResetComm(uint8_t nodeId = 0u);

// Process incoming NMT boot-up frame (COB-ID 0x700+nodeId, data[0]==0x00).
// Automatically calls sendStart() to move slave to OPERATIONAL.
void onBootUp(uint8_t nodeId);

// Process incoming heartbeat frame (COB-ID 0x700+nodeId, data[0]==nmtState).
void onHeartbeat(uint8_t nodeId, uint8_t nmtStateByte);

// Update node capability info when TPDO2 is received
void onNodeStatusPDO(uint8_t nodeId, uint8_t caps);

// Query
UC2_NMT_State getNodeState(uint8_t nodeId);
bool          isOperational(uint8_t nodeId);
bool          isOnline(uint8_t nodeId);

// Pointer to the internal node table (count written to *count)
const UC2_NodeEntry* getNodes(int* count);

// ============================================================================
// Slave-side API
// ============================================================================

// Process an incoming NMT command frame (called from CAN RX handler)
// ownNodeId is this device's configured node ID.
void onNMTCommand(uint8_t cmdByte, uint8_t targetNodeId, uint8_t ownNodeId);

// Get / set own NMT state (slave uses this; master ignores it)
UC2_NMT_State getOwnState();
void          setOwnState(UC2_NMT_State state);

} // namespace NMTManager
