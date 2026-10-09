// Host-side implementations for the uc2-esp32 Arduino/ESP-IDF stub headers.
// Single-threaded; everything is immediate and side-effect free unless a
// test explicitly needs behavior (Preferences is a real in-memory store).
#include "Arduino.h"
#include "Preferences.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"
#include "freertos/event_groups.h"
#include "esp_err.h"
#include "esp_log.h"
#include "esp_system.h"
#include "esp_mac.h"
#include "esp_task_wdt.h"
#include "driver/gpio.h"
#include "driver/twai.h"
#include "WiFi.h"
#include "ESPmDNS.h"
#include "ArduinoOTA.h"

#include <cstdarg>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <mutex>
#include <string>
#include <map>
#include <vector>

// ── Timing ──────────────────────────────────────────────────────────────────
// A process-wide fake clock, advanced by delay()/delayMicroseconds().
// MockClock (test-side) controls it directly when included.
namespace {
uint64_t g_hostMicros = 0;
std::mutex g_hostClockMutex;
}

uint32_t millis() { return (uint32_t)(g_hostMicros / 1000); }
uint32_t micros() { return (uint32_t)(g_hostMicros); }
void delay(uint32_t ms) { g_hostMicros += (uint64_t)ms * 1000; }
void delayMicroseconds(uint32_t us) { g_hostMicros += us; }

// ── Pin I/O ─────────────────────────────────────────────────────────────────
void pinMode(uint8_t pin, uint8_t mode) { (void)pin; (void)mode; }
void digitalWrite(uint8_t pin, uint8_t val) { (void)pin; (void)val; }
int digitalRead(uint8_t pin) { (void)pin; return 0; }
int analogRead(uint8_t pin) { (void)pin; return 0; }
void analogWrite(uint8_t pin, int val) { (void)pin; (void)val; }
long map(long x, long in_min, long in_max, long out_min, long out_max) {
    return (x - in_min) * (out_max - out_min) / (in_max - in_min) + out_min;
}

// ── Print ───────────────────────────────────────────────────────────────────
void Print::printNumber(unsigned long v, uint8_t base) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%lu", v);
    (void)base;
    write((const uint8_t*)buf, strlen(buf));
}
void Print::printFloat(double v, uint8_t digits) {
    char buf[32];
    snprintf(buf, sizeof(buf), "%.*f", digits, v);
    write((const uint8_t*)buf, strlen(buf));
}

// ── Serial / Wire objects ───────────────────────────────────────────────────
HardwareSerial Serial;
HardwareSerial Serial1;
HardwareSerial Serial2;
TwoWire Wire;
TwoWire Wire1;
WiFiClass WiFi;
MDNSResponder MDNS;
ArduinoOTAClass ArduinoOTA;

// ── FreeRTOS (single-threaded) ──────────────────────────────────────────────
BaseType_t xTaskCreate(TaskFunction_t fn, const char* name, uint32_t stack,
                       void* param, UBaseType_t prio, TaskHandle_t* handle) {
    (void)fn; (void)name; (void)stack; (void)param; (void)prio;
    if (handle) *handle = nullptr;
    return pdPASS;
}
BaseType_t xTaskCreatePinnedToCore(TaskFunction_t fn, const char* name,
                                   uint32_t stack, void* param, UBaseType_t prio,
                                   TaskHandle_t* handle, int core) {
    (void)core;
    return xTaskCreate(fn, name, stack, param, prio, handle);
}
void vTaskDelete(TaskHandle_t handle) { (void)handle; }
void vTaskDelay(TickType_t ticks) { delay((uint32_t)ticks); }
void vTaskDelayUntil(TickType_t* lastWake, TickType_t inc) {
    (void)lastWake;
    delay((uint32_t)inc);
}
void vTaskSuspend(TaskHandle_t handle) { (void)handle; }
void vTaskResume(TaskHandle_t handle) { (void)handle; }
void vTaskSuspendAll(void) {}
BaseType_t xTaskResumeAll(void) { return pdPASS; }
void taskYIELD(void) {}
TickType_t xTaskGetTickCount(void) { return millis() / portTICK_PERIOD_MS; }
uint32_t xTaskGetTickCountFromISR(void) { return xTaskGetTickCount(); }
UBaseType_t uxTaskGetNumberOfTasks(void) { return 1; }

