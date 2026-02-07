/**
 * @file DAC_MCP4822.cpp
 * @brief MCP4822 Dual 12-bit DAC driver implementation
 * 
 * Optimized for high-speed galvo control with minimal latency.
 */

#include "DAC_MCP4822.h"
#include "esp_log.h"
#include <cstring>

static const char* TAG = "DAC_MCP4822";

DAC_MCP4822::~DAC_MCP4822()
{
    if (spi_) {
        spi_bus_remove_device(spi_);
        spi_ = nullptr;
    }
}

bool DAC_MCP4822::init(spi_host_device_t spi_host, int pin_mosi, int pin_sclk, 
                       int pin_cs, int pin_ldac, int clock_speed_hz)
{
    // Initialize SPI bus
    spi_bus_config_t buscfg = {};
    buscfg.mosi_io_num = pin_mosi;
    buscfg.miso_io_num = -1;  // MCP4822 has no MISO
    buscfg.sclk_io_num = pin_sclk;
    buscfg.quadwp_io_num = -1;
    buscfg.quadhd_io_num = -1;
    buscfg.max_transfer_sz = 4096;

    esp_err_t ret = spi_bus_initialize(spi_host, &buscfg, SPI_DMA_CH_AUTO);
    if (ret != ESP_OK && ret != ESP_ERR_INVALID_STATE) {
        // ESP_ERR_INVALID_STATE means bus already initialized (shared bus)
        ESP_LOGE(TAG, "SPI bus init failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Add SPI device with optimal settings for MCP4822
    spi_device_interface_config_t devcfg = {};
    devcfg.clock_speed_hz = clock_speed_hz;
    devcfg.mode = 0;  // SPI mode 0 (CPOL=0, CPHA=0)
    devcfg.spics_io_num = pin_cs;
    devcfg.queue_size = 1;
    devcfg.flags = SPI_DEVICE_NO_DUMMY;  // No dummy bits for maximum speed

    ret = spi_bus_add_device(spi_host, &devcfg, &spi_);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "SPI device add failed: %s", esp_err_to_name(ret));
        return false;
    }

    // Configure LDAC pin if used
    pin_ldac_ = pin_ldac;
    if (pin_ldac_ >= 0) {
        gpio_config_t io = {};
        io.intr_type = GPIO_INTR_DISABLE;
        io.mode = GPIO_MODE_OUTPUT;
        io.pin_bit_mask = (1ULL << pin_ldac_);
        io.pull_down_en = GPIO_PULLDOWN_DISABLE;
        io.pull_up_en = GPIO_PULLUP_DISABLE;
        gpio_config(&io);
        gpio_set_level((gpio_num_t)pin_ldac_, 1);  // Default high (inactive)
    }

    initialized_ = true;
    ESP_LOGI(TAG, "Initialized (MOSI=%d, SCLK=%d, CS=%d, LDAC=%d, %d MHz)", 
             pin_mosi, pin_sclk, pin_cs, pin_ldac_, clock_speed_hz / 1000000);
    return true;
}

void DAC_MCP4822::write(bool channel_b, uint16_t value)
{
    if (!spi_) return;

    // MCP4822 command format (16 bits):
    // D15: DAC select (0=A, 1=B)
    // D14: BUF (0=unbuffered, 1=buffered Vref) - use unbuffered for speed
    // D13: GA (1=1x gain, 0=2x gain) - use 1x for full range
    // D12: SHDN (1=active, 0=shutdown)
    // D11-D0: 12-bit data
    
    value &= 0x0FFF;  // Ensure 12-bit
    uint16_t cmd = 0;
    cmd |= (channel_b ? 1 : 0) << 15;  // Channel select
    cmd |= (0u) << 14;                  // BUF=0 (unbuffered for speed)
    cmd |= (1u) << 13;                  // GA=1 (1x gain)
    cmd |= (1u) << 12;                  // SHDN=1 (active)
    cmd |= value;

    spi_transaction_t t;
    memset(&t, 0, sizeof(t));
    t.flags = SPI_TRANS_USE_TXDATA;
    t.length = 16;  // 16 bits
    t.tx_data[0] = (uint8_t)(cmd >> 8);
    t.tx_data[1] = (uint8_t)(cmd & 0xFF);

    // Use polling transmit for deterministic timing (no queue jitter)
    spi_device_polling_transmit(spi_, &t);
}

void DAC_MCP4822::setXY(uint16_t x, uint16_t y)
{
    if (!spi_) return;

    // Prepare both transactions
    x &= 0x0FFF;
    y &= 0x0FFF;
    
    // Channel A (X)
    uint16_t cmd_x = (0u << 15) | (0u << 14) | (1u << 13) | (1u << 12) | x;
    // Channel B (Y)
    uint16_t cmd_y = (1u << 15) | (0u << 14) | (1u << 13) | (1u << 12) | y;

    spi_transaction_t t1, t2;
    memset(&t1, 0, sizeof(t1));
    memset(&t2, 0, sizeof(t2));
    
    t1.flags = SPI_TRANS_USE_TXDATA;
    t1.length = 16;
    t1.tx_data[0] = (uint8_t)(cmd_x >> 8);
    t1.tx_data[1] = (uint8_t)(cmd_x & 0xFF);

    t2.flags = SPI_TRANS_USE_TXDATA;
    t2.length = 16;
    t2.tx_data[0] = (uint8_t)(cmd_y >> 8);
    t2.tx_data[1] = (uint8_t)(cmd_y & 0xFF);

    // Send both in sequence for atomic update with LDAC
    spi_device_polling_transmit(spi_, &t1);
    spi_device_polling_transmit(spi_, &t2);
}

void DAC_MCP4822::ldacPulse()
{
    if (pin_ldac_ < 0) return;
    
    // Pulse LDAC low to latch both channels simultaneously
    gpio_set_level((gpio_num_t)pin_ldac_, 0);
    // Short pulse (~100ns) - NOP instructions for minimal delay
    __asm__ __volatile__("nop; nop; nop; nop;");
    gpio_set_level((gpio_num_t)pin_ldac_, 1);
}
