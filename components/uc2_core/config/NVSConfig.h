#pragma once
// T4.1: C (was C++). namespace NVSConfig -> free functions (nvsconfig_*).
// loadConfig gains a pin_cfg_node_id parameter: the original read the C++
// global pinConfig.CAN_ID_CURRENT directly, but a .c TU cannot touch a C++
// object, so the C++ caller passes (pinConfig.CAN_ID_CURRENT & 0x7F). reset
// re-loads using the node id captured by the most recent nvsconfig_load.
#include <stdint.h>
#include "RuntimeConfig.h"

#include <cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

// Load config from NVS, falling back to compile-time defaults.
// pin_cfg_node_id = the board's CAN_ID_CURRENT (masked to 1..127) — used as the
// default canNodeId on first boot (before LSS/serial overrides it in NVS).
void nvsconfig_load(uint8_t pin_cfg_node_id);

// Persist current runtimeConfig to NVS
void nvsconfig_save(void);

// Clear the NVS namespace and reload compile-time defaults
void nvsconfig_reset(void);

// Serialize current config to a new cJSON object (caller must free)
cJSON *nvsconfig_to_json(void);

// Merge partial JSON into runtimeConfig (does NOT save — call nvsconfig_save after)
void nvsconfig_from_json(cJSON *json);

#ifdef __cplusplus
}
#endif
