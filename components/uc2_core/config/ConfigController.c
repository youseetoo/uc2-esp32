// T4.1: C (was C++). namespace Config -> free functions (config_*).
// uc2::Preferences -> raw nvs_* (the wrapper's semantics are replicated inline:
//   put* = nvs_set_* + nvs_commit; get* returns the default when the key is
//   absent; clear = nvs_erase_all + nvs_commit). uc2::String + new (getPsxMac)
// -> a static buffer. log_* -> ESP_LOG* (esp_log.h, C-compatible).
#include "ConfigController.h"

#include <string.h>

#include "esp_log.h"
#include "nvs_flash.h"
#include "nvs.h"

static const char *TAG = "ConfigController";
#define PREF_NAMESPACE "UC2"

// PSX MAC buffer returned by config_psx_mac_get (a MAC is 17 chars; 64 is ample).
// Was a heap char* (new char[]) that the caller never freed — now static.
#define PSX_MAC_MAX 64
static char s_psx_mac[PSX_MAC_MAX] = "00:00:00:00:00:00";

void config_setup(const char *pindef_name)
{
    nvs_flash_init();
    ESP_LOGD(TAG, "Setup ConfigController");
    ESP_LOGD(TAG, "Using PinConfig: %s", pindef_name);
    ESP_LOGD(TAG, "Compile time: %s %s", __DATE__, __TIME__);
}

bool config_reset_prefs(void)
{
    nvs_handle_t h = 0;
    ESP_LOGI(TAG, "resetPreferences");
    if (nvs_open(PREF_NAMESPACE, NVS_READWRITE, &h) == ESP_OK) {
        nvs_erase_all(h);
        nvs_commit(h);
        nvs_close(h);
    }
    return true;
}

void config_psx_mac_set(const char *mac)
{
    // NOTE: the original began the namespace READ-ONLY then called putString
    // (a write) — the write silently failed, so the MAC was never persisted.
    // Preserved here (NVS_READONLY) to keep behavior identical; flip to
    // NVS_READWRITE to actually persist the MAC (a bug fix, not a C conversion).
    nvs_handle_t h = 0;
    if (nvs_open(PREF_NAMESPACE, NVS_READONLY, &h) != ESP_OK) {
        return;
    }
    nvs_set_str(h, "mac", mac);
    nvs_commit(h);
    nvs_close(h);
}

char *config_psx_mac_get(void)
{
    nvs_handle_t h = 0;
    size_t len = sizeof(s_psx_mac);
    if (nvs_open(PREF_NAMESPACE, NVS_READONLY, &h) == ESP_OK &&
        nvs_get_str(h, "mac", s_psx_mac, &len) == ESP_OK) {
        nvs_close(h);
    } else {
        if (h) {
            nvs_close(h);
        }
        strcpy(s_psx_mac, "00:00:00:00:00:00");
    }
    return s_psx_mac;
}

int config_psx_type_get(void)
{
    nvs_handle_t h = 0;
    int32_t v = 0;
    if (nvs_open(PREF_NAMESPACE, NVS_READONLY, &h) == ESP_OK) {
        if (nvs_get_i32(h, "controllertype", &v) != ESP_OK) {
            v = 0;
        }
        nvs_close(h);
    }
    return (int)v;
}

void config_psx_type_set(int type)
{
    nvs_handle_t h = 0;
    if (nvs_open(PREF_NAMESPACE, NVS_READWRITE, &h) != ESP_OK) {
        return;
    }
    nvs_set_i32(h, "controllertype", type);
    nvs_commit(h);
    nvs_close(h);
}
