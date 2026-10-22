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
#include "nvs.h"
#include "nvs_flash.h"
#include "esp_task_wdt.h"
#include "driver/gpio.h"
#include "soc/gpio_struct.h"
#include "driver/twai.h"
#include "esp_adc/adc_oneshot.h"
#include "esp_adc/adc_cali_scheme.h"
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

// C-linkage accessors for the fake clock: the esp_timer.h / esp_rom_sys.h
// stub headers (included from C TUs via uc2_hal/uc2_time.h) need the SAME
// clock so the C-side millis()/micros() match the Arduino ones above.
extern "C" uint64_t host_fake_clock_us(void) { return g_hostMicros; }
extern "C" void host_fake_clock_advance_us(uint32_t us) { g_hostMicros += us; }

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
// extern "C": the freertos stub headers are C-linkage guarded, and C TUs
// (uc2_time.h delay(), FocusMotor.c semaphores, AxisNotify/EncoderMonitor
// queues) call these directly.
extern "C" {
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
uint8_t xPortGetCoreID(void) { return 0; }  // host is single-core

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
BaseType_t xQueueSendToBackFromISR(QueueHandle_t q, const void* item, BaseType_t* woken) {
    (void)q; (void)item; (void)woken;
    return pdPASS;
}
BaseType_t xQueueSendToFront(QueueHandle_t q, const void* item, TickType_t timeout) {
    (void)q; (void)item; (void)timeout;
    return pdPASS;
}
UBaseType_t uxQueueMessagesWaiting(QueueHandle_t q) { (void)q; return 0; }
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
}  // extern "C" — FreeRTOS

// ── esp_system / esp_mac / esp_task_wdt ─────────────────────────────────────
// extern "C" (matches the guarded stub headers; C TUs call these directly).
extern "C" {
void esp_restart(void) { /* no-op on host */ }
uint32_t esp_get_free_heap_size(void) { return 320 * 1024; }
uint32_t esp_get_free_internal_heap_size(void) { return 320 * 1024; }
uint32_t esp_get_minimum_free_heap_size(void) { return 300 * 1024; }
uint32_t esp_get_free_heap_size_static(void) { return 0; }
// esp_random: defined by the static inline in esp_random.h (every caller
// includes it via uc2_misc.h); a definition here had C++ linkage and
// conflicted with the extern "C" declaration (C2732).
esp_reset_reason_t esp_reset_reason(void) { return ESP_RST_POWERON; }
uint32_t esp_sntp_get_time(void) { return 0; }
void ets_delay_us(uint32_t us) { (void)us; }  // no-op on host
}  // extern "C" — esp_system functions

extern "C" {
esp_err_t esp_read_mac(uint8_t* mac_addr, esp_mac_type_t type) {
    (void)type;
    static const uint8_t fixed[6] = {0xA0, 0xA4, 0x60, 0x12, 0x34, 0x56};
    memcpy(mac_addr, fixed, 6);
    return ESP_OK;
}
esp_err_t esp_base_mac_addr_get(uint8_t* mac) { return esp_read_mac(mac, ESP_MAC_WIFI_STA); }
esp_err_t esp_get_mac(uint8_t* mac) { return esp_read_mac(mac, ESP_MAC_WIFI_STA); }

esp_err_t esp_task_wdt_init(const esp_task_wdt_config_t* config) { (void)config; return ESP_OK; }
esp_err_t esp_task_wdt_register(void* task) { (void)task; return ESP_OK; }
esp_err_t esp_task_wdt_register_low_priority(void* task) { (void)task; return ESP_OK; }
esp_err_t esp_task_wdt_deregister(void* task) { (void)task; return ESP_OK; }
esp_err_t esp_task_wdt_feed(void* task) { (void)task; return ESP_OK; }
esp_err_t esp_task_wdt_reset(void) { return ESP_OK; }
esp_err_t esp_task_wdt_add(void* task) { (void)task; return ESP_OK; }
esp_err_t esp_task_wdt_remove(void* task) { (void)task; return ESP_OK; }
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

uint32_t esp_cpu_get_cycle_count(void) { return (uint32_t)(g_hostMicros * 240); }
}  // extern "C" — esp_mac / esp_task_wdt / esp_cpu

