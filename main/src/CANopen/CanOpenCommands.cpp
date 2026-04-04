/**
 * CanOpenCommands.cpp
 *
 * CANopen replacement for can_controller act/get JSON commands.
 * Uses CanOpenStack for bus diagnostics and NMTManager for device control.
 */
#include "CanOpenCommands.h"
#include "CanOpenStack.h"
#include "NMTManager.h"
#include "cJSON.h"
#include <Preferences.h>
#include "esp_log.h"
#include "driver/twai.h"
#include "PinConfig.h"

#define TAG_COC "UC2_CO_CMD"

namespace CanOpenCommands {

// Persistent node ID — initialised from preferences on first access
static uint8_t s_nodeId = 0;
static bool    s_idLoaded = false;

static uint8_t loadNodeIdFromPrefs()
{
    Preferences prefs;
    prefs.begin("CAN", true);
    uint8_t id = prefs.getUInt("address", pinConfig.CAN_ID_CURRENT);
    prefs.end();
    return id;
}

uint8_t getNodeId()
{
    if (!s_idLoaded) {
        s_nodeId  = loadNodeIdFromPrefs();
        s_idLoaded = true;
    }
    return s_nodeId;
}

void setNodeId(uint8_t id)
{
    s_nodeId   = id;
    s_idLoaded = true;
    Preferences prefs;
    prefs.begin("CAN", false);
    prefs.putUInt("address", id);
    prefs.end();
    ESP_LOGI(TAG_COC, "Node ID set to %u", id);
}

// ========================================================================
// /can_act
// ========================================================================
int act(cJSON *doc)
{
    int qid = 0;
    cJSON *qidObj = cJSON_GetObjectItem(doc, "qid");
    if (qidObj) qid = qidObj->valueint;

    // --- Set address ---
    // {"task":"/can_act", "address": 10}
    cJSON *address = cJSON_GetObjectItem(doc, "address");
    if (address) {
#ifdef UC2_CANOPEN_MASTER
        ESP_LOGW(TAG_COC, "Master node ID cannot be changed at runtime");
#else
        setNodeId((uint8_t)address->valueint);
#endif
        return 1;
    }

    // --- Debug toggle ---
    // {"task":"/can_act", "debug": true}
    cJSON *debug = cJSON_GetObjectItem(doc, "debug");
    if (debug) {
        bool dbg = cJSON_IsTrue(debug);
        ESP_LOGI(TAG_COC, "Debug mode %s", dbg ? "enabled" : "disabled");
        // CANopen stack uses ESP log levels — adjust as needed
        if (dbg)
            esp_log_level_set("UC2_COS", ESP_LOG_DEBUG);
        else
            esp_log_level_set("UC2_COS", ESP_LOG_INFO);
        return 1;
    }

    // --- Restart remote node ---
    // {"task":"/can_act", "restart": 5}
    cJSON *restart = cJSON_GetObjectItem(doc, "restart");
    if (restart) {
#ifdef UC2_CANOPEN_MASTER
        uint8_t targetNode = (uint8_t)restart->valueint;
        ESP_LOGI(TAG_COC, "Sending NMT reset to node %u", targetNode);
        CanOpenStack::resetNode(targetNode);
#else
        ESP_LOGW(TAG_COC, "restart only available on master");
#endif
        return 1;
    }

    // --- OTA via WiFi ---
    // {"task":"/can_act", "ota": {"canid":11, "ssid":"...", "password":"..."}}
    cJSON *ota = cJSON_GetObjectItem(doc, "ota");
    if (ota) {
        ESP_LOGW(TAG_COC, "WiFi-based OTA not available under CANopen (use /can_ota_stream instead)");
        return -1;
    }

    // --- Scan ---
    // {"task":"/can_act", "scan": true}
    cJSON *scan = cJSON_GetObjectItem(doc, "scan");
    if (scan && cJSON_IsTrue(scan)) {
#ifdef UC2_CANOPEN_MASTER
        // Under CANopen, the NMT heartbeat already discovers nodes.
        // Return the current known-node list from NMTManager.
        ESP_LOGI(TAG_COC, "Scan requested — returning NMT node table");
        // Trigger a fresh NMT discovery round (send start-all so nodes send HB)
        NMTManager::sendStart(0);
        return qid;
#else
        ESP_LOGW(TAG_COC, "Scan only available on master");
        return -1;
#endif
    }

    return qid;
}

// ========================================================================
// /can_get
// ========================================================================
cJSON *get(cJSON *doc)
{
    cJSON *out = cJSON_CreateObject();
    if (!out) return nullptr;

    // Mirror the legacy response format
    cJSON_AddItemToObject(out, "input", cJSON_Duplicate(doc, true));
    cJSON_AddNumberToObject(out, "address", CanOpenStack::ownNodeId());
    cJSON_AddNumberToObject(out, "addresspref", getNodeId());
    cJSON_AddStringToObject(out, "stack", "canopen");

    // TWAI bus health
    twai_status_info_t status;
    if (twai_get_status_info(&status) == ESP_OK) {
        cJSON *busHealth = cJSON_CreateObject();
        const char *stateStr = "unknown";
        switch (status.state) {
            case TWAI_STATE_STOPPED:    stateStr = "stopped";    break;
            case TWAI_STATE_RUNNING:    stateStr = "running";    break;
            case TWAI_STATE_BUS_OFF:    stateStr = "bus_off";    break;
            case TWAI_STATE_RECOVERING: stateStr = "recovering"; break;
        }
        cJSON_AddStringToObject(busHealth, "state", stateStr);
        cJSON_AddNumberToObject(busHealth, "tx_error_counter", status.tx_error_counter);
        cJSON_AddNumberToObject(busHealth, "rx_error_counter", status.rx_error_counter);
        cJSON_AddNumberToObject(busHealth, "msgs_to_tx", status.msgs_to_tx);
        cJSON_AddNumberToObject(busHealth, "msgs_to_rx", status.msgs_to_rx);
        cJSON_AddNumberToObject(busHealth, "tx_failed_count", status.tx_failed_count);
        cJSON_AddNumberToObject(busHealth, "rx_missed_count", status.rx_missed_count);
        cJSON_AddNumberToObject(busHealth, "rx_overrun_count", status.rx_overrun_count);
        cJSON_AddNumberToObject(busHealth, "arb_lost_count", status.arb_lost_count);
        cJSON_AddNumberToObject(busHealth, "bus_error_count", status.bus_error_count);
        cJSON_AddItemToObject(out, "bus_health", busHealth);
    }

    // CANopen stack stats
    cJSON_AddNumberToObject(out, "tx_count", CanOpenStack::txCount());
    cJSON_AddNumberToObject(out, "rx_count", CanOpenStack::rxCount());
    cJSON_AddNumberToObject(out, "err_count", CanOpenStack::errorCount());
    cJSON_AddBoolToObject(out, "bus_ok", CanOpenStack::isBusOk());

    // CAN pin info
    cJSON_AddNumberToObject(out, "rx", pinConfig.CAN_RX);
    cJSON_AddNumberToObject(out, "tx", pinConfig.CAN_TX);

    return out;
}

// ========================================================================
// /can_ota — CAN-based OTA (not yet ported to CANopen)
// ========================================================================
cJSON *actCanOta(cJSON * /*doc*/)
{
    cJSON *resp = cJSON_CreateObject();
    if (resp) {
        cJSON_AddBoolToObject(resp, "success", false);
        cJSON_AddStringToObject(resp, "message",
            "CAN OTA not yet available under CANopen stack");
    }
    return resp;
}

cJSON *actCanOtaStream(cJSON * /*doc*/)
{
    cJSON *resp = cJSON_CreateObject();
    if (resp) {
        cJSON_AddBoolToObject(resp, "success", false);
        cJSON_AddStringToObject(resp, "message",
            "CAN OTA streaming not yet available under CANopen stack");
    }
    return resp;
}

void handleOtaLoop()
{
    // No-op — OTA via CANopen not yet implemented
}

} // namespace CanOpenCommands
