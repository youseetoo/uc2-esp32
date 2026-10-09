// Host stub: esp_log.h — logs are collected, not printed (keeps test output clean).
#pragma once
#include <cstdio>

typedef int esp_log_level_t;

#define ESP_LOG_NONE 0
#define ESP_LOG_ERROR 1
#define ESP_LOG_WARN 2
#define ESP_LOG_INFO 3
#define ESP_LOG_DEBUG 4
#define ESP_LOG_VERBOSE 5

// Tag-agnostic no-op macros (variadic).
#define ESP_LOGE(tag, fmt, ...) do { (void)(tag); } while (0)
#define ESP_LOGW(tag, fmt, ...) do { (void)(tag); } while (0)
#define ESP_LOGI(tag, fmt, ...) do { (void)(tag); } while (0)
#define ESP_LOGD(tag, fmt, ...) do { (void)(tag); } while (0)
#define ESP_LOGV(tag, fmt, ...) do { (void)(tag); } while (0)
#define ESP_LOG_LEVEL(level, tag, fmt, ...) do { (void)(level); (void)(tag); } while (0)
#define ESP_LOG_BUFFER_HEX(tag, buf, len) do { (void)(tag); (void)(buf); (void)(len); } while (0)
#define ESP_LOG_BUFFER_HEX_LEVEL(tag, buf, len, level) do { (void)(tag); (void)(buf); (void)(len); (void)(level); } while (0)
#define ESP_LOG_BUFFER_CHAR(tag, buf, len) do { (void)(tag); (void)(buf); (void)(len); } while (0)
#define ESP_LOG_BUFFER_HEX_CHAR(tag, buf, len) do { (void)(tag); (void)(buf); (void)(len); } while (0)

void vprintf(const char* fmt, void* args);
void esp_log_set_level(const char* tag, esp_log_level_t level);
esp_log_level_t esp_log_get_level(const char* tag);
void esp_log_set_vprintf_log_fn(void (*fn)(const char* fmt, ...));
void esp_log_set_vprintf_fn(void (*fn)(const char* fmt, void* args));