// C++ linkage: matches the C++-only overload declared outside the extern "C"
// block in esp_task_wdt.h (C linkage forbids overloading, C2733).
esp_err_t esp_task_wdt_init(TickType_t timeout_ms, int auto_load) {
    (void)timeout_ms; (void)auto_load;
    return ESP_OK;
}

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
extern "C" {  // matches the guarded declarations in esp_log.h (C TUs include it)
void esp_log_set_level(const char* tag, esp_log_level_t level) { (void)tag; (void)level; }
esp_log_level_t esp_log_get_level(const char* tag) { (void)tag; return ESP_LOG_INFO; }
void esp_log_set_vprintf_log_fn(void (*fn)(const char* fmt, ...)) { (void)fn; }
void esp_log_set_vprintf_fn(void (*fn)(const char* fmt, void* args)) { (void)fn; }
// Host: not a real error table — ESP_OK gets a name, anything else a hex form
// (single static buffer; host builds are effectively single-threaded for logs).
const char* esp_err_to_name(esp_err_t code) {
    if (code == ESP_OK) return "ESP_OK";
    static char buf[16];
    snprintf(buf, sizeof buf, "ESP_ERR_0x%lx", (unsigned long)code);
    return buf;
}
}  // extern "C" — esp_log

// ── driver/spi_master (host: no hardware; records the last transaction) ──────
// The C island (dac_mcp4822.c) transmits through these; a test could inspect
// g_hostSpiLastTx via host_spi_last_transaction() if needed.
#include "driver/spi_master.h"
namespace {
spi_transaction_t* g_hostSpiLastTx = nullptr;
}
extern "C" {
esp_err_t spi_bus_initialize(spi_host_device_t host, const spi_bus_config_t* bus_config,
                             spi_dma_chan_t dma_chan) {
    (void)host; (void)bus_config; (void)dma_chan;
    return ESP_OK;
}
esp_err_t spi_bus_free(spi_host_device_t host) { (void)host; return ESP_OK; }
esp_err_t spi_bus_add_device(spi_host_device_t host,
                             const spi_device_interface_config_t* dev_config,
                             spi_device_handle_t* handle) {
    (void)host; (void)dev_config;
    static char s_hostSpiDevice;
    if (handle) *handle = &s_hostSpiDevice;
    return ESP_OK;
}
esp_err_t spi_bus_remove_device(spi_device_handle_t handle) { (void)handle; return ESP_OK; }
esp_err_t spi_device_transmit(spi_device_handle_t device, spi_transaction_t* trans) {
    (void)device;
    g_hostSpiLastTx = trans;
    return ESP_OK;
}
esp_err_t spi_device_polling_transmit(spi_device_handle_t device, spi_transaction_t* trans) {
    (void)device;
    g_hostSpiLastTx = trans;
    return ESP_OK;
}
esp_err_t spi_device_queue_trans(spi_device_handle_t device, spi_transaction_t* trans,
                                 uint32_t timeout_ms) {
    (void)timeout_ms;
    return spi_device_transmit(device, trans);
}
spi_transaction_t* spi_device_get_trans_result(spi_device_handle_t device, uint32_t timeout_ms) {
    (void)device; (void)timeout_ms;
    spi_transaction_t* t = g_hostSpiLastTx;
    g_hostSpiLastTx = nullptr;
    return t;
}
void spi_transaction_data_clear(spi_transaction_t* trans) { (void)trans; }
}  // extern "C" — spi_master
spi_transaction_t* host_spi_last_transaction(void) { return g_hostSpiLastTx; }

