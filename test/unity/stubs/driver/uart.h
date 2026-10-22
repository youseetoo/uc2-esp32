// Host stub: driver/uart.h — classic IDF UART API subset used by
// uc2_hal/src/uc2_serial.cpp (+ apps): install/delete, param_config,
// set_pin, write/read bytes, buffered len, flush, wait_tx_done,
// set_tx_idle_num. In-memory RX/TX per port; definitions: host_impl.cpp.
// Test hooks (C++): host_uart_inject_rx / host_uart_take_tx in host_impl.cpp.
#pragma once
#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"
#include "freertos/FreeRTOS.h"  // TickType_t

typedef int uart_port_t;
#define UART_NUM_0 0
#define UART_NUM_1 1
#define UART_NUM_2 2
#define UART_PIN_NO_CHANGE (-1)

typedef enum {
    UART_DATA_5_BITS = 0,
    UART_DATA_6_BITS,
    UART_DATA_7_BITS,
    UART_DATA_8_BITS
} uart_word_length_t;

typedef enum {
    UART_PARITY_DISABLE = 0,
    UART_PARITY_EVEN,
    UART_PARITY_ODD
} uart_parity_t;

typedef enum {
    UART_STOP_BITS_1 = 1,
    UART_STOP_BITS_1_5,
    UART_STOP_BITS_2
} uart_stop_bits_t;

typedef enum {
    UART_HW_FLOWCTRL_DISABLE = 0,
    UART_HW_FLOWCTRL_RTS,
    UART_HW_FLOWCTRL_CTS,
    UART_HW_FLOWCTRL_DTR
} uart_hw_flowcontrol_t;

typedef enum {
    UART_SCLK_DEFAULT = 0,
    UART_SCLK_APB,
    UART_SCLK_RXDTICK0
} uart_sclk_t;

typedef struct {
    int baud_rate;
    uart_word_length_t data_bits;
    uart_parity_t parity;
    uart_stop_bits_t stop_bits;
    uart_hw_flowcontrol_t flow_ctrl;
    uint16_t rx_flow_ctrl_thresh;
    uart_sclk_t source_clk;
} uart_config_t;

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t uart_driver_install(uart_port_t uart_num, int rx_buffer_size,
                              int tx_buffer_size, int queue_size,
                              void* queue_handle, int flags);
esp_err_t uart_driver_delete(uart_port_t uart_num);
esp_err_t uart_param_config(uart_port_t uart_num, const uart_config_t* config);
esp_err_t uart_set_pin(uart_port_t uart_num, int tx_pin, int rx_pin,
                       int rts_pin, int cts_pin);
esp_err_t uart_set_tx_idle_num(uart_port_t uart_num, uint16_t idle_num);
int uart_write_bytes(uart_port_t uart_num, const void* src, size_t size);
int uart_read_bytes(uart_port_t uart_num, void* buf, uint32_t length,
                    TickType_t ticks_to_wait);
esp_err_t uart_get_buffered_data_len(uart_port_t uart_num, size_t* size);
esp_err_t uart_flush(uart_port_t uart_num);
esp_err_t uart_wait_tx_done(uart_port_t uart_num, TickType_t ticks_to_wait);

#ifdef __cplusplus
}
#endif