// Semaphores: always available, take/give are no-ops.
namespace {
char g_hostSem;
char g_hostSem2;
}
SemaphoreHandle_t xSemaphoreCreateMutex(void) { return &g_hostSem; }
SemaphoreHandle_t xSemaphoreCreateRecursiveMutex(void) { return &g_hostSem; }
SemaphoreHandle_t xSemaphoreCreateBinary(void) { return &g_hostSem2; }
SemaphoreHandle_t xSemaphoreCreateCounting(UBaseType_t maxCount, UBaseType_t initCount) {
    (void)maxCount; (void)initCount;
    return &g_hostSem2;
}
BaseType_t xSemaphoreTake(SemaphoreHandle_t sem, TickType_t timeout) {
    (void)sem; (void)timeout;
    return pdPASS;
}
BaseType_t xSemaphoreGive(SemaphoreHandle_t sem) { (void)sem; return pdPASS; }
BaseType_t xSemaphoreTakeFromISR(SemaphoreHandle_t sem, BaseType_t* woken) {
    (void)sem;
    if (woken) *woken = pdFALSE;
    return pdPASS;
}
BaseType_t xSemaphoreGiveFromISR(SemaphoreHandle_t sem, BaseType_t* woken) {
    (void)sem;
    if (woken) *woken = pdFALSE;
    return pdPASS;
}
BaseType_t xSemaphoreTakeRecursive(SemaphoreHandle_t sem, TickType_t timeout) {
    (void)sem; (void)timeout;
    return pdPASS;
}
BaseType_t xSemaphoreGiveRecursive(SemaphoreHandle_t sem) { (void)sem; return pdPASS; }
UBaseType_t uxSemaphoreGetCount(SemaphoreHandle_t sem) { (void)sem; return 1; }
void vSemaphoreDelete(SemaphoreHandle_t sem) { (void)sem; }

QueueHandle_t xQueueCreate(UBaseType_t len, UBaseType_t itemSize) {
    (void)len; (void)itemSize;
    return &g_hostSem2;
}
BaseType_t xQueueSend(QueueHandle_t q, const void* item, TickType_t timeout) {
    (void)q; (void)item; (void)timeout;
    return pdPASS;
}
BaseType_t xQueueReceive(QueueHandle_t q, void* item, TickType_t timeout) {
    (void)q; (void)item; (void)timeout;
    return pdFAIL;
}
void vQueueDelete(QueueHandle_t q) { (void)q; }

EventGroupHandle_t xEventGroupCreate(void) { return &g_hostSem2; }
EventBits_t xEventGroupSetBits(EventGroupHandle_t g, EventBits_t bits) {
    (void)g;
    return bits;
}
EventBits_t xEventGroupClearBits(EventGroupHandle_t g, EventBits_t bits) {
    (void)g; (void)bits;
    return 0;
}
EventBits_t xEventGroupWaitBits(EventGroupHandle_t g, EventBits_t bits, BaseType_t clear,
                                BaseType_t any, TickType_t timeout) {
    (void)g; (void)clear; (void)any; (void)timeout;
    return bits;
}
EventBits_t xEventGroupGetBits(EventGroupHandle_t g) { (void)g; return 0; }
void vEventGroupDelete(EventGroupHandle_t g) { (void)g; }

// ── esp_system / esp_mac / esp_task_wdt ─────────────────────────────────────
void esp_restart(void) { /* no-op on host */ }
uint32_t esp_get_free_heap_size(void) { return 320 * 1024; }
uint32_t esp_get_free_internal_heap_size(void) { return 320 * 1024; }
uint32_t esp_get_minimum_free_heap_size(void) { return 300 * 1024; }
uint32_t esp_get_free_heap_size_static(void) { return 0; }
uint32_t esp_random(void) {
    static uint32_t s = 0x12345678;
    s = s * 1103515245 + 12345;
    return s >> 8;
}
esp_reset_reason_t esp_reset_reason(void) { return ESP_RST_POWERON; }
uint32_t esp_sntp_get_time(void) { return 0; }
void ets_delay_us(uint32_t us) { (void)us; }  // no-op on host
ESPClass ESP;  // Arduino global object