// ── driver/i2c_master + driver/i2c_slave (host: in-memory bus) ───────────────
// uc2_wire.cpp (the real Wire façade) runs against this. One global bus stands
// in for I2C_NUM_0; devices are keyed by address:
//   * master transmit:  recorded per device (host_i2c_take_tx)
//   * master receive:   served from per-device injected RX (host_i2c_inject_rx);
//                       a device with no injected bytes NACKs
//                       (ESP_ERR_INVALID_RESPONSE), like an absent chip.
#include "driver/i2c_master.h"
#include "driver/i2c_slave.h"
namespace {
struct HostI2cDev {
    std::vector<uint8_t> tx;  // bytes the master transmitted to this address
    std::vector<uint8_t> rx;  // test-injected RX bytes
};
std::map<uint16_t, HostI2cDev> g_hostI2cDevices;
std::map<const void*, uint16_t> g_hostI2cDevAddr;  // handle -> address

uint16_t hostI2cAddrOf(const void* handle) {
    auto it = g_hostI2cDevAddr.find(handle);
    return (it != g_hostI2cDevAddr.end()) ? it->second : 0;
}
}
extern "C" {
esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t* cfg,
                             i2c_master_bus_handle_t* out) {
    (void)cfg;
    static char s_hostI2cBus;
    if (out) *out = &s_hostI2cBus;
    return ESP_OK;
}
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus,
                                    const i2c_device_config_t* dc,
                                    i2c_master_dev_handle_t* out) {
    (void)bus;
    if (!dc || !out) return ESP_ERR_INVALID_ARG;
    static char s_hostI2cDevPool[32];
    static int s_hostI2cDevNext = 0;
    if (s_hostI2cDevNext >= 32) return ESP_ERR_NO_MEM;
    *out = &s_hostI2cDevPool[s_hostI2cDevNext++];
    g_hostI2cDevAddr[*out] = (uint16_t)dc->device_address;
    return ESP_OK;
}
esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t bus) {
    (void)bus;
    g_hostI2cDevices.clear();
    g_hostI2cDevAddr.clear();
    return ESP_OK;
}
esp_err_t i2c_master_transmit(i2c_master_dev_handle_t dev, const uint8_t* wr,
                              size_t wr_len, int32_t ticks_to_wait) {
    (void)ticks_to_wait;
    if (!dev) return ESP_ERR_INVALID_ARG;
    uint16_t addr = hostI2cAddrOf(dev);
    if (addr == 0) return ESP_ERR_INVALID_STATE;
    auto& d = g_hostI2cDevices[addr];
    if (wr) d.tx.insert(d.tx.end(), wr, wr + wr_len);
    return ESP_OK;
}
esp_err_t i2c_master_receive(i2c_master_dev_handle_t dev, uint8_t* rd,
                             size_t rd_len, int32_t ticks_to_wait) {
    (void)ticks_to_wait;
    if (!dev) return ESP_ERR_INVALID_ARG;
    uint16_t addr = hostI2cAddrOf(dev);
    if (addr == 0) return ESP_ERR_INVALID_STATE;
    auto& d = g_hostI2cDevices[addr];
    if (d.rx.empty()) return ESP_ERR_INVALID_RESPONSE;  // nothing there -> NACK
    size_t n = d.rx.size() < rd_len ? d.rx.size() : rd_len;
    if (rd) {
        memcpy(rd, d.rx.data(), n);
        d.rx.erase(d.rx.begin(), d.rx.begin() + (long)n);
    }
    return n == rd_len ? ESP_OK : ESP_ERR_INVALID_RESPONSE;
}
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t dev,
                                      const uint8_t* wr, size_t wr_len,
                                      uint8_t* rd, size_t rd_len,
                                      int32_t ticks_to_wait) {
    esp_err_t err = i2c_master_transmit(dev, wr, wr_len, ticks_to_wait);
    if (err != ESP_OK) return err;
    return i2c_master_receive(dev, rd, rd_len, ticks_to_wait);
}
esp_err_t i2c_new_slave_device(const i2c_slave_config_t* cfg,
                               i2c_slave_dev_handle_t* out) {
    static char s_hostI2cSlave;
    if (!cfg) return ESP_ERR_INVALID_ARG;
    if (out) *out = &s_hostI2cSlave;
    return ESP_OK;
}
esp_err_t i2c_del_slave_device(i2c_slave_dev_handle_t dev) { (void)dev; return ESP_OK; }
esp_err_t i2c_slave_register_event_callbacks(i2c_slave_dev_handle_t dev,
                                             i2c_slave_event_callbacks_t* cbs, void* arg) {
    (void)dev; (void)cbs; (void)arg;  // slave mode not exercised on host
    return ESP_OK;
}
esp_err_t i2c_slave_write(i2c_slave_dev_handle_t dev, const uint8_t* data,
                          size_t len, size_t* bytes_transferred, uint32_t ticks_to_wait) {
    (void)dev; (void)data; (void)ticks_to_wait;
    if (bytes_transferred) *bytes_transferred = len;
    return ESP_OK;
}
}  // extern "C" — i2c v2

// Test hooks (C++): drive the in-memory I2C bus from the test TUs.
void host_i2c_inject_rx(uint16_t addr, const uint8_t* data, size_t len) {
    auto& d = g_hostI2cDevices[addr];
    if (data && len) d.rx.insert(d.rx.end(), data, data + len);
}
size_t host_i2c_take_tx(uint16_t addr, uint8_t* buf, size_t cap) {
    auto it = g_hostI2cDevices.find(addr);
    if (it == g_hostI2cDevices.end()) return 0;
    size_t n = it->second.tx.size() < cap ? it->second.tx.size() : cap;
    if (n > 0 && buf) memcpy(buf, it->second.tx.data(), n);
    it->second.tx.erase(it->second.tx.begin(), it->second.tx.begin() + (long)n);
    return n;
}
size_t host_i2c_tx_count(uint16_t addr) {
    auto it = g_hostI2cDevices.find(addr);
    return (it != g_hostI2cDevices.end()) ? it->second.tx.size() : 0;
}

