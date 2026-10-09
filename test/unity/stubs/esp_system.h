// Host stub: esp_system.h
#pragma once
#include <cstdint>
#include "esp_err.h"

typedef int esp_reset_reason_t;
#define ESP_RST_POWERON 0
#define ESP_RST_EXT 1
#define ESP_RST_SW 2
#define ESP_RST_PANIC 3
#define ESP_RST_INT_WDT 4
#define ESP_RST_TASK_WDT 5
#define ESP_RST_WDT 6
#define ESP_RST_DEEP_SLEEP 7
#define ESP_RST_BROWNOUT 8
#define ESP_RST_SDIO 9

void esp_restart(void);
uint32_t esp_get_free_heap_size(void);
uint32_t esp_get_free_internal_heap_size(void);
uint32_t esp_get_minimum_free_heap_size(void);
uint32_t esp_get_free_heap_size_static(void);
uint32_t esp_random(void);
esp_reset_reason_t esp_reset_reason(void);
uint32_t esp_sntp_get_time(void);
void ets_delay_us(uint32_t us);  // ESP-IDF microsecond busy-wait (no-op on host)

// Arduino global `ESP` object (subset used by the code under test).
class ESPClass {
public:
    void restart() { esp_restart(); }
    uint32_t getFreeHeap() { return esp_get_free_heap_size(); }
    uint32_t getHeapSize() { return esp_get_free_heap_size(); }
    uint32_t getCpuFreqMHz() { return 240; }
    uint32_t getChipModel() { return 0; }
    void deepSleep(uint32_t t) { (void)t; }
};
extern ESPClass ESP;
