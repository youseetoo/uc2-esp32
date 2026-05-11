// DeviceRouter.h — Routing-table-driven command dispatcher.
// Consults RoutingTable to decide: LOCAL → call controller directly,
// REMOTE → forward via CANopen SDO, DISABLED → log warning.
// Active on all CANopen builds (CAN_CONTROLLER_CANOPEN).
#pragma once

#include <cJSON.h>
#include <stdint.h>

class DeviceRouter {
public:
    // Route a JSON command through the routing table.
    // Returns the JSON response (caller must free with cJSON_Delete),
    // or nullptr if this task is not handled.
    static cJSON* routeCommand(const char* task, cJSON* doc);

    // Per-command-type handlers — each consults RoutingTable::find()
    static cJSON* handleMotorAct(cJSON* doc);
    static cJSON* handleMotorGet(cJSON* doc);
    static cJSON* handleLaserAct(cJSON* doc);
    static cJSON* handleLaserGet(cJSON* doc);
    static cJSON* handleHomeAct(cJSON* doc);
    static cJSON* handleHomeGet(cJSON* doc);
    static cJSON* handleLedAct(cJSON* doc);
    static cJSON* handleLedGet(cJSON* doc);
    static cJSON* handleGalvoAct(cJSON* doc);
    static cJSON* handleGalvoGet(cJSON* doc);
    static cJSON* handleTmcAct(cJSON* doc);
    static cJSON* handleTmcGet(cJSON* doc);

    // State act — currently only routes "restart" to a remote node when
    // "nodeId" is provided in the JSON; other state ops are handled locally.
    static cJSON* handleStateAct(cJSON* doc);

#ifdef CAN_CONTROLLER_CANOPEN
    // /ota_start — JSON preamble that flips the master into binary OTA receive
    // mode. Subsequent raw bytes on Serial are streamed into a PSRAM buffer
    // by OtaBinaryReceive::processBytes() and then forwarded to the slave via
    // CanOpenOTAStreaming::flashSlave().
    static cJSON* handleOtaStart(cJSON* doc);
#endif

private:
#ifdef CAN_CONTROLLER_CANOPEN
    static cJSON* handleStateGet(uint8_t nodeId);
#endif
};
