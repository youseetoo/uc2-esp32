#include "Ws2812Rmt.h"
#include <Arduino.h>

#ifdef ESP32
#include "driver/rmt.h"
#include "esp_intr_alloc.h"

namespace Ws2812Rmt
{
    static const char *TAG = "Ws2812Rmt";

    // WS2812 timing (800 kHz). Tick = APB/clk_div = 80 MHz / 2 = 40 MHz → 25 ns.
    // Datasheet windows: T0H 0.4µs, T0L 0.85µs, T1H 0.8µs, T1L 0.45µs.
    static const uint16_t WS2812_T0H_TICKS = 16; // 0.40 µs
    static const uint16_t WS2812_T0L_TICKS = 34; // 0.85 µs
    static const uint16_t WS2812_T1H_TICKS = 32; // 0.80 µs
    static const uint16_t WS2812_T1L_TICKS = 18; // 0.45 µs

    static bool s_initialised = false;
    static rmt_channel_t s_channel = RMT_CHANNEL_4;
    static int s_pin = -1;

    // Translator: convert raw pixel bytes into RMT items, MSB first per byte.
    static void IRAM_ATTR ws2812_translator(const void *src, rmt_item32_t *dest,
                                            size_t src_size, size_t wanted_num,
                                            size_t *translated_size, size_t *item_num)
    {
        if (src == nullptr || dest == nullptr) {
            *translated_size = 0;
            *item_num = 0;
            return;
        }
        const rmt_item32_t bit0 = {{{WS2812_T0H_TICKS, 1, WS2812_T0L_TICKS, 0}}};
        const rmt_item32_t bit1 = {{{WS2812_T1H_TICKS, 1, WS2812_T1L_TICKS, 0}}};
        size_t size = 0;
        size_t num = 0;
        const uint8_t *psrc = (const uint8_t *)src;
        rmt_item32_t *pdest = dest;
        while (size < src_size && num < wanted_num) {
            for (int i = 0; i < 8; i++) {
                if (*psrc & (1 << (7 - i))) {
                    pdest->val = bit1.val;
                } else {
                    pdest->val = bit0.val;
                }
                num++;
                pdest++;
            }
            size++;
            psrc++;
        }
        *translated_size = size;
        *item_num = num;
    }

    bool begin(int gpioPin, int rmtChannel)
    {
        if (s_initialised) {
            return true;
        }
        s_pin = gpioPin;
        s_channel = (rmt_channel_t)rmtChannel;

        rmt_config_t cfg = {};
        cfg.rmt_mode = RMT_MODE_TX;
        cfg.channel = s_channel;
        cfg.gpio_num = (gpio_num_t)gpioPin;
        cfg.clk_div = 2; // 40 MHz tick
        cfg.mem_block_num = 1;
        cfg.tx_config.loop_en = false;
        cfg.tx_config.carrier_en = false;
        cfg.tx_config.idle_level = RMT_IDLE_LEVEL_LOW;
        cfg.tx_config.idle_output_en = true;

        esp_err_t err = rmt_config(&cfg);
        if (err != ESP_OK) {
            log_e("%s: rmt_config failed: %d", TAG, (int)err);
            return false;
        }

        // CRITICAL DIFFERENCE vs Adafruit_NeoPixel's espShow():
        // pass ESP_INTR_FLAG_SHARED so we can coexist with FastAccelStepper,
        // which already registered a shared ISR for the RMT peripheral.
        err = rmt_driver_install(s_channel, 0, ESP_INTR_FLAG_SHARED);
        if (err != ESP_OK) {
            log_e("%s: rmt_driver_install (shared) failed: %d — retrying without flag",
                  TAG, (int)err);
            // Fallback: try without the flag in case FAS isn't using RMT in this
            // build. Almost always the SHARED path is what works on UC2_4.
            err = rmt_driver_install(s_channel, 0, 0);
            if (err != ESP_OK) {
                log_e("%s: rmt_driver_install fallback also failed: %d", TAG, (int)err);
                return false;
            }
        }

        err = rmt_translator_init(s_channel, ws2812_translator);
        if (err != ESP_OK) {
            log_e("%s: rmt_translator_init failed: %d", TAG, (int)err);
            rmt_driver_uninstall(s_channel);
            return false;
        }

        s_initialised = true;
        log_i("%s: ready on GPIO %d using RMT channel %d", TAG, gpioPin, (int)s_channel);
        return true;
    }

    void show(const uint8_t *pixels, size_t numBytes)
    {
        if (!s_initialised || pixels == nullptr || numBytes == 0) {
            return;
        }
        // rmt_write_sample blocks until all pixels have been pushed (wait_tx_done=true).
        esp_err_t err = rmt_write_sample(s_channel, pixels, numBytes, true);
        if (err != ESP_OK) {
            log_w("%s: rmt_write_sample err=%d", TAG, (int)err);
        }
    }
}

#else // ESP32

namespace Ws2812Rmt
{
    bool begin(int, int) { return false; }
    void show(const uint8_t *, size_t) {}
}

#endif // ESP32