esp_err_t esp_read_mac(uint8_t* mac_addr, esp_mac_type_t type) {
    (void)type;
    static const uint8_t fixed[6] = {0xA0, 0xA4, 0x60, 0x12, 0x34, 0x56};
    memcpy(mac_addr, fixed, 6);
    return ESP_OK;
}
esp_err_t esp_base_mac_addr_get(uint8_t* mac) { return esp_read_mac(mac, ESP_MAC_WIFI_STA); }
esp_err_t esp_get_mac(uint8_t* mac) { return esp_read_mac(mac, ESP_MAC_WIFI_STA); }

esp_err_t esp_task_wdt_init(const esp_task_wdt_config_t* config) { (void)config; return ESP_OK; }
esp_err_t esp_task_wdt_init(TickType_t timeout_ms, int auto_load) {
    (void)timeout_ms; (void)auto_load;
    return ESP_OK;
}
esp_err_t esp_task_wdt_register(void* task) { (void)task; return ESP_OK; }
esp_err_t esp_task_wdt_register_low_priority(void* task) { (void)task; return ESP_OK; }
esp_err_t esp_task_wdt_deregister(void* task) { (void)task; return ESP_OK; }
esp_err_t esp_task_wdt_feed(void* task) { (void)task; return ESP_OK; }
esp_err_t esp_task_wdt_reset(void) { return ESP_OK; }
esp_err_t esp_task_wdt_add_task(void* task, esp_task_wdt_add_arg_t arg) {
    (void)task; (void)arg;
    return ESP_OK;
}
esp_err_t esp_task_wdt_remove_task(void* task, esp_task_wdt_add_arg_t arg) {
    (void)task; (void)arg;
    return ESP_OK;
}
esp_err_t esp_task_wdt_enable(void* task) { (void)task; return ESP_OK; }
esp_err_t esp_task_wdt_disable(void* task) { (void)task; return ESP_OK; }
esp_err_t esp_task_wdt_reset_config(void) { return ESP_OK; }

// ── LEDC (no-op state) ──────────────────────────────────────────────────────
namespace {
uint32_t g_hostLedcDuty[64];
}
void ledcSetup(uint8_t timer, double freq, uint8_t duty_bit) {
    (void)timer; (void)freq; (void)duty_bit;
}
void ledcAttachPin(uint8_t pin, uint8_t timer) { (void)pin; (void)timer; }
void ledcDetachPin(uint8_t pin) { if (pin < 64) g_hostLedcDuty[pin] = 0; }
void ledcWrite(uint8_t pin, uint32_t duty) { if (pin < 64) g_hostLedcDuty[pin] = duty; }
uint32_t ledcRead(uint8_t pin) { return (pin < 64) ? g_hostLedcDuty[pin] : 0; }
// Channel-based tone API (Arduino 2.0.x): no-op on host, but record the duty
// on the channel index so a test could observe "a tone was written".
void ledcWriteTone(uint8_t channel, uint32_t freq) { (void)channel; (void)freq; }

// ── Random (Arduino API — deterministic LCG, tests are reproducible) ───────
namespace {
uint32_t g_hostRandState = 0x12345678;
}
uint32_t random(uint32_t max) {
    g_hostRandState = g_hostRandState * 1103515245 + 12345;
    return (max == 0) ? 0 : (g_hostRandState >> 8) % max;
}
uint32_t random(uint32_t lo, uint32_t hi) {
    if (hi <= lo) return lo;
    return lo + random(hi - lo);
}
void randomSeed(uint32_t seed) { g_hostRandState = seed ? seed : 1; }

// ── esp_log ─────────────────────────────────────────────────────────────────
void vprintf(const char* fmt, void* args) {
    std::vprintf(fmt, *reinterpret_cast<std::va_list*>(args));
}

void host_vprintf(const char* fmt, ...) {
    std::va_list ap;
    va_start(ap, fmt);
    std::vfprintf(stderr, fmt, ap);
    va_end(ap);
}
void esp_log_set_level(const char* tag, esp_log_level_t level) { (void)tag; (void)level; }
esp_log_level_t esp_log_get_level(const char* tag) { (void)tag; return ESP_LOG_INFO; }
void esp_log_set_vprintf_log_fn(void (*fn)(const char* fmt, ...)) { (void)fn; }
void esp_log_set_vprintf_fn(void (*fn)(const char* fmt, void* args)) { (void)fn; }

