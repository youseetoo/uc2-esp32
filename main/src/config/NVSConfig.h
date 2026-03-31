#pragma once
#include "RuntimeConfig.h"
#include "cJSON.h"

// NVS-backed runtime configuration manager.
// Reads/writes RuntimeConfig fields from/to the "uc2cfg" NVS namespace.
// Missing keys silently fall back to the compile-time defaults in RuntimeConfig.

namespace NVSConfig
{
    // Load config from NVS into the global runtimeConfig.
    // Call once during boot, after nvs_flash_init().
    void load();

    // Persist the current runtimeConfig to NVS.
    void save();

    // Erase the "uc2cfg" NVS namespace and reboot with compiled defaults.
    void reset();

    // Serialize current runtimeConfig to a cJSON object (caller must cJSON_Delete).
    cJSON *toJSON();

    // Merge a partial JSON object into runtimeConfig (for /config_set).
    // Returns true if at least one field was updated.
    bool fromJSON(cJSON *doc);
};
