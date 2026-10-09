// Host stub: driver/twai.h
#pragma once
#include <cstdint>
#include "esp_err.h"

typedef struct twai_driver* twai_driver_handle_t;
typedef uint32_t twai_timestamp_t;

#define TWAI_DEFAULT_TIMEOUT 0

typedef enum {
    TWAI_DATA_LENGTH_8,
    TWAI_DATA_LENGTH_0,
    TWAI_DATA_LENGTH_1,
    TWAI_DATA_LENGTH_2,
    TWAI_DATA_LENGTH_3,
    TWAI_DATA_LENGTH_4,
    TWAI_DATA_LENGTH_5,
    TWAI_DATA_LENGTH_6,
    TWAI_DATA_LENGTH_7
} twai_data_length_code_t;

typedef struct {
    uint8_t data[8];
    twai_data_length_code_t data_length_code;
} twai_data_t;

typedef struct {
    uint32_t identifier;
    bool extended_rec;
    bool rtr;
    twai_data_t data;
} twai_message_t;

#define TWAI_CLK_SRC_DEFAULT 0
#define TWAI_CLK_SRC_XTAL    0x00000000
#define TWAI_CLK_SRC_RC      0x00000001
#define TWAI_CLK_SRC_PLL_F120M 0x00000002
#define TWAI_CLK_SRC_PLL_F80M  0x00000004
#define TWAI_CLK_SRC_APB     0x00000008

typedef struct twai_timing_config {
    uint32_t clk_src;
    uint32_t bus_speed;
    uint32_t bit_timing;
} twai_timing_config_t;

#define _TWAI_TIMING(src, speed) { .clk_src = (src), .bus_speed = (speed), .bit_timing = 0 }

#define TWAI_TIMING_CONFIG_DEFAULT() _TWAI_TIMING(TWAI_CLK_SRC_DEFAULT, 500000)
#define TWAI_TIMING_CONFIG_100KBPS()   _TWAI_TIMING(TWAI_CLK_SRC_DEFAULT, 100000)
#define TWAI_TIMING_CONFIG_125KBPS()   _TWAI_TIMING(TWAI_CLK_SRC_DEFAULT, 125000)
#define TWAI_TIMING_CONFIG_250KBPS()   _TWAI_TIMING(TWAI_CLK_SRC_DEFAULT, 250000)
#define TWAI_TIMING_CONFIG_500KBPS()   _TWAI_TIMING(TWAI_CLK_SRC_DEFAULT, 500000)
#define TWAI_TIMING_CONFIG_1MBPS()     _TWAI_TIMING(TWAI_CLK_SRC_DEFAULT, 1000000)
#define TWAI_TIMING_CONFIG_2MBPS()     _TWAI_TIMING(TWAI_CLK_SRC_DEFAULT, 2000000)
#define TWAI_TIMING_CONFIG_5MBPS()     _TWAI_TIMING(TWAI_CLK_SRC_DEFAULT, 5000000)
#define TWAI_TIMING_CONFIG_10MBPS()    _TWAI_TIMING(TWAI_CLK_SRC_DEFAULT, 10000000)
#define TWAI_TIMING_CONFIG_250KBPS_40MHZ()  _TWAI_TIMING(TWAI_CLK_SRC_APB, 250000)
#define TWAI_TIMING_CONFIG_500KBPS_40MHZ()  _TWAI_TIMING(TWAI_CLK_SRC_APB, 500000)
#define TWAI_TIMING_CONFIG_1MBPS_40MHZ()    _TWAI_TIMING(TWAI_CLK_SRC_APB, 1000000)
#define TWAI_TIMING_CONFIG_250KBPS_80MHZ()  _TWAI_TIMING(TWAI_CLK_SRC_PLL_F80M, 250000)
#define TWAI_TIMING_CONFIG_500KBPS_80MHZ()  _TWAI_TIMING(TWAI_CLK_SRC_PLL_F80M, 500000)
#define TWAI_TIMING_CONFIG_1MBPS_80MHZ()    _TWAI_TIMING(TWAI_CLK_SRC_PLL_F80M, 1000000)
#define TWAI_TIMING_CONFIG_2MBPS_80MHZ()    _TWAI_TIMING(TWAI_CLK_SRC_PLL_F80M, 2000000)
#define TWAI_TIMING_CONFIG_250KBPS_160MHZ() _TWAI_TIMING(TWAI_CLK_SRC_PLL_F120M, 250000)
#define TWAI_TIMING_CONFIG_500KBPS_160MHZ() _TWAI_TIMING(TWAI_CLK_SRC_PLL_F120M, 500000)
#define TWAI_TIMING_CONFIG_1MBPS_160MHZ()   _TWAI_TIMING(TWAI_CLK_SRC_PLL_F120M, 1000000)
#define TWAI_TIMING_CONFIG_2MBPS_160MHZ()   _TWAI_TIMING(TWAI_CLK_SRC_PLL_F120M, 2000000)
#define TWAI_TIMING_CONFIG_5MBPS_160MHZ()   _TWAI_TIMING(TWAI_CLK_SRC_PLL_F120M, 5000000)
#define TWAI_TIMING_CONFIG_10MBPS_160MHZ()  _TWAI_TIMING(TWAI_CLK_SRC_PLL_F120M, 10000000)

typedef struct {
    uint32_t clk_src;
    uint32_t bus_speed;
    uint32_t bit_timing;
    uint32_t self_reception_enabled;
    uint32_t self_reception_echo_buffered;
    uint32_t rx_fifo_depth;
    uint32_t tx_queue_depth;
    uint32_t alarm_callback_depth;
    uint32_t alerts_enabled;
    int32_t pins_rx;
    int32_t pins_tx;
    int32_t pins_int;
    twai_timing_config_t* timing_config;
    void* callback;
    void* user_data;
} twai_config_t;

typedef struct {
    uint32_t bus_error_count;
    uint32_t tx_error_count;
    uint32_t rx_error_count;
    uint32_t last_error;
    uint32_t tx_queue_head;
    uint32_t tx_queue_size;
    uint32_t tx_queue_count;
    uint32_t tx_fifo_count;
    uint32_t tx_fifo_size;
    uint32_t rx_fifo_head;
    uint32_t rx_fifo_size;
    uint32_t rx_fifo_count;
} twai_status_info_t;

esp_err_t twai_driver_install(const twai_config_t* config, twai_driver_handle_t* driver);
esp_err_t twai_driver_uninstall(twai_driver_handle_t driver);
esp_err_t twai_start(twai_driver_handle_t driver);
esp_err_t twai_stop(twai_driver_handle_t driver);
esp_err_t twai_transmit(twai_driver_handle_t driver, const twai_message_t* message, uint32_t timeout_ms);
esp_err_t twai_receive(twai_driver_handle_t driver, twai_message_t* message, uint32_t timeout_ms);
esp_err_t twai_get_status_info(twai_driver_handle_t driver, twai_status_info_t* status);
esp_err_t twai_clear_error_counters(twai_driver_handle_t driver);
const char* twai_status_text(esp_err_t status_code);
esp_err_t twai_get_timing_config_for_baudrate(uint32_t baudrate, uint32_t clk_src_hz,
                                              uint32_t clk_div, uint8_t sample_point,
                                              uint32_t* timing_config);
