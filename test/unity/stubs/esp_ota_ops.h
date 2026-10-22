// Host stub: esp_ota_ops.h
//
// Minimal OTA surface for the uc2_canopen OTA classes (CanOpenOTA,
// OtaBinaryReceive). All operations are immediate no-ops that succeed, so
// the state machines can be exercised on the host without flash.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef uint32_t esp_ota_handle_t;

// Host partition type: only the members the OTA code touches (label).
// sizeof must be consistent across TUs (it is — plain struct, no padding
// beyond the array).
typedef struct {
    char label[17];
} esp_partition_t;

typedef enum {
    ESP_OTA_IMG_PENDING_VERIFY = 0,
    ESP_OTA_IMG_VALID = 1,
    ESP_OTA_IMG_INVALID = 2,
    ESP_OTA_IMG_ABORTED = 3,
    ESP_OTA_IMG_NEW = 4,
} esp_ota_img_state_t;

const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t* from);
const esp_partition_t* esp_ota_get_running_partition(void);
esp_err_t esp_ota_begin(const esp_partition_t* partition, uint32_t size, esp_ota_handle_t* handle);
esp_err_t esp_ota_write(esp_ota_handle_t handle, const void* data, size_t len);
esp_err_t esp_ota_end(esp_ota_handle_t handle);
esp_err_t esp_ota_abort(esp_ota_handle_t handle);
esp_err_t esp_ota_set_boot_partition(const esp_partition_t* partition);
esp_ota_img_state_t esp_ota_get_state_partition(const esp_partition_t* partition);

#ifdef __cplusplus
}
#endif