// ── driver/gpio ─────────────────────────────────────────────────────────────
namespace {
std::map<int32_t, int32_t> g_hostGpioLevels;
}
esp_err_t gpio_install_isr_service(int intr_alloc_flags) { (void)intr_alloc_flags; return ESP_OK; }
esp_err_t gpio_isr_handler_add(gpio_num_t gpio_num, gpio_isr_t isr_handler, void* args) {
    (void)gpio_num; (void)isr_handler; (void)args;
    return ESP_OK;
}
esp_err_t gpio_reconfigure(const gpio_config_t* p_gpio_config) {
    (void)p_gpio_config;
    return ESP_OK;
}
esp_err_t gpio_set_direction(gpio_num_t gpio_num, gpio_mode_t mode) {
    (void)gpio_num; (void)mode;
    return ESP_OK;
}
esp_err_t gpio_set_level(gpio_num_t gpio_num, uint32_t level) {
    g_hostGpioLevels[gpio_num] = (int32_t)level;
    return ESP_OK;
}
esp_err_t gpio_get_level(gpio_num_t gpio_num, int32_t* level) {
    auto it = g_hostGpioLevels.find(gpio_num);
    *level = (it != g_hostGpioLevels.end()) ? it->second : 0;
    return ESP_OK;
}
esp_err_t gpio_set_pull_mode(gpio_num_t gpio_num, gpio_pull_mode_t pull) {
    (void)gpio_num; (void)pull;
    return ESP_OK;
}
esp_err_t gpio_set_intr_type(gpio_num_t gpio_num, gpio_interrupt_type_t type) {
    (void)gpio_num; (void)type;
    return ESP_OK;
}
esp_err_t gpio_pad_select_gpio(gpio_num_t gpio_num) { (void)gpio_num; return ESP_OK; }
esp_err_t gpio_matrix_in(gpio_num_t gpio_num, uint32_t sig, bool invert) {
    (void)gpio_num; (void)sig; (void)invert;
    return ESP_OK;
}
esp_err_t gpio_matrix_out(gpio_num_t gpio_num, uint32_t sig, bool open_drain, bool invert) {
    (void)gpio_num; (void)sig; (void)open_drain; (void)invert;
    return ESP_OK;
}
esp_err_t gpio_deep_sleep_hold_dis(void) { return ESP_OK; }
esp_err_t gpio_deep_sleep_hold_en(void) { return ESP_OK; }
esp_err_t gpio_iset_gpio(gpio_num_t gpio_num, uint8_t val) {
    (void)gpio_num; (void)val;
    return ESP_OK;
}
esp_err_t gpio_set_wait_delay(gpio_num_t gpio_num, uint32_t wait_delay) {
    (void)gpio_num; (void)wait_delay;
    return ESP_OK;
}
void gpio_set_edge_int(gpio_num_t gpio_num, gpio_int_type_t type) {
    (void)gpio_num; (void)type;
}
esp_err_t intr_attach(const char* name, intr_handler_t handler, void* arg) {
    (void)name; (void)handler; (void)arg;
    return ESP_OK;
}

