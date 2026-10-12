#pragma once
#include "RuntimeConfig.h"

// Forward declaration
struct cJSON;

namespace NVSConfig {
    // Load config from NVS, falling back to compile-time defaults
    void loadConfig();

    // Persist current runtimeConfig to NVS
    void saveConfig();

    // Clear the NVS namespace and reload compile-time defaults
    void resetConfig();

    // Serialize current config to a new cJSON object (caller must free)
    cJSON* toJson();

    // Merge partial JSON into runtimeConfig (does NOT save — call saveConfig after)
    void fromJson(cJSON* json);
}
