// Host stub: driver/gpio.h
#pragma once
#include <cstdint>
#include "esp_err.h"

typedef int32_t gpio_num_t;

#define GPIO_NUM_0  0
#define GPIO_NUM_1  1
#define GPIO_NUM_2  2
#define GPIO_NUM_3  3
#define GPIO_NUM_4  4
#define GPIO_NUM_5  5
#define GPIO_NUM_6  6
#define GPIO_NUM_7  7
#define GPIO_NUM_8  8
#define GPIO_NUM_9  9
#define GPIO_NUM_10 10
#define GPIO_NUM_11 11
#define GPIO_NUM_12 12
#define GPIO_NUM_13 13
#define GPIO_NUM_14 14
#define GPIO_NUM_15 15
#define GPIO_NUM_16 16
#define GPIO_NUM_17 17
#define GPIO_NUM_18 18
#define GPIO_NUM_19 19
#define GPIO_NUM_20 20
#define GPIO_NUM_21 21
#define GPIO_NUM_22 22
#define GPIO_NUM_23 23
#define GPIO_NUM_25 25
#define GPIO_NUM_26 26
#define GPIO_NUM_27 27
#define GPIO_NUM_28 28
#define GPIO_NUM_29 29
#define GPIO_NUM_30 30
#define GPIO_NUM_32 32
#define GPIO_NUM_33 33
#define GPIO_NUM_34 34
#define GPIO_NUM_35 35
#define GPIO_NUM_36 36
#define GPIO_NUM_37 37
#define GPIO_NUM_38 38
#define GPIO_NUM_39 39
#define GPIO_NUM_40 40
#define GPIO_NUM_41 41
#define GPIO_NUM_42 42
#define GPIO_NUM_43 43
#define GPIO_NUM_44 44
#define GPIO_NUM_45 45
#define GPIO_NUM_46 46
#define GPIO_NUM_47 47
#define GPIO_NUM_48 48
#define GPIO_NUM_NC -1
#define GPIO_MAX_NUM 49

typedef enum {
    GPIO_MODE_INPUT = 0x1,
    GPIO_MODE_OUTPUT = 0x2,
    GPIO_MODE_OUTPUT_OD = 0x4,
    GPIO_MODE_DISABLE = 0x0
} gpio_mode_t;

typedef enum {
    GPIO_PULLUP_DISABLE = 0,
    GPIO_PULLUP_ENABLE = 1,
    GPIO_PULLDOWN_DISABLE = 2,
    GPIO_PULLDOWN_ENABLE = 3
} gpio_pull_mode_t;

typedef enum {
    GPIO_INTR_DISABLE = 0,
    GPIO_INTR_POSEDGE = 1,
    GPIO_INTR_NEGEDGE = 2,
    GPIO_INTR_ANYEDGE = 3,
    GPIO_INTR_LOW_LEVEL = 4,
    GPIO_INTR_HIGH_LEVEL = 5
} gpio_intr_type_t;

typedef enum {
    GPIO_DRIVE_CAP_0 = 0,
    GPIO_DRIVE_CAP_1,
    GPIO_DRIVE_CAP_2,
    GPIO_DRIVE_CAP_3,
    GPIO_DRIVE_CAP_4,
    GPIO_DRIVE_CAP_5,
    GPIO_DRIVE_CAP_6,
    GPIO_DRIVE_CAP_7,
    GPIO_DRIVE_CAP_8
} gpio_drive_capability_t;

typedef struct {
    uint32_t pin_bit_mask;
    gpio_mode_t mode;
    gpio_pull_mode_t pull_up_en;
    gpio_pull_mode_t pull_down_en;
    gpio_intr_type_t intr_type;
} gpio_config_t;

typedef uint32_t gpio_interrupt_type_t;

// uc2's legacy edge-int types (InterruptController).
typedef enum {
    GPIO_INT_TYPE_POSTEDGE,
    GPIO_INT_TYPE_POSTEDGE_FILTERED,
    GPIO_INT_TYPE_POSTEDGE_NF,
    GPIO_INT_TYPE_POSTEDGE_NF_FILTERED
} gpio_int_type_t;

typedef void (*gpio_isr_t)(void* arg);
typedef void (*gpio_intr_fn_t)(void* arg);

esp_err_t gpio_install_isr_service(int intr_alloc_flags);
esp_err_t gpio_isr_handler_add(gpio_num_t gpio_num, gpio_isr_t isr_handler, void* args);
esp_err_t gpio_reconfigure(const gpio_config_t* p_gpio_config);
esp_err_t gpio_set_direction(gpio_num_t gpio_num, gpio_mode_t mode);
esp_err_t gpio_set_level(gpio_num_t gpio_num, uint32_t level);
esp_err_t gpio_get_level(gpio_num_t gpio_num, int32_t* level);
esp_err_t gpio_set_pull_mode(gpio_num_t gpio_num, gpio_pull_mode_t pull);
esp_err_t gpio_set_intr_type(gpio_num_t gpio_num, gpio_interrupt_type_t type);
esp_err_t gpio_pad_select_gpio(gpio_num_t gpio_num);
esp_err_t gpio_deep_sleep_hold_dis(void);
esp_err_t gpio_deep_sleep_hold_en(void);
esp_err_t gpio_iset_gpio(gpio_num_t gpio_num, uint8_t val);
esp_err_t gpio_set_wait_delay(gpio_num_t gpio_num, uint32_t wait_delay);
void gpio_set_edge_int(gpio_num_t gpio_num, gpio_int_type_t type);
esp_err_t gpio_matrix_in(gpio_num_t gpio_num, uint32_t sig, bool invert);
esp_err_t gpio_matrix_out(gpio_num_t gpio_num, uint32_t sig, bool open_drain, bool invert);

// Legacy intr API
typedef void (*intr_handler_t)(void*);
esp_err_t intr_attach(const char* name, intr_handler_t handler, void* arg);
