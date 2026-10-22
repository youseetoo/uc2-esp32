// Host stub: nvs_flash.h — the in-memory NVS needs no flash init; erase()
// wipes every namespace (tests rely on a clean slate per process).
#pragma once
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t nvs_flash_init(void);
esp_err_t nvs_flash_erase(void);
esp_err_t nvs_flash_deinit(void);

#ifdef __cplusplus
}
#endif
