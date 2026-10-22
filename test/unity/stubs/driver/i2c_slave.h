#pragma once
// Host stub: driver/i2c_slave.h — the modern (v2) I2C slave driver surface
// used by uc2_wire.h (M5 Dial build). Declarations only (uc2_wire.cpp is
// not part of the host build).
#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#include "driver/gpio.h"    // gpio_num_t
#include "driver/i2c_master.h"  // i2c_port_t, i2c_clock_source_t
#include "esp_err.h"

typedef void *i2c_slave_dev_handle_t;

typedef struct {
    i2c_port_t i2c_port;
    int sda_io_num;
    int scl_io_num;
    int clk_source;
    uint32_t send_buf_depth;
    uint32_t receive_buf_depth;
    uint32_t intr_priority;
    uint32_t slave_addr;
    uint8_t addr_bit_len;
    uint32_t i2c_flags;
    struct {
        unsigned int enable_internal_pullup : 1;
        unsigned int disable_ack_response : 1;
        unsigned int ack_all_data : 1;
    } flags;
} i2c_slave_config_t;

typedef struct {
    uint32_t trans_id;
    uint8_t *buffer;
    uint32_t length;
} i2c_slave_rx_done_event_data_t;

typedef struct {
    bool is_read;
    uint32_t trans_id;
    uint32_t rx_len;
    uint32_t tx_len;
} i2c_slave_request_event_data_t;

typedef struct {
    bool (*on_receive)(i2c_slave_dev_handle_t dev_handle,
                       const i2c_slave_rx_done_event_data_t *data, void *arg);
    bool (*on_request)(i2c_slave_dev_handle_t dev_handle,
                       const i2c_slave_request_event_data_t *data, void *arg);
    bool (*on_ack)(i2c_slave_dev_handle_t dev_handle, uint32_t trans_id,
                   void *arg);
    bool (*on_nack)(i2c_slave_dev_handle_t dev_handle, uint32_t trans_id,
                    void *arg);
    bool (*on_reset)(i2c_slave_dev_handle_t dev_handle, void *arg);
} i2c_slave_event_callbacks_t;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t i2c_new_slave_device(const i2c_slave_config_t *slave_config,
                               i2c_slave_dev_handle_t *ret_slave_handle);
esp_err_t i2c_del_slave_device(i2c_slave_dev_handle_t dev_handle);
esp_err_t i2c_slave_register_event_callbacks(i2c_slave_dev_handle_t dev_handle,
                                             i2c_slave_event_callbacks_t *callbacks,
                                             void *callback_arg);
esp_err_t i2c_slave_write(i2c_slave_dev_handle_t dev_handle, const uint8_t *data,
                          size_t len, size_t *bytes_transferred, uint32_t ticks_to_wait);

#ifdef __cplusplus
}
#endif
