// Implements the RMT peripheral on ESP32 (raw ESP-IDF 6.x) for NeoPixel.
// T5.3: converted from the Arduino-ESP32 build. Uses the modern
// rmt_new_tx_channel / rmt_new_copy_encoder / rmt_transmit API.
#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <stdlib.h>
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"
#include "driver/rmt_tx.h"
#include "driver/rmt_encoder.h"
#include "driver/gpio.h"

#define WS2812_T0H_NS 400
#define WS2812_T0L_NS 850
#define WS2812_T1H_NS 800
#define WS2812_T1L_NS 450
#define WS2811_T0H_NS 500
#define WS2811_T0L_NS 2000
#define WS2811_T1H_NS 1200
#define WS2811_T1L_NS 1300

static rmt_channel_handle_t show_channel = NULL;
static rmt_encoder_handle_t show_encoder = NULL;
static int show_channel_pin = -1;
static SemaphoreHandle_t show_mutex = NULL;

// All Adafruit_NeoPixel instances must be constructed before tasks start, so
// the mutex can be created once (avoids a race on lazy init).
void espInit(void) {
    if (!show_mutex) {
        show_mutex = xSemaphoreCreateMutex();
    }
}

void espShow(uint16_t pin, uint8_t *pixels, uint32_t numBytes, uint8_t type) {
    if (!show_mutex) espInit();
    if (!show_mutex) return;
    xSemaphoreTake(show_mutex, portMAX_DELAY);

    // Create (or rebind) the RMT TX channel for the requested pin.
    if (!show_channel || show_channel_pin != (int)pin) {
        if (show_channel) {
            rmt_del_channel(show_channel);
            show_channel = NULL;
        }
        if (show_encoder) {
            rmt_del_encoder(show_encoder);
            show_encoder = NULL;
        }
        const rmt_tx_channel_config_t cfg = {
            .gpio_num = (gpio_num_t)pin,
            .clk_src = RMT_CLK_SRC_DEFAULT,
            .resolution_hz = 10 * 1000 * 1000, // 100 ns per tick
            .mem_block_symbols = 64,
            .trans_queue_depth = 0,
        };
        if (rmt_new_tx_channel(&cfg, &show_channel) != ESP_OK) {
            xSemaphoreGive(show_mutex);
            return;
        }
        if (rmt_new_copy_encoder(NULL, &show_encoder) != ESP_OK) {
            rmt_del_channel(show_channel);
            show_channel = NULL;
            xSemaphoreGive(show_mutex);
            return;
        }
        show_channel_pin = (int)pin;
    }

    // type = is800KHz (1 = WS2812 800 kHz-class, 0 = WS2811 400 kHz).
    const uint32_t t0h = (type ? WS2812_T0H_NS : WS2811_T0H_NS) / 100;
    const uint32_t t0l = (type ? WS2812_T0L_NS : WS2811_T0L_NS) / 100;
    const uint32_t t1h = (type ? WS2812_T1H_NS : WS2811_T1H_NS) / 100;
    const uint32_t t1l = (type ? WS2812_T1L_NS : WS2811_T1L_NS) / 100;

    const size_t n = (size_t)numBytes * 8;
    rmt_symbol_word_t *symbols =
        (rmt_symbol_word_t *)malloc(n * sizeof(rmt_symbol_word_t));
    if (symbols == NULL) {
        xSemaphoreGive(show_mutex);
        return;
    }
    size_t i = 0;
    for (uint32_t b = 0; b < numBytes; b++) {
        for (int bit = 7; bit >= 0; bit--) {
            if (pixels[b] & (1u << bit)) {
                symbols[i++] = (rmt_symbol_word_t){.duration0 = t1h, .level0 = 1,
                                                   .duration1 = t1l, .level1 = 0};
            } else {
                symbols[i++] = (rmt_symbol_word_t){.duration0 = t0h, .level0 = 1,
                                                   .duration1 = t0l, .level1 = 0};
            }
        }
    }

    rmt_transmit(show_channel, show_encoder, symbols, n * sizeof(rmt_symbol_word_t), NULL);
    rmt_tx_wait_all_done(show_channel, 100);
    free(symbols);

    xSemaphoreGive(show_mutex);
}
