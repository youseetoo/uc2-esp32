// T1.3 (no Arduino): log_* → ESP_LOG*.
// Call sites in main/ use the Arduino signature log_i("fmt", ...) with an
// implicit tag; every converted module keeps its own
// `static const char *TAG` and the macros forward to it.
#pragma once

#include "esp_log.h"

#define log_d(fmt, ...) ESP_LOGD(TAG, fmt, ##__VA_ARGS__)
#define log_i(fmt, ...) ESP_LOGI(TAG, fmt, ##__VA_ARGS__)
#define log_w(fmt, ...) ESP_LOGW(TAG, fmt, ##__VA_ARGS__)
#define log_e(fmt, ...) ESP_LOGE(TAG, fmt, ##__VA_ARGS__)
