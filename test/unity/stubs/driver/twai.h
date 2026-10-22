// Host stub: driver/twai.h — IDF 6.1 TWAI driver API surface (no-op on host).
// The uc2_canopen/CANopenModule.cpp and canopen_node/CO_driver_ESP32.c builds
// against the 6.1 API: twai_driver_install(general, timing, filter),
// twai_start() with no handle, twai_receive(msg, timeout), alert-based loop.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"
#include "hal/twai_types.h"
#include "hal/gpio_types.h"

#define TWAI_IO_UNUSED (-1)
#define TWAI_MAX_CONTROLLER_NUM 4

typedef enum {
    TWAI_MODE_NORMAL = 0,
    TWAI_MODE_LOOPBACK_SELF = 1,
    TWAI_MODE_LOOPBACK_LOCAL = 2,
    TWAI_MODE_SILENT = 3,
    TWAI_MODE_SINGLE_WIRE = 4
} twai_mode_t;

#define TWAI_ALERT_NONE            0u
#define TWAI_ALERT_RX_DATA         (1u << 0)
#define TWAI_ALERT_TX_COMPLETE     (1u << 1)
#define TWAI_ALERT_BUS_ERROR       (1u << 2)
#define TWAI_ALERT_TX_ABORT        (1u << 3)
#define TWAI_ALERT_RX_OVERFLOW     (1u << 4)
#define TWAI_ALERT_TX_OVERFLOW     (1u << 5)
#define TWAI_ALERT_SELF_RECEPTION  (1u << 6)
#define TWAI_ALERT_BUS_OFF         (1u << 7)
#define TWAI_ALERT_DEGRADED        (1u << 8)
#define TWAI_ALERT_BUS_RECOVERED   (1u << 9)
#define TWAI_ALERT_ALL             0xFFFFFFFFu

// Interrupt flags (real IDF: esp_intr.h) — only what TWAI config uses.
#ifndef ESP_INTR_FLAG_LEVEL1
#define ESP_INTR_FLAG_LEVEL1 0
#define ESP_INTR_FLAG_LEVEL2 1
#define ESP_INTR_FLAG_EDGE_POSITIVE 2
#endif

#define TWAI_CLK_SRC_DEFAULT 0

typedef struct {
    uint32_t clk_src;
    uint32_t bus_speed;
    uint32_t bit_timing;
} twai_timing_config_t;

#define _TWAI_TIMING(src, speed) { .clk_src = (src), .bus_speed = (speed), .bit_timing = 0 }
#define TWAI_TIMING_CONFIG_DEFAULT()      _TWAI_TIMING(TWAI_CLK_SRC_DEFAULT, 500000)
#define TWAI_TIMING_CONFIG_100KBITS()     _TWAI_TIMING(TWAI_CLK_SRC_DEFAULT, 100000)
#define TWAI_TIMING_CONFIG_125KBITS()     _TWAI_TIMING(TWAI_CLK_SRC_DEFAULT, 125000)
#define TWAI_TIMING_CONFIG_250KBITS()     _TWAI_TIMING(TWAI_CLK_SRC_DEFAULT, 250000)
#define TWAI_TIMING_CONFIG_500KBITS()     _TWAI_TIMING(TWAI_CLK_SRC_DEFAULT, 500000)
#define TWAI_TIMING_CONFIG_1MBITS()       _TWAI_TIMING(TWAI_CLK_SRC_DEFAULT, 1000000)
#define TWAI_TIMING_CONFIG_2MBITS()       _TWAI_TIMING(TWAI_CLK_SRC_DEFAULT, 2000000)
#define TWAI_TIMING_CONFIG_5MBITS()       _TWAI_TIMING(TWAI_CLK_SRC_DEFAULT, 5000000)
#define TWAI_TIMING_CONFIG_10MBITS()      _TWAI_TIMING(TWAI_CLK_SRC_DEFAULT, 10000000)

typedef struct {
    uint32_t id;
    uint32_t id_mask;
} twai_filter_config_t;

#define TWAI_FILTER_CONFIG_ACCEPT_ALL() { .id = 0, .id_mask = 0 }

// 6.1: trailing general_flags sub-struct (CANopenModule.cpp inits it {0}).
typedef struct {
    uint32_t flags;
} twai_general_flags_t;

typedef struct {
    uint8_t controller_id;
    twai_mode_t mode;
    gpio_num_t tx_io;
    gpio_num_t rx_io;
    gpio_num_t clkout_io;
    gpio_num_t bus_off_io;
    uint32_t tx_queue_len;
    uint32_t rx_queue_len;
    uint32_t alerts_enabled;
    uint32_t clkout_divider;
    uint32_t intr_flags;
    twai_general_flags_t general_flags;
} twai_general_config_t;

typedef enum {
    TWAI_STATE_STOPPED = 0,
    TWAI_STATE_RUNNING = 1,
    TWAI_STATE_BUS_OFF = 2,
    TWAI_STATE_RECOVERING = 3
} twai_state_t;

typedef struct {
    twai_state_t state;
    uint32_t msgs_to_tx;
    uint32_t tx_error_counter;
    uint32_t rx_error_counter;
    uint32_t tx_failed_count;
    uint32_t bus_error_count;
} twai_status_info_t;

esp_err_t twai_driver_install(const twai_general_config_t* general_conf,
                              const twai_timing_config_t* timing_conf,
                              const twai_filter_config_t* filter_conf);
esp_err_t twai_driver_uninstall(void);
esp_err_t twai_start(void);
esp_err_t twai_stop(void);
esp_err_t twai_transmit(const twai_message_t* message, uint32_t timeout_ms);
esp_err_t twai_receive(twai_message_t* message, uint32_t timeout_ms);
esp_err_t twai_reconfigure_alerts(uint32_t alerts, void* user_data);
esp_err_t twai_initiate_recovery(void);
esp_err_t twai_read_alerts(uint32_t* out_alerts, uint32_t timeout_ms);
esp_err_t twai_get_status_info(twai_status_info_t* status);
esp_err_t twai_clear_error_counters(void);
const char* twai_status_text(esp_err_t status_code);