// ── driver/twai ─────────────────────────────────────────────────────────────
// Concrete body for the opaque `struct twai_driver` handle from twai.h.
struct twai_driver { bool started = false; };
namespace {
twai_driver g_hostTwai;
}
esp_err_t twai_driver_install(const twai_config_t* config, twai_driver_handle_t* driver) {
    (void)config;
    *driver = &g_hostTwai;
    return ESP_OK;
}
esp_err_t twai_driver_uninstall(twai_driver_handle_t driver) { (void)driver; return ESP_OK; }
esp_err_t twai_start(twai_driver_handle_t driver) {
    if (driver) driver->started = true;
    return ESP_OK;
}
esp_err_t twai_stop(twai_driver_handle_t driver) {
    if (driver) driver->started = false;
    return ESP_OK;
}
esp_err_t twai_transmit(twai_driver_handle_t driver, const twai_message_t* message,
                        uint32_t timeout_ms) {
    (void)driver; (void)message; (void)timeout_ms;
    return ESP_OK;
}
esp_err_t twai_receive(twai_driver_handle_t driver, twai_message_t* message,
                       uint32_t timeout_ms) {
    (void)driver; (void)message; (void)timeout_ms;
    return ESP_ERR_TIMEOUT;
}
esp_err_t twai_get_status_info(twai_driver_handle_t driver, twai_status_info_t* status) {
    (void)driver;
    memset(status, 0, sizeof(*status));
    return ESP_OK;
}
esp_err_t twai_clear_error_counters(twai_driver_handle_t driver) { (void)driver; return ESP_OK; }
const char* twai_status_text(esp_err_t status_code) {
    (void)status_code;
    return "OK";
}
esp_err_t twai_get_timing_config_for_baudrate(uint32_t baudrate, uint32_t clk_src_hz,
                                              uint32_t clk_div, uint8_t sample_point,
                                              uint32_t* timing_config) {
    (void)baudrate; (void)clk_src_hz; (void)clk_div; (void)sample_point;
    if (timing_config) *timing_config = 0;
    return ESP_OK;
}

// ── Preferences (in-memory) ─────────────────────────────────────────────────
std::map<std::string, std::map<std::string, std::string>>& Preferences::backing() {
    static std::map<std::string, std::map<std::string, std::string>> store;
    return store;
}
void Preferences::testClearAll() { backing().clear(); }

bool Preferences::begin(const char* name, bool readOnly) {
    if (!name) return false;
    ns_ = name;
    read_only_ = readOnly;
    open_ = true;
    return true;
}
void Preferences::end() { open_ = false; }

bool Preferences::getRaw(const char* key, std::string& out) const {
    auto ns = backing().find(ns_);
    if (ns == backing().end()) return false;
    auto kv = ns->second.find(key ? key : "");
    if (kv == ns->second.end()) return false;
    out = kv->second;
    return true;
}
bool Preferences::putRaw(const char* key, const std::string& v) {
    if (read_only_) return false;
    backing()[ns_][key ? key : ""] = v;
    return true;
}

bool Preferences::putString(const char* key, const char* value) {
    return putRaw(key, value ? value : "");
}
bool Preferences::getString(const char* key, char* value, size_t* length) {
    std::string s;
    if (!getRaw(key, s)) return false;
    size_t n = length ? *length : (value ? strlen(value) + 1 : s.size() + 1);
    if (value && n) memcpy(value, s.c_str(), (n <= s.size() + 1) ? n : n - 1);
    if (value) value[(n < s.size() + 1) ? n - 1 : n - 1] = 0;
    if (length) *length = s.size();
    return true;
}
bool Preferences::getString(const char* key, std::string& value, size_t* length) {
    if (!getRaw(key, value)) return false;
    if (length) *length = value.size();
    return true;
}
size_t Preferences::getLength(const char* key) {
    std::string s;
    return getRaw(key, s) ? s.size() : 0;
}
bool Preferences::putBytes(const char* key, const void* buffer, size_t length) {
    if (!buffer) return false;
    return putRaw(key, std::string((const char*)buffer, length));
}
bool Preferences::getBytes(const char* key, void* buffer, size_t length, size_t* return_length) {
    std::string s;
    if (!getRaw(key, s)) return false;
    size_t n = (length < s.size()) ? length : s.size();
    if (buffer) memcpy(buffer, s.data(), n);
    if (return_length) *return_length = s.size();
    return true;
}
size_t Preferences::getBytesLength(const char* key) {
    std::string s;
    return getRaw(key, s) ? s.size() : 0;
}

