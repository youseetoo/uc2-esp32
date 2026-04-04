/**
 * CanOpenCommands.h
 *
 * CANopen replacement for can_controller act/get JSON commands.
 * Provides /can_act, /can_get, /can_ota, /can_ota_stream endpoints
 * using the CANopen stack instead of the legacy ISO-TP transport.
 */
#pragma once
#include "cJSON.h"
#include <stdint.h>

namespace CanOpenCommands {

// JSON command handlers (mirror the legacy can_controller API)
int    act(cJSON *doc);
cJSON *get(cJSON *doc);

// OTA over CANopen (not yet implemented — returns error stub)
cJSON *actCanOta(cJSON *doc);
cJSON *actCanOtaStream(cJSON *doc);

// Non-blocking OTA loop handler (no-op under CANopen for now)
void handleOtaLoop();

// Node ID accessors (replaces can_controller::getCANAddress / device_can_id)
uint8_t getNodeId();
void    setNodeId(uint8_t id);

} // namespace CanOpenCommands
