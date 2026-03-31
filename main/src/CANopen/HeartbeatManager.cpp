/**
 * HeartbeatManager.cpp
 *
 * UC2 CANopen-Lite heartbeat producer/consumer implementation.
 */
#include "HeartbeatManager.h"
#include "ObjectDictionary.h"
#include "NMTManager.h"
#include <Arduino.h>
#include "driver/twai.h"
#include "esp_log.h"

#define TAG_HB "UC2_HB"

namespace HeartbeatManager {

static uint8_t  s_ownNodeId  = 1u;
static uint16_t s_periodMs   = 1000u;
static uint32_t s_lastSentMs = 0u;

void init(uint8_t ownNodeId, uint16_t periodMs)
{
    s_ownNodeId  = ownNodeId;
    s_periodMs   = periodMs;
    s_lastSentMs = 0u;
}

void setProducerPeriod(uint16_t periodMs)
{
    s_periodMs = periodMs;
}

void sendHeartbeat()
{
    uint8_t nmtState = (uint8_t)NMTManager::getOwnState();
    twai_message_t f = {};
    f.identifier       = UC2_COBID_HEARTBEAT_BASE + s_ownNodeId;
    f.extd             = 0;
    f.rtr              = 0;
    f.data_length_code = 1;
    f.data[0]          = nmtState;
    // Non-blocking transmit (best-effort — dropped if bus is busy)
    twai_transmit(&f, pdMS_TO_TICKS(5));
}

void tick()
{
    if (s_periodMs == 0u) return;
    uint32_t now = (uint32_t)millis();
    if (now - s_lastSentMs >= s_periodMs) {
        s_lastSentMs = now;
        sendHeartbeat();
    }
}

bool decode(const twai_message_t& frame, uint8_t* nodeId, uint8_t* nmtState)
{
    uint32_t id = frame.identifier;
    if (id < UC2_COBID_HEARTBEAT_BASE + 1u) return false;
    if (id > UC2_COBID_HEARTBEAT_BASE + 127u) return false;
    if (frame.data_length_code < 1) return false;
    *nodeId   = (uint8_t)(id - UC2_COBID_HEARTBEAT_BASE);
    *nmtState = frame.data[0];
    return true;
}

bool processFrame(const twai_message_t& frame)
{
    uint8_t nodeId   = 0;
    uint8_t nmtState = 0;
    if (!decode(frame, &nodeId, &nmtState)) return false;

    if (nmtState == UC2_NMT_STATE_INITIALIZING) {
        // Boot-up message
        ESP_LOGI(TAG_HB, "Boot-up from node %d", nodeId);
        NMTManager::onBootUp(nodeId);
    } else {
        NMTManager::onHeartbeat(nodeId, nmtState);
    }
    return true;
}

} // namespace HeartbeatManager
