/**
 * NMTManager.cpp
 *
 * UC2 CANopen-Lite NMT manager implementation.
 * Sends NMT commands via raw TWAI; tracks slave states via heartbeat frames.
 */
#include "NMTManager.h"
#include <string.h>
#include <Arduino.h>
#include "driver/twai.h"
#include "esp_log.h"

#define TAG_NMT "UC2_NMT"

namespace NMTManager {

static UC2_NodeEntry    s_nodes[UC2_NMT_MAX_NODES];
static int              s_count = 0;
static NodeStatusCallback s_cb  = nullptr;
static UC2_NMT_State    s_ownState = UC2_NMT_State::INITIALIZING;

// ---------------------------------------------------------------------------
// Internal helpers
// ---------------------------------------------------------------------------

static UC2_NodeEntry* findNode(uint8_t nodeId)
{
    for (int i = 0; i < s_count; i++) {
        if (s_nodes[i].nodeId == nodeId) return &s_nodes[i];
    }
    return nullptr;
}

static UC2_NodeEntry* findOrAlloc(uint8_t nodeId)
{
    UC2_NodeEntry* n = findNode(nodeId);
    if (n) return n;
    if (s_count >= UC2_NMT_MAX_NODES) {
        ESP_LOGE(TAG_NMT, "Node table full — cannot track node %d", nodeId);
        return nullptr;
    }
    n = &s_nodes[s_count++];
    n->nodeId             = nodeId;
    n->state              = UC2_NMT_State::UNKNOWN;
    n->capabilities       = 0;
    n->lastHeartbeatMs    = (uint32_t)millis();
    n->heartbeatTimeoutMs = 3000u;
    n->online             = false;
    return n;
}

static void sendNMTFrame(uint8_t cmd, uint8_t target)
{
    twai_message_t f = {};
    f.identifier       = UC2_COBID_NMT;
    f.extd             = 0;
    f.rtr              = 0;
    f.data_length_code = 2;
    f.data[0]          = cmd;
    f.data[1]          = target;
    twai_transmit(&f, pdMS_TO_TICKS(10));
}

// ---------------------------------------------------------------------------
// Initialization
// ---------------------------------------------------------------------------

void init()
{
    s_count = 0;
    memset(s_nodes, 0, sizeof(s_nodes));
    for (int i = 0; i < UC2_NMT_MAX_NODES; i++) {
        s_nodes[i].state              = UC2_NMT_State::UNKNOWN;
        s_nodes[i].heartbeatTimeoutMs = 3000u;
    }
    s_ownState = UC2_NMT_State::INITIALIZING;
}

void registerExpectedNode(uint8_t nodeId, uint16_t timeoutMs)
{
    UC2_NodeEntry* n = findOrAlloc(nodeId);
    if (n) n->heartbeatTimeoutMs = timeoutMs;
}

void setNodeStatusCallback(NodeStatusCallback cb) { s_cb = cb; }

// ---------------------------------------------------------------------------
// Master-side API
// ---------------------------------------------------------------------------

void update()
{
    uint32_t now = (uint32_t)millis();
    for (int i = 0; i < s_count; i++) {
        if (!s_nodes[i].online) continue;
        uint32_t elapsed = now - s_nodes[i].lastHeartbeatMs;
        if (elapsed > s_nodes[i].heartbeatTimeoutMs) {
            s_nodes[i].online = false;
            s_nodes[i].state  = UC2_NMT_State::OFFLINE;
            ESP_LOGW(TAG_NMT, "Node %d offline (HB timeout %lu ms)",
                     s_nodes[i].nodeId, (unsigned long)elapsed);
            if (s_cb) s_cb(s_nodes[i].nodeId, UC2_NMT_State::OFFLINE, false);
        }
    }
}

void sendStart(uint8_t nodeId)          { sendNMTFrame(UC2_NMT_CMD_START,           nodeId); }
void sendStop(uint8_t nodeId)           { sendNMTFrame(UC2_NMT_CMD_STOP,            nodeId); }
void sendPreOperational(uint8_t nodeId) { sendNMTFrame(UC2_NMT_CMD_PRE_OPERATIONAL, nodeId); }
void sendReset(uint8_t nodeId)          { sendNMTFrame(UC2_NMT_CMD_RESET_NODE,      nodeId); }
void sendResetComm(uint8_t nodeId)      { sendNMTFrame(UC2_NMT_CMD_RESET_COMM,      nodeId); }

void onBootUp(uint8_t nodeId)
{
    ESP_LOGI(TAG_NMT, "Node %d boot-up", nodeId);
    UC2_NodeEntry* n = findOrAlloc(nodeId);
    if (!n) return;
    bool wasOnline       = n->online;
    n->state             = UC2_NMT_State::PRE_OPERATIONAL;
    n->online            = true;
    n->lastHeartbeatMs   = (uint32_t)millis();
    if (!wasOnline && s_cb) {
        s_cb(nodeId, UC2_NMT_State::PRE_OPERATIONAL, true);
    }
    // Auto-transition slave to OPERATIONAL
    sendStart(nodeId);
}

void onHeartbeat(uint8_t nodeId, uint8_t nmtStateByte)
{
    UC2_NodeEntry*  n = findOrAlloc(nodeId);
    if (!n) return;
    UC2_NMT_State prev = n->state;
    bool wasOnline     = n->online;

    switch (nmtStateByte) {
    case UC2_NMT_STATE_PRE_OPERATIONAL: n->state = UC2_NMT_State::PRE_OPERATIONAL; break;
    case UC2_NMT_STATE_OPERATIONAL:     n->state = UC2_NMT_State::OPERATIONAL;     break;
    case UC2_NMT_STATE_STOPPED:         n->state = UC2_NMT_State::STOPPED;         break;
    default:                            n->state = UC2_NMT_State::UNKNOWN;          break;
    }
    n->online          = true;
    n->lastHeartbeatMs = (uint32_t)millis();

    if ((!wasOnline || prev != n->state) && s_cb) {
        s_cb(nodeId, n->state, true);
    }
}

void onNodeStatusPDO(uint8_t nodeId, uint8_t caps)
{
    UC2_NodeEntry* n = findOrAlloc(nodeId);
    if (n) n->capabilities = caps;
}

UC2_NMT_State getNodeState(uint8_t nodeId)
{
    const UC2_NodeEntry* n = findNode(nodeId);
    return n ? n->state : UC2_NMT_State::UNKNOWN;
}

bool isOperational(uint8_t nodeId)
{
    return getNodeState(nodeId) == UC2_NMT_State::OPERATIONAL;
}

bool isOnline(uint8_t nodeId)
{
    const UC2_NodeEntry* n = findNode(nodeId);
    return n && n->online;
}

const UC2_NodeEntry* getNodes(int* count)
{
    *count = s_count;
    return s_nodes;
}

// ---------------------------------------------------------------------------
// Slave-side API
// ---------------------------------------------------------------------------

void onNMTCommand(uint8_t cmdByte, uint8_t targetNodeId, uint8_t ownNodeId)
{
    if (targetNodeId != 0u && targetNodeId != ownNodeId) return;  // not for us

    switch (cmdByte) {
    case UC2_NMT_CMD_START:
        s_ownState = UC2_NMT_State::OPERATIONAL;
        ESP_LOGI(TAG_NMT, "NMT → OPERATIONAL");
        break;
    case UC2_NMT_CMD_STOP:
        s_ownState = UC2_NMT_State::STOPPED;
        ESP_LOGI(TAG_NMT, "NMT → STOPPED");
        break;
    case UC2_NMT_CMD_PRE_OPERATIONAL:
        s_ownState = UC2_NMT_State::PRE_OPERATIONAL;
        ESP_LOGI(TAG_NMT, "NMT → PRE-OPERATIONAL");
        break;
    case UC2_NMT_CMD_RESET_NODE:
    case UC2_NMT_CMD_RESET_COMM:
        ESP_LOGI(TAG_NMT, "NMT → RESET (restarting)");
        esp_restart();
        break;
    default:
        ESP_LOGW(TAG_NMT, "Unknown NMT cmd 0x%02X", cmdByte);
        break;
    }
}

UC2_NMT_State getOwnState()                  { return s_ownState; }
void          setOwnState(UC2_NMT_State st)  { s_ownState = st; }

} // namespace NMTManager
