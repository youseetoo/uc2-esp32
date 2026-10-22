#pragma once
// Host stub: driver/i2c_master.h — the modern (v2) I2C master driver surface
// used by uc2_wire.h (class-based façade). Declarations only: the host build
// does not compile uc2_wire.cpp, so no definitions are needed here.
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"  // gpio_num_t
#include "esp_err.h"

typedef enum {
    I2C_NUM_0 = 0,
    I2C_NUM_1,
    I2C_NUM_MAX,
} i2c_port_t;

typedef enum {
    I2C_CLK_SRC_DEFAULT = 0,
    I2C_CLK_SRC_DEFAULT_PRESET,
    I2C_CLK_SRC_RTC_FAST,
    I2C_CLK_SRC_RTC_SLOW,
    I2C_CLK_SRC_MAX,
} i2c_clock_source_t;

#define I2C_ADDR_BIT_LEN_7 (7)
#define I2C_ADDR_BIT_LEN_10 (10)

typedef void *i2c_master_bus_handle_t;
typedef void *i2c_master_dev_handle_t;

typedef struct {
    i2c_port_t i2c_port;
    int sda_io_num;
    int scl_io_num;
    int clk_source;
    uint32_t glitch_ignore_cnt;
    uint32_t intr_priority;
    bool strict_timing;
    uint32_t trans_queue_depth;
    uint32_t i2c_flags;
    struct {
        unsigned int enable_internal_pullup : 1;
        unsigned int apband_clamp_en : 1;
    } flags;
} i2c_master_bus_config_t;

typedef enum {
    I2C_DEVICE_TYPE_STD = 0,
    I2C_DEVICE_TYPE_FAST,
    I2C_DEVICE_TYPE_FAST_PLUS,
    I2C_DEVICE_TYPE_MAX,
} i2c_device_type_t;

typedef struct {
    uint16_t dev_addr_length;
    uint16_t device_address;
    uint32_t scl_speed_hz;
    i2c_device_type_t device_type;
} i2c_device_config_t;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t i2c_new_master_bus(const i2c_master_bus_config_t *bus_config,
                             i2c_master_bus_handle_t *ret_bus_handle);
esp_err_t i2c_master_bus_add_device(i2c_master_bus_handle_t bus_handle,
                                    const i2c_device_config_t *dev_config,
                                    i2c_master_dev_handle_t *ret_handle);
esp_err_t i2c_del_master_bus(i2c_master_bus_handle_t bus_handle);

esp_err_t i2c_master_transmit(i2c_master_dev_handle_t dev_handle,
                              const uint8_t *write_buffer, size_t write_len,
                              int32_t ticks_to_wait);
esp_err_t i2c_master_receive(i2c_master_dev_handle_t dev_handle,
                             uint8_t *read_buffer, size_t read_len,
                             int32_t ticks_to_wait);
esp_err_t i2c_master_transmit_receive(i2c_master_dev_handle_t dev_handle,
                                      const uint8_t *write_buffer,
                                      size_t write_len, uint8_t *read_buffer,
                                      size_t read_len, int32_t ticks_to_wait);

#ifdef __cplusplus
}
#endif
