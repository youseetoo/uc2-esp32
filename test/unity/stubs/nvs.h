// Host stub: nvs.h — in-memory NVS (namespace -> key -> byte blob).
// API = the IDF 6.1 subset used by uc2_hal/uc2_prefs.h (grep-verified):
// nvs_open (handle as OUT parameter, 6.1 style) / close / commit /
// erase_key / erase_all / set_{u8,i8,u16,i32,float,str,blob} /
// get_{u8,i8,u16,i32,float,str,blob}. Definitions: host_impl.cpp.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#define ESP_ERR_NVS_NOT_FOUND 0x1101
#define ESP_ERR_NVS_INVALID_LENGTH 0x1102
#define ESP_ERR_NVS_READ_ONLY 0x1103
#define ESP_ERR_NVS_FULL 0x1104
#define ESP_ERR_NVS_INVALID_STATE 0x1105

typedef uint32_t nvs_handle_t;

typedef enum {
    NVS_READONLY = 0,
    NVS_READWRITE = 1
} nvs_open_mode_t;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t nvs_open(const char* namespace_name, nvs_open_mode_t open_mode,
                   nvs_handle_t* out_handle);
void nvs_close(nvs_handle_t handle);
esp_err_t nvs_commit(nvs_handle_t handle);
esp_err_t nvs_erase_key(nvs_handle_t handle, const char* key);
esp_err_t nvs_erase_all(nvs_handle_t handle);

esp_err_t nvs_set_u8(nvs_handle_t handle, const char* key, uint8_t value);
esp_err_t nvs_set_i8(nvs_handle_t handle, const char* key, int8_t value);
esp_err_t nvs_set_u16(nvs_handle_t handle, const char* key, uint16_t value);
esp_err_t nvs_set_i32(nvs_handle_t handle, const char* key, int32_t value);
esp_err_t nvs_set_float(nvs_handle_t handle, const char* key, float value);
esp_err_t nvs_set_str(nvs_handle_t handle, const char* key, const char* value);
esp_err_t nvs_set_blob(nvs_handle_t handle, const char* key, const void* value,
                       size_t length);

esp_err_t nvs_get_u8(nvs_handle_t handle, const char* key, uint8_t* out_value);
esp_err_t nvs_get_i8(nvs_handle_t handle, const char* key, int8_t* out_value);
esp_err_t nvs_get_u16(nvs_handle_t handle, const char* key, uint16_t* out_value);
esp_err_t nvs_get_i32(nvs_handle_t handle, const char* key, int32_t* out_value);
esp_err_t nvs_get_float(nvs_handle_t handle, const char* key, float* out_value);
// *length in = buffer size, out = stored length INCLUDING the trailing NUL
// (IDF semantics). Buffer too small -> ESP_ERR_NVS_INVALID_LENGTH,
// *length = required size.
esp_err_t nvs_get_str(nvs_handle_t handle, const char* key, char* out_value,
                      size_t* length);
// buf == NULL -> length query (*length = stored size), like IDF 6.1.
esp_err_t nvs_get_blob(nvs_handle_t handle, const char* key, void* buf,
                       size_t* length);

#ifdef __cplusplus
}
#endif
