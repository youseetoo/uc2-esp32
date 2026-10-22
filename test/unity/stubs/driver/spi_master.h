// Host stub: driver/spi_master.h — classic SPI bus/device API surface used by
// dac_mcp4822.c (galvo DAC). No-op on the host.
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    SPI1_HOST = 0,
    SPI2_HOST = 1,
    SPI3_HOST = 2
} spi_host_device_t;

typedef enum {
    SPI_DMA_CH_AUTO = 0,
    SPI_DMA_CH0 = 0,
    SPI_DMA_CH1 = 1,
    SPI_DMA_CH2 = 2
} spi_dma_chan_t;

#define SPI_DEVICE_NO_DUMMY (1 << 0)
#define SPI_DEVICE_NO_DUMMY_BIT 0
#define SPI_DEVICE_HALFDUPLEX (1 << 1)
#define SPI_TRANS_USE_TXDATA (1u << 16)

typedef struct {
    int sclk_io_num;
    int mosi_io_num;
    int miso_io_num;
    int qp_io_num;
    int qd_io_num;
    int quadwp_io_num;
    int quadhd_io_num;
    int cs0_io_num;
    int data4_io_num;
    int data5_io_num;
    int data6_io_num;
    int data7_io_num;
    uint32_t max_transfer_sz;
} spi_bus_config_t;

typedef struct {
    int command;
    int address;
    uint32_t trans_flags;
    uint32_t flags;
    uint16_t rxlength;
    uint16_t rxlength7;
    uint16_t rxlength8;
    uint16_t rxlength9;
    uint16_t rxlength10;
    uint16_t rxlength11;
    uint16_t rxlength12;
    uint8_t bits_per_word;
    uint32_t pre_delays;
    uint8_t pre_cb_delays;
    uint32_t delays;
    uint8_t cb_delays;
    uint32_t cs_setup_us;
    uint32_t cs_hold_us;
    uint8_t *rx_buffer;
    uint8_t *tx_buffer;
    void *user;
    size_t length;
    uint8_t tx_data[64];
} spi_transaction_t;

typedef void *spi_device_handle_t;

typedef struct {
    uint32_t clock_speed_hz;
    int mode;
    int spics_io_num;
    int queue_size;
    int command_bits;
    int address_bits;
    uint32_t flags;
    void *pre_cb;
    void *post_cb;
} spi_device_interface_config_t;

esp_err_t spi_bus_initialize(spi_host_device_t host, const spi_bus_config_t* bus_config,
                             spi_dma_chan_t dma_chan);
esp_err_t spi_bus_free(spi_host_device_t host);
esp_err_t spi_bus_add_device(spi_host_device_t host,
                             const spi_device_interface_config_t* dev_config,
                             spi_device_handle_t* handle);
esp_err_t spi_bus_remove_device(spi_device_handle_t handle);
esp_err_t spi_device_transmit(spi_device_handle_t device, spi_transaction_t* trans);
esp_err_t spi_device_polling_transmit(spi_device_handle_t device, spi_transaction_t* trans);
esp_err_t spi_device_queue_trans(spi_device_handle_t device, spi_transaction_t* trans,
                                 uint32_t timeout_ms);
spi_transaction_t *spi_device_get_trans_result(spi_device_handle_t device,
                                               uint32_t timeout_ms);
void spi_transaction_data_clear(spi_transaction_t* trans);

#ifdef __cplusplus
}  // extern "C"
#endif