#define PREF_GETTER(name, ctype, dflt) \
    bool Preferences::name(const char* key, ctype defaultValue) { \
        std::string s; \
        if (!getRaw(key, s) || s.empty()) return dflt; \
        char* endp = nullptr; \
        ctype v = (ctype)strtoull(s.c_str(), &endp, 0); \
        return (endp && *endp == 0) ? v : (ctype)defaultValue; \
    } \
    bool Preferences::name##U(const char* key, ctype defaultValue) { \
        std::string s; \
        if (!getRaw(key, s) || s.empty()) return dflt; \
        char* endp = nullptr; \
        ctype v = (ctype)strtoull(s.c_str(), &endp, 10); \
        return (endp && *endp == 0) ? v : (ctype)defaultValue; \
    } \
    bool Preferences::name##Put(const char* key, ctype value) { \
        return putRaw(key, std::to_string((long long)value)); \
    }

// Simple explicit implementations (macro above is fiddly; keep it readable).
bool Preferences::putUChar(const char* key, uint8_t value) {
    return putRaw(key, std::to_string((int)value));
}
bool Preferences::getUChar(const char* key, uint8_t defaultValue) {
    std::string s;
    if (!getRaw(key, s) || s.empty()) return defaultValue;
    return (uint8_t)atoi(s.c_str());
}
bool Preferences::putChar(const char* key, int8_t value) {
    return putRaw(key, std::to_string((int)value));
}
bool Preferences::getChar(const char* key, int8_t defaultValue) {
    std::string s;
    if (!getRaw(key, s) || s.empty()) return defaultValue;
    return (int8_t)atoi(s.c_str());
}
bool Preferences::putShort(const char* key, int16_t value) {
    return putRaw(key, std::to_string((int)value));
}
bool Preferences::getShort(const char* key, int16_t defaultValue) {
    std::string s;
    if (!getRaw(key, s) || s.empty()) return defaultValue;
    return (int16_t)atoi(s.c_str());
}
bool Preferences::putUShort(const char* key, uint16_t value) {
    return putRaw(key, std::to_string((int)value));
}
bool Preferences::getUShort(const char* key, uint16_t defaultValue) {
    std::string s;
    if (!getRaw(key, s) || s.empty()) return defaultValue;
    return (uint16_t)atoi(s.c_str());
}
bool Preferences::putInt(const char* key, int32_t value) {
    return putRaw(key, std::to_string((long long)value));
}
bool Preferences::getInt(const char* key, int32_t defaultValue) {
    std::string s;
    if (!getRaw(key, s) || s.empty()) return defaultValue;
    return (int32_t)atoll(s.c_str());
}
bool Preferences::putUInt(const char* key, uint32_t value) {
    return putRaw(key, std::to_string((unsigned long long)value));
}
bool Preferences::getUInt(const char* key, uint32_t defaultValue) {
    std::string s;
    if (!getRaw(key, s) || s.empty()) return defaultValue;
    return (uint32_t)strtoull(s.c_str(), nullptr, 10);
}
bool Preferences::putFloat(const char* key, float value) {
    return putRaw(key, std::to_string((double)value));
}
bool Preferences::getFloat(const char* key, float defaultValue) {
    std::string s;
    if (!getRaw(key, s) || s.empty()) return defaultValue;
    return (float)atof(s.c_str());
}
bool Preferences::putDouble(const char* key, double value) {
    return putRaw(key, std::to_string(value));
}
bool Preferences::getDouble(const char* key, double defaultValue) {
    std::string s;
    if (!getRaw(key, s) || s.empty()) return defaultValue;
    return atof(s.c_str());
}
bool Preferences::putBool(const char* key, bool value) {
    return putRaw(key, value ? "1" : "0");
}
bool Preferences::getBool(const char* key, bool defaultValue) {
    std::string s;
    if (!getRaw(key, s) || s.empty()) return defaultValue;
    return atoi(s.c_str()) != 0;
}
bool Preferences::isKey(const char* key) {
    std::string s;
    return getRaw(key, s);
}
bool Preferences::remove(const char* key) {
    if (read_only_) return false;
    auto ns = backing().find(ns_);
    if (ns == backing().end()) return false;
    return ns->second.erase(key ? key : "") > 0;
}
bool Preferences::clear() {
    if (read_only_) return false;
    backing()[ns_].clear();
    return true;
}
int Preferences::getNamespaceCount(bool readOnly) {
    (void)readOnly;
    return (int)backing().size();
}
int Preferences::getKeyCount() {
    auto ns = backing().find(ns_);
    return (ns != backing().end()) ? (int)ns->second.size() : 0;
}