// ── driver/temperature_sensor (host: fixed 25 °C) ────────────────────────────
#include "driver/temperature_sensor.h"
namespace { char g_hostTsenStorage; }
extern "C" {
esp_err_t temperature_sensor_install(const temperature_sensor_config_t* config, temperature_sensor_handle_t* handle) {
    (void)config;
    if (handle) *handle = (temperature_sensor_handle_t)(void*)&g_hostTsenStorage;
    return ESP_OK;
}
esp_err_t temperature_sensor_enable(temperature_sensor_handle_t handle) { (void)handle; return ESP_OK; }
esp_err_t temperature_sensor_disable(temperature_sensor_handle_t handle) { (void)handle; return ESP_OK; }
esp_err_t temperature_sensor_get_celsius(temperature_sensor_handle_t handle, float* celsius) {
    (void)handle;
    if (celsius) *celsius = 25.0f;
    return ESP_OK;
}
esp_err_t temperature_sensor_deinit(temperature_sensor_handle_t handle) { (void)handle; return ESP_OK; }
}  // extern "C" — temperature_sensor

// ── driver/gpio ─────────────────────────────────────────────────────────────
namespace {
std::map<int32_t, int32_t> g_hostGpioLevels;
}
// The GPIO register-block instance declared by soc/gpio_struct.h (the stub has
// no extern "C" guard, so C and C++ TUs both see C linkage). Zeroed dummy:
// HighSpeedScannerCore.cpp writes its set/clear members as no-ops on host.
gpio_function_t GPIO = {0};
extern "C" {  // driver/gpio — C-linkage (matches the driver/gpio.h stub guard)
esp_err_t gpio_install_isr_service(int intr_alloc_flags) { (void)intr_alloc_flags; return ESP_OK; }
esp_err_t gpio_isr_handler_add(gpio_num_t gpio_num, gpio_isr_t isr_handler, void* args) {
    (void)gpio_num; (void)isr_handler; (void)args;
    return ESP_OK;
}
esp_err_t gpio_config(const gpio_config_t* p_gpio_config) {
    (void)p_gpio_config;  // host sim tracks levels only, not mode/pull/intr
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
int gpio_get_level(gpio_num_t gpio_num) {
    auto it = g_hostGpioLevels.find(gpio_num);
    return (it != g_hostGpioLevels.end()) ? it->second : 0;
}
esp_err_t gpio_set_pull_mode(gpio_num_t gpio_num, gpio_pull_mode_t pull) {
    (void)gpio_num; (void)pull;
    return ESP_OK;
}
esp_err_t gpio_set_intr_type(gpio_num_t gpio_num, gpio_interrupt_type_t type) {
    (void)gpio_num; (void)type;
    return ESP_OK;
}
esp_err_t gpio_intr_enable(gpio_num_t gpio_num) { (void)gpio_num; return ESP_OK; }
esp_err_t gpio_isr_handler_remove(gpio_num_t gpio_num) {
    (void)gpio_num;
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
}  // extern "C" — driver/gpio


// ── driver/twai (IDF 6.1 API) ───────────────────────────────────────────────
namespace { bool s_twaiStarted = false; }
esp_err_t twai_driver_install(const twai_general_config_t* general_conf,
                              const twai_timing_config_t* timing_conf,
                              const twai_filter_config_t* filter_conf) {
    (void)general_conf; (void)timing_conf; (void)filter_conf;
    return ESP_OK;
}
esp_err_t twai_driver_uninstall(void) { return ESP_OK; }
esp_err_t twai_start(void) { s_twaiStarted = true; return ESP_OK; }
esp_err_t twai_stop(void) { s_twaiStarted = false; return ESP_OK; }
esp_err_t twai_transmit(const twai_message_t* message, uint32_t timeout_ms) {
    (void)message; (void)timeout_ms;
    return ESP_OK;
}
esp_err_t twai_receive(twai_message_t* message, uint32_t timeout_ms) {
    (void)message; (void)timeout_ms;
    return ESP_ERR_TIMEOUT;
}
esp_err_t twai_reconfigure_alerts(uint32_t alerts, void* user_data) {
    (void)alerts; (void)user_data;
    return ESP_OK;
}
esp_err_t twai_initiate_recovery(void) { return ESP_OK; }
esp_err_t twai_read_alerts(uint32_t* out_alerts, uint32_t timeout_ms) {
    (void)timeout_ms;
    if (out_alerts) *out_alerts = TWAI_ALERT_NONE;
    return ESP_OK;
}
esp_err_t twai_get_status_info(twai_status_info_t* status) {
    memset(status, 0, sizeof(*status));
    return ESP_OK;
}
esp_err_t twai_clear_error_counters(void) { return ESP_OK; }
const char* twai_status_text(esp_err_t status_code) {
    (void)status_code;
    return "OK";
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

// ── nvs (in-memory; declarations in nvs.h / nvs_flash.h) ────────────────────
// Key values are std::string (binary-safe; str values keep their trailing NUL,
// like real NVS). Handles are monotonic ids into g_hostNvsOpen and are never
// reused, so a stale handle can't alias a closed namespace. This store is
// separate from the legacy Arduino Preferences `backing()` above.
namespace {
std::map<std::string, std::map<std::string, std::string>> g_hostNvsNamespaces;
std::map<nvs_handle_t, std::string> g_hostNvsOpen;
nvs_handle_t g_hostNvsNextHandle = 1;

std::map<std::string, std::string> *nvsNs(nvs_handle_t h) {
    auto it = g_hostNvsOpen.find(h);
    if (it == g_hostNvsOpen.end()) return nullptr;
    return &g_hostNvsNamespaces[it->second];
}
esp_err_t nvsGetFixed(nvs_handle_t h, const char *key, size_t want, void *out) {
    auto *ns = nvsNs(h);
    if (!ns || key == nullptr || out == nullptr) return ESP_ERR_INVALID_ARG;
    auto it = ns->find(key);
    if (it == ns->end()) return ESP_ERR_NVS_NOT_FOUND;
    if (it->second.size() != want) return ESP_ERR_NVS_INVALID_LENGTH;  // type/size mismatch, like IDF
    memcpy(out, it->second.data(), want);
    return ESP_OK;
}
}

extern "C" {
esp_err_t nvs_flash_init(void) { return ESP_OK; }
esp_err_t nvs_flash_erase(void) {
    g_hostNvsNamespaces.clear();
    g_hostNvsOpen.clear();
    return ESP_OK;
}
esp_err_t nvs_flash_deinit(void) { return ESP_OK; }

esp_err_t nvs_open(const char *namespace_name, nvs_open_mode_t open_mode,
                   nvs_handle_t *out_handle) {
    if (namespace_name == nullptr || out_handle == nullptr) return ESP_ERR_INVALID_ARG;
    // Independent handles on one namespace are fine (uc2_prefs.h relies on it).
    for (auto &kv : g_hostNvsOpen) {
        if (kv.second == namespace_name) { *out_handle = kv.first; return ESP_OK; }
    }
    if (open_mode == NVS_READONLY && !g_hostNvsNamespaces.count(namespace_name))
        return ESP_ERR_NVS_NOT_FOUND;  // real IDF: opening missing ns readonly fails
    *out_handle = g_hostNvsNextHandle++;
    g_hostNvsOpen[*out_handle] = namespace_name;
    (void)g_hostNvsNamespaces[namespace_name];  // create if missing
    return ESP_OK;
}
void nvs_close(nvs_handle_t handle) { g_hostNvsOpen.erase(handle); }
esp_err_t nvs_commit(nvs_handle_t handle) {  // data is committed immediately
    return nvsNs(handle) ? ESP_OK : ESP_ERR_INVALID_STATE;
}
esp_err_t nvs_erase_key(nvs_handle_t handle, const char *key) {
    auto *ns = nvsNs(handle);
    if (!ns || key == nullptr) return ESP_ERR_INVALID_ARG;
    if (ns->erase(key) == 0) return ESP_ERR_NVS_NOT_FOUND;
    return ESP_OK;
}
esp_err_t nvs_erase_all(nvs_handle_t handle) {
    auto *ns = nvsNs(handle);
    if (!ns) return ESP_ERR_INVALID_STATE;
    ns->clear();
    return ESP_OK;
}

esp_err_t nvs_set_blob(nvs_handle_t handle, const char *key, const void *value,
                       size_t length) {
    auto *ns = nvsNs(handle);
    if (!ns || key == nullptr || (value == nullptr && length != 0)) return ESP_ERR_INVALID_ARG;
    (*ns)[key] = std::string((const char *)value, length);
    return ESP_OK;
}
esp_err_t nvs_get_blob(nvs_handle_t handle, const char *key, void *buf, size_t *length) {
    auto *ns = nvsNs(handle);
    if (!ns || key == nullptr || length == nullptr) return ESP_ERR_INVALID_ARG;
    auto it = ns->find(key);
    if (it == ns->end()) return ESP_ERR_NVS_NOT_FOUND;
    if (buf == nullptr) { *length = it->second.size(); return ESP_OK; }  // length query
    size_t copied = it->second.size() < *length ? it->second.size() : *length;
    memcpy(buf, it->second.data(), copied);
    *length = copied;
    return ESP_OK;
}

esp_err_t nvs_set_u8(nvs_handle_t h, const char *k, uint8_t v) { return nvs_set_blob(h, k, &v, sizeof(v)); }
esp_err_t nvs_set_i8(nvs_handle_t h, const char *k, int8_t v) { return nvs_set_blob(h, k, &v, sizeof(v)); }
esp_err_t nvs_set_u16(nvs_handle_t h, const char *k, uint16_t v) { return nvs_set_blob(h, k, &v, sizeof(v)); }
esp_err_t nvs_set_i32(nvs_handle_t h, const char *k, int32_t v) { return nvs_set_blob(h, k, &v, sizeof(v)); }
esp_err_t nvs_set_float(nvs_handle_t h, const char *k, float v) { return nvs_set_blob(h, k, &v, sizeof(v)); }
esp_err_t nvs_set_str(nvs_handle_t h, const char *k, const char *v) {
    if (v == nullptr) return ESP_ERR_INVALID_ARG;
    return nvs_set_blob(h, k, v, strlen(v) + 1);  // store with trailing NUL, like IDF
}

esp_err_t nvs_get_u8(nvs_handle_t h, const char *k, uint8_t *v) { return nvsGetFixed(h, k, sizeof(uint8_t), v); }
esp_err_t nvs_get_i8(nvs_handle_t h, const char *k, int8_t *v) { return nvsGetFixed(h, k, sizeof(int8_t), v); }
esp_err_t nvs_get_u16(nvs_handle_t h, const char *k, uint16_t *v) { return nvsGetFixed(h, k, sizeof(uint16_t), v); }
esp_err_t nvs_get_i32(nvs_handle_t h, const char *k, int32_t *v) { return nvsGetFixed(h, k, sizeof(int32_t), v); }
esp_err_t nvs_get_float(nvs_handle_t h, const char *k, float *v) { return nvsGetFixed(h, k, sizeof(float), v); }
esp_err_t nvs_get_str(nvs_handle_t h, const char *k, char *buf, size_t *length) {
    auto *ns = nvsNs(h);
    if (!ns || k == nullptr || length == nullptr) return ESP_ERR_INVALID_ARG;
    auto it = ns->find(k);
    if (it == ns->end()) return ESP_ERR_NVS_NOT_FOUND;
    size_t need = it->second.size();  // includes trailing NUL
    if (buf == nullptr) { *length = need; return ESP_OK; }
    if (need > *length) { *length = need; return ESP_ERR_NVS_INVALID_LENGTH; }
    memcpy(buf, it->second.data(), need);
    *length = need;  // IDF: length includes the trailing NUL
    return ESP_OK;
}
}  // extern "C" — nvs

// ── uart (in-memory; declarations in driver/uart.h) ─────────────────────────
// Per-port RX/TX byte buffers. uart_read_bytes with a wait advances the fake
// clock (host_fake_clock_advance_us) instead of sleeping, so timeout behaviour
// is deterministic: a test that injects RX bytes first reads them immediately;
// an empty buffer consumes the timeout in fake time and returns 0.
#include "driver/uart.h"

namespace {
constexpr int kUartPorts = 3;
struct HostUartPort {
    bool installed = false;
    std::vector<uint8_t> rx;
    std::vector<uint8_t> tx;
};
HostUartPort g_hostUart[kUartPorts];

HostUartPort *hostUart(uart_port_t p) {
    return (p >= 0 && p < kUartPorts) ? &g_hostUart[p] : nullptr;
}
}

extern "C" {
esp_err_t uart_driver_install(uart_port_t uart_num, int rx_buffer_size,
                              int tx_buffer_size, int queue_size,
                              void* queue_handle, int flags) {
    (void)rx_buffer_size; (void)tx_buffer_size; (void)queue_size;
    (void)queue_handle; (void)flags;
    HostUartPort *p = hostUart(uart_num);
    if (!p) return ESP_ERR_INVALID_ARG;
    if (p->installed) return ESP_ERR_INVALID_STATE;  // like IDF
    p->installed = true;
    p->rx.clear();
    p->tx.clear();
    return ESP_OK;
}
esp_err_t uart_driver_delete(uart_port_t uart_num) {
    HostUartPort *p = hostUart(uart_num);
    if (!p) return ESP_ERR_INVALID_ARG;
    p->installed = false;
    p->rx.clear();
    p->tx.clear();
    return ESP_OK;
}
esp_err_t uart_param_config(uart_port_t uart_num, const uart_config_t* config) {
    (void)config;
    return hostUart(uart_num) ? ESP_OK : ESP_ERR_INVALID_ARG;
}
esp_err_t uart_set_pin(uart_port_t uart_num, int tx_pin, int rx_pin,
                       int rts_pin, int cts_pin) {
    (void)tx_pin; (void)rx_pin; (void)rts_pin; (void)cts_pin;
    return hostUart(uart_num) ? ESP_OK : ESP_ERR_INVALID_ARG;
}
esp_err_t uart_set_tx_idle_num(uart_port_t uart_num, uint16_t idle_num) {
    (void)idle_num;
    return hostUart(uart_num) ? ESP_OK : ESP_ERR_INVALID_ARG;
}
int uart_write_bytes(uart_port_t uart_num, const void* src, size_t size) {
    HostUartPort *p = hostUart(uart_num);
    if (!p || !p->installed || (size != 0 && src == nullptr)) return -1;
    const uint8_t* s = (const uint8_t*)src;
    p->tx.insert(p->tx.end(), s, s + size);
    return (int)size;
}
int uart_read_bytes(uart_port_t uart_num, void* buf, uint32_t length,
                    TickType_t ticks_to_wait) {
    HostUartPort *p = hostUart(uart_num);
    if (!p || !p->installed || (length != 0 && buf == nullptr)) return -1;
    if (p->rx.empty() && ticks_to_wait != 0 && ticks_to_wait != portMAX_DELAY) {
        // Consume the timeout in fake time (Arduino delay() semantics on host).
        uint32_t ms = ticks_to_wait > 60000 ? 60000 : (uint32_t)ticks_to_wait;
        host_fake_clock_advance_us(ms * 1000u);
    }
    size_t n = p->rx.size() < length ? p->rx.size() : length;
    if (n > 0) {
        memcpy(buf, p->rx.data(), n);
        p->rx.erase(p->rx.begin(), p->rx.begin() + (long)n);
    }
    return (int)n;
}
esp_err_t uart_get_buffered_data_len(uart_port_t uart_num, size_t* size) {
    HostUartPort *p = hostUart(uart_num);
    if (!p || size == nullptr) return ESP_ERR_INVALID_ARG;
    *size = p->rx.size();
    return ESP_OK;
}
esp_err_t uart_flush(uart_port_t uart_num) {
    HostUartPort *p = hostUart(uart_num);
    if (!p) return ESP_ERR_INVALID_ARG;
    p->tx.clear();
    return ESP_OK;
}
esp_err_t uart_wait_tx_done(uart_port_t uart_num, TickType_t ticks_to_wait) {
    (void)ticks_to_wait;  // TX is synchronous on host — always done
    return hostUart(uart_num) ? ESP_OK : ESP_ERR_INVALID_ARG;
}
}  // extern "C" — uart

// ── Test hooks (C++): drive the in-memory UART from the test TUs ────────────
// Inject bytes into a port's RX buffer (as if the peer had transmitted).
void host_uart_inject_rx(int port, const uint8_t* data, size_t len) {
    HostUartPort *p = hostUart((uart_port_t)port);
    if (!p || len == 0) return;
    p->rx.insert(p->rx.end(), data, data + len);
}
// Inject a C string into a port's RX buffer.
void host_uart_inject_rx_str(int port, const char* s) {
    if (!s) return;
    host_uart_inject_rx(port, (const uint8_t*)s, strlen(s));
}
// Take (and clear) everything a port has transmitted.
size_t host_uart_take_tx(int port, uint8_t* buf, size_t cap) {
    HostUartPort *p = hostUart((uart_port_t)port);
    if (!p) return 0;
    size_t n = p->tx.size() < cap ? p->tx.size() : cap;
    if (n > 0 && buf) memcpy(buf, p->tx.data(), n);
    p->tx.erase(p->tx.begin(), p->tx.begin() + (long)n);
    return n;
}
// Bytes a port has transmitted so far (does not clear).
size_t host_uart_tx_count(int port) {
    HostUartPort *p = hostUart((uart_port_t)port);
    return p ? p->tx.size() : 0;
}

// ── OTA (uc2_canopen CanOpenOTA) ─────────────────────────────────────────────
// Immediate success no-ops; a fake "next" partition so the OTA state machine
// can run on the host.
#include "esp_ota_ops.h"
extern "C" {

static const esp_partition_t s_hostOtaNextPartition = {"ota_1"};
static const esp_partition_t s_hostOtaRunningPartition = {"ota_0"};

const esp_partition_t* esp_ota_get_next_update_partition(const esp_partition_t* from) {
    (void)from;
    return &s_hostOtaNextPartition;
}
const esp_partition_t* esp_ota_get_running_partition(void) {
    return &s_hostOtaRunningPartition;
}
esp_err_t esp_ota_begin(const esp_partition_t* partition, uint32_t size, esp_ota_handle_t* handle) {
    (void)partition; (void)size;
    if (handle) *handle = 1;
    return ESP_OK;
}
esp_err_t esp_ota_write(esp_ota_handle_t handle, const void* data, size_t len) {
    (void)handle; (void)data; (void)len;
    return ESP_OK;
}
esp_err_t esp_ota_end(esp_ota_handle_t handle) { (void)handle; return ESP_OK; }
esp_err_t esp_ota_abort(esp_ota_handle_t handle) { (void)handle; return ESP_OK; }
esp_err_t esp_ota_set_boot_partition(const esp_partition_t* partition) {
    (void)partition;
    return ESP_OK;
}
esp_ota_img_state_t esp_ota_get_state_partition(const esp_partition_t* partition) {
    (void)partition;
    return ESP_OTA_IMG_VALID;
}

} // extern "C" — OTA

// ── CanOpenOTA (host: no-op receiver registration) ──────────────────────────
// The real CanOpenOTA.cpp (OD 0x2F00 SDO domain → esp_ota_write) is not part
// of the host link; CANopenModule.cpp still calls the registration once after
// CO_CANopenInit(). This no-op keeps that call resolvable. Must NOT be
// compiled in a build that also links CanOpenOTA.cpp (duplicate symbol).
#include "CanOpenOTA.h"
namespace CanOpenOTA {
void registerOdExtensions() {}
}  // namespace CanOpenOTA

// ── esp_adc oneshot (host: no-op driver) ────────────────────────────────────
// uc2_adc.cpp (the real first-party C-linkage analogRead/analogReadMilliVolts/
// ... façade, T2.3) links against these. Unit creation and pin→channel mapping
// always succeed so the lazy-init paths exercise without hardware; reads
// return 0 and calibration is "not supported" (naive-scale fallback).
extern "C" {

esp_err_t adc_oneshot_new_unit(const adc_oneshot_unit_init_cfg_t *init_cfg,
                               adc_oneshot_unit_handle_t *ret_handle)
{
    (void)init_cfg;
    static adc_oneshot_unit_handle_t dummy = (adc_oneshot_unit_handle_t)1;
    *ret_handle = dummy;
    return ESP_OK;
}

esp_err_t adc_oneshot_del_unit(adc_oneshot_unit_handle_t handle)
{
    (void)handle;
    return ESP_OK;
}

esp_err_t adc_oneshot_config_channel(adc_oneshot_unit_handle_t handle,
                                     adc_channel_t channel,
                                     const adc_oneshot_chan_cfg_t *config)
{
    (void)handle; (void)channel; (void)config;
    return ESP_OK;
}

esp_err_t adc_oneshot_read(adc_oneshot_unit_handle_t handle,
                           adc_channel_t channel, int *value)
{
    (void)handle; (void)channel;
    *value = 0;
    return ESP_OK;
}

esp_err_t adc_oneshot_io_to_channel(int gpio_number, adc_unit_t *unit,
                                    adc_channel_t *channel)
{
    (void)gpio_number;
    *unit = ADC_UNIT_1;
    *channel = ADC_CHANNEL_0;
    return ESP_OK;
}

esp_err_t adc_cali_create_scheme_curve_fitting(
    const adc_cali_curve_fitting_config_t *config, adc_cali_handle_t *out_handle)
{
    (void)config;
    *out_handle = nullptr;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t adc_cali_raw_to_voltage(adc_cali_handle_t handle, int raw, int *mv)
{
    (void)handle; (void)raw; (void)mv;
    return ESP_ERR_NOT_SUPPORTED;
}

esp_err_t adc_cali_delete_scheme(adc_cali_handle_t handle)
{
    (void)handle;
    return ESP_OK;
}

} // extern "C" — esp_adc
