// Host stub: esp_log.h — logs are collected, not printed (keeps test output clean).
#pragma once
#include <stdio.h>

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

#ifdef __cplusplus
// C++-only: C++ linkage overload of the CRT name. In C, stdio.h already
// declares vprintf (C linkage) — a C file that calls it links against the
// C++-compiled host_impl definition via the mangled-free C symbol.
void vprintf(const char* fmt, void* args);
#endif

// extern "C" guard for the C TUs that include esp_log.h (QidRegistry.c etc.);
// definitions in host_impl.cpp are guarded to match. (vprintf stays C++ —
// it deliberately overloads the CRT name.)
#ifdef __cplusplus
extern "C" {
#endif

void esp_log_set_level(const char* tag, esp_log_level_t level);
#define esp_log_level_set esp_log_set_level // IDF alias
esp_log_level_t esp_log_get_level(const char* tag);
void esp_log_set_vprintf_log_fn(void (*fn)(const char* fmt, ...));
void esp_log_set_vprintf_fn(void (*fn)(const char* fmt, void* args));

#ifdef __cplusplus
}
#endif
