// Host stub: driver/ledc.h
//
// IDF 6.1 LEDC API surface as used by uc2_hal/uc2_ledc.h (the only consumer
// in the host build). Functions are static inline no-ops; if a test later
// needs to observe duty writes, give these real state (mirroring the
// g_hostLedcDuty pattern in host_impl.cpp).
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

typedef enum {
    LEDC_LOW_SPEED_MODE = 0,
    LEDC_HIGH_SPEED_MODE = 1,
} ledc_speed_mode_t;

typedef uint8_t ledc_timer_t;
typedef uint8_t ledc_channel_t;
typedef uint8_t ledc_timer_bit_t;

// Named constants (IDF 6.1 values). #defines because the typedefs above are
// plain uint8_t; BuzzerController.c uses LEDC_TIMER_3 / LEDC_CHANNEL_7 /
// LEDC_TIMER_8_BIT.
#define LEDC_TIMER_0 0
#define LEDC_TIMER_1 1
#define LEDC_TIMER_2 2
#define LEDC_TIMER_3 3
#define LEDC_TIMER_4 4
#define LEDC_TIMER_5 5
#define LEDC_TIMER_6 6
#define LEDC_TIMER_7 7
#define LEDC_CHANNEL_0 0
#define LEDC_CHANNEL_1 1
#define LEDC_CHANNEL_2 2
#define LEDC_CHANNEL_3 3
#define LEDC_CHANNEL_4 4
#define LEDC_CHANNEL_5 5
#define LEDC_CHANNEL_6 6
#define LEDC_CHANNEL_7 7
#define LEDC_CHANNEL_8 8
#define LEDC_CHANNEL_9 9
#define LEDC_CHANNEL_10 10
#define LEDC_CHANNEL_11 11
#define LEDC_CHANNEL_12 12
#define LEDC_CHANNEL_13 13
#define LEDC_CHANNEL_14 14
#define LEDC_CHANNEL_15 15
#define LEDC_TIMER_1_BIT 1
#define LEDC_TIMER_2_BIT 2
#define LEDC_TIMER_4_BIT 4
#define LEDC_TIMER_5_BIT 5
#define LEDC_TIMER_8_BIT 8
#define LEDC_TIMER_10_BIT 10
#define LEDC_TIMER_12_BIT 12
#define LEDC_TIMER_14_BIT 14
#define LEDC_TIMER_15_BIT 15
#define LEDC_TIMER_16_BIT 16

typedef enum {
    LEDC_AUTO_CLK = 0,
} ledc_clk_cfg_t;

typedef enum {
    LEDC_SLEEP_MODE_NO_ALIVE_NO_PD = 0,
} ledc_sleep_mode_t;

typedef struct {
    ledc_speed_mode_t speed_mode;
    ledc_timer_t timer_num;
    ledc_timer_bit_t duty_resolution;
    uint32_t freq_hz;
    ledc_clk_cfg_t clk_cfg;
    bool deconfigure;
} ledc_timer_config_t;

typedef struct {
    int32_t gpio_num;
    ledc_speed_mode_t speed_mode;
    ledc_timer_t timer_sel;
    ledc_channel_t channel;
    uint32_t duty;
    uint32_t hpoint;
    ledc_sleep_mode_t sleep_mode;
    struct {
        uint32_t output_invert : 1;
    } flags;
    bool deconfigure;
} ledc_channel_config_t;

static inline esp_err_t ledc_timer_config(const ledc_timer_config_t* cfg) { (void)cfg; return ESP_OK; }
static inline esp_err_t ledc_channel_config(const ledc_channel_config_t* cfg) { (void)cfg; return ESP_OK; }
static inline esp_err_t ledc_set_duty(ledc_speed_mode_t mode, ledc_channel_t ch, uint32_t duty) {
    (void)mode; (void)ch; (void)duty;
    return ESP_OK;
}
static inline esp_err_t ledc_update_duty(ledc_speed_mode_t mode, ledc_channel_t ch) {
    (void)mode; (void)ch;
    return ESP_OK;
}
static inline uint32_t ledc_get_duty(ledc_speed_mode_t mode, ledc_channel_t ch) {
    (void)mode; (void)ch;
    return 0;
}
static inline esp_err_t ledc_set_duty_and_update(ledc_speed_mode_t mode, ledc_channel_t ch,
                                                 uint32_t duty, ledc_timer_t timer) {
    (void)mode; (void)ch; (void)duty; (void)timer;
    return ESP_OK;
}
static inline esp_err_t ledc_set_freq(ledc_speed_mode_t mode, ledc_timer_t timer,
                                      uint32_t freq_hz) {
    (void)mode; (void)timer; (void)freq_hz;
    return ESP_OK;
}
