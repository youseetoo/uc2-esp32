// T5.1: ESP32Encoder C-ified from C++ class to flat C functions.
// Original: https://github.com/madhephaestus/ESP32Encoder (MIT)
// Raw-IDF port using handle-based PCNT API (driver/pulse_cnt.h).
// The PCNT 16-bit counter saturates at ±32767; software accumulation in count
// provides unbounded int64 results via esp32encoder_get_count().

#include "ESP32Encoder.h"
#include "driver/pulse_cnt.h"
#include "driver/gpio.h"
#include "esp_log.h"
#include "soc/clk_tree_defs.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include <string.h>
#include <stdlib.h>
#include <stdint.h>

static const char *TAG = "ESP32Encoder";

#define MAX_ESP32_ENCODERS 6

// Encoder instance state
typedef struct {
    int unit;
    pcnt_unit_handle_t unit_handle;
    int64_t count;
    bool attached;
    bool always_interrupt;
    enc_isr_cb_t isr_cb;
    void *isr_cb_data;
} esp32encoder_inst_t;

static esp32encoder_inst_t encoders[MAX_ESP32_ENCODERS];

// Global: spinlock for software count updates in ISR context
static portMUX_TYPE encoder_spinlock = portMUX_INITIALIZER_UNLOCKED;
#define ENTER_CRITICAL() portENTER_CRITICAL_SAFE(&encoder_spinlock)
#define EXIT_CRITICAL() portEXIT_CRITICAL_SAFE(&encoder_spinlock)

static bool encoders_inited = false;

static void encoder_init_global(void)
{
    if (encoders_inited) return;
    memset(encoders, 0, sizeof(encoders));
    for (int i = 0; i < MAX_ESP32_ENCODERS; i++) {
        encoders[i].unit = -1;
    }
    encoders_inited = true;
}

// PCNT watch-point callback (ISR context): fires when the 16-bit counter reaches
// a configured watch point (±32767). Accumulate the limit into the software
// counter and reset the hardware counter to 0.
static bool esp32encoder_pcnt_on_reach(pcnt_unit_handle_t unit,
                                       const pcnt_watch_event_data_t *edata,
                                       void *user_ctx)
{
    ESP32Encoder *enc = (ESP32Encoder *)user_ctx;
    esp32encoder_inst_t *inst = &encoders[enc->unit];

    ENTER_CRITICAL();
    if (edata->watch_point_value >= INT16_MAX) {
        inst->count += INT16_MAX;
    } else if (edata->watch_point_value <= INT16_MIN) {
        inst->count += INT16_MIN;
    }
    pcnt_unit_clear_count(unit);
    if (inst->isr_cb) {
        inst->isr_cb(inst->isr_cb_data);
    }
    EXIT_CRITICAL();
    return false; // keep the PCNT unit running
}

ESP32Encoder *esp32encoder_create(bool always_interrupt, enc_isr_cb_t isr_cb, void *isr_cb_data)
{
    encoder_init_global();

    ESP32Encoder *enc = (ESP32Encoder *)calloc(1, sizeof(ESP32Encoder));
    if (!enc) return NULL;

    // Find a free slot
    for (int i = 0; i < MAX_ESP32_ENCODERS; i++) {
        if (encoders[i].unit == -1) {
            enc->unit = i;
            encoders[i].unit = i;
            encoders[i].always_interrupt = always_interrupt;
            encoders[i].isr_cb = isr_cb;
            encoders[i].isr_cb_data = isr_cb_data ? isr_cb_data : enc;
            break;
        }
    }

    if (enc->unit < 0 || enc->unit >= MAX_ESP32_ENCODERS) {
        free(enc);
        return NULL;
    }

    return enc;
}

void esp32encoder_delete(ESP32Encoder *enc)
{
    if (!enc) return;
    esp32encoder_detach(enc);
    if (enc->unit >= 0 && enc->unit < MAX_ESP32_ENCODERS) {
        encoders[enc->unit].unit = -1;
    }
    free(enc);
}

void esp32encoder_attach(ESP32Encoder *enc, int a, int b, encType et)
{
    if (!enc) return;
    if (enc->attached) {
        ESP_LOGE(TAG, "attach: already attached");
        return;
    }

    encoders[enc->unit].attached = true;
    encoders[enc->unit].count = 0;

    int pin_a = a;
    int pin_b = b;

    // Configure GPIOs
    gpio_config_t gpio_conf = {
        .pin_bit_mask = ((1ULL << pin_a) | (1ULL << pin_b)),
        .mode = GPIO_MODE_INPUT,
        .pull_up_en = GPIO_PULLUP_ENABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type = GPIO_INTR_DISABLE,
    };
    gpio_config(&gpio_conf);

    // Create PCNT unit
    pcnt_unit_config_t unit_cfg = {
        .group_id = 0,
        .clk_src = PCNT_CLK_SRC_DEFAULT,
        .low_limit = INT16_MIN,
        .high_limit = INT16_MAX,
        .intr_priority = 1,
        .flags.accum_count = false,
    };
    esp_err_t err = pcnt_new_unit(&unit_cfg, &encoders[enc->unit].unit_handle);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "attach: pcnt_new_unit failed: %d", err);
        return;
    }

    // Channel 0: A = edge signal, B = level signal
    pcnt_chan_config_t ch0_cfg = {
        .edge_gpio_num = pin_a,
        .level_gpio_num = pin_b,
    };
    pcnt_channel_handle_t ch0 = NULL;
    err = pcnt_new_channel(encoders[enc->unit].unit_handle, &ch0_cfg, &ch0);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "attach: pcnt_new_channel(ch0) failed: %d", err);
        return;
    }

    if (et == ENCODER_SINGLE) {
        // Count only on A falling edges (legacy: pos=HOLD, neg=INCREASE)
        pcnt_channel_set_edge_action(ch0, PCNT_CHANNEL_EDGE_ACTION_HOLD, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
        pcnt_channel_set_level_action(ch0, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
    } else {
        // Quadrature: A rising -> DEC, A falling -> INC, B high -> reverse
        pcnt_channel_set_edge_action(ch0, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
        pcnt_channel_set_level_action(ch0, PCNT_CHANNEL_LEVEL_ACTION_INVERSE, PCNT_CHANNEL_LEVEL_ACTION_KEEP);
    }

    // Channel 1 (full quadrature only): B = edge signal, A = level signal
    if (et == ENCODER_FULL) {
        pcnt_chan_config_t ch1_cfg = {
            .edge_gpio_num = pin_b,
            .level_gpio_num = pin_a,
        };
        pcnt_channel_handle_t ch1 = NULL;
        err = pcnt_new_channel(encoders[enc->unit].unit_handle, &ch1_cfg, &ch1);
        if (err == ESP_OK) {
            pcnt_channel_set_edge_action(ch1, PCNT_CHANNEL_EDGE_ACTION_DECREASE, PCNT_CHANNEL_EDGE_ACTION_INCREASE);
            pcnt_channel_set_level_action(ch1, PCNT_CHANNEL_LEVEL_ACTION_KEEP, PCNT_CHANNEL_LEVEL_ACTION_INVERSE);
        } else {
            ESP_LOGE(TAG, "attach: pcnt_new_channel(ch1) failed: %d", err);
        }
    }

    // Configure wrap-around via watch points
    pcnt_unit_add_watch_point(encoders[enc->unit].unit_handle, INT16_MAX);
    pcnt_unit_add_watch_point(encoders[enc->unit].unit_handle, INT16_MIN);

    // Register callback for wrap-around
    pcnt_event_callbacks_t cbs = {
        .on_reach = esp32encoder_pcnt_on_reach,
    };
    pcnt_unit_register_event_callbacks(encoders[enc->unit].unit_handle, &cbs, enc);

    // Default glitch filter (legacy default: 250)
    pcnt_glitch_filter_config_t filter_cfg = {
        .max_glitch_ns = 250u * 100u,
    };
    pcnt_unit_set_glitch_filter(encoders[enc->unit].unit_handle, &filter_cfg);

    pcnt_unit_enable(encoders[enc->unit].unit_handle);
    pcnt_unit_start(encoders[enc->unit].unit_handle);
    pcnt_unit_clear_count(encoders[enc->unit].unit_handle);

    enc->attached = true;
}

void esp32encoder_detach(ESP32Encoder *enc)
{
    if (!enc) return;
    if (encoders[enc->unit].unit_handle) {
        pcnt_unit_stop(encoders[enc->unit].unit_handle);
        pcnt_unit_disable(encoders[enc->unit].unit_handle);
        pcnt_del_unit(encoders[enc->unit].unit_handle);
        encoders[enc->unit].unit_handle = NULL;
    }
    encoders[enc->unit].attached = false;
    enc->attached = false;
}

int64_t esp32encoder_get_count(ESP32Encoder *enc)
{
    if (!enc || !encoders[enc->unit].attached) return 0;

    int raw = 0;
    pcnt_unit_get_count(encoders[enc->unit].unit_handle, &raw);

    ENTER_CRITICAL();
    int64_t total = encoders[enc->unit].count + raw;
    EXIT_CRITICAL();

    return total;
}

void esp32encoder_set_count(ESP32Encoder *enc, int64_t value)
{
    if (!enc || !encoders[enc->unit].attached) return;

    int raw = 0;
    pcnt_unit_get_count(encoders[enc->unit].unit_handle, &raw);

    ENTER_CRITICAL();
    encoders[enc->unit].count = value - raw;
    EXIT_CRITICAL();
}

int64_t esp32encoder_clear_count(ESP32Encoder *enc)
{
    if (!enc || !encoders[enc->unit].attached) return 0;

    int raw = 0;
    pcnt_unit_get_count(encoders[enc->unit].unit_handle, &raw);
    pcnt_unit_clear_count(encoders[enc->unit].unit_handle);

    ENTER_CRITICAL();
    int64_t prev = encoders[enc->unit].count + raw;
    encoders[enc->unit].count = 0;
    EXIT_CRITICAL();

    return prev;
}

int64_t esp32encoder_pause_count(ESP32Encoder *enc)
{
    if (!enc || !encoders[enc->unit].attached) return 0;

    int raw = 0;
    pcnt_unit_get_count(encoders[enc->unit].unit_handle, &raw);
    pcnt_unit_stop(encoders[enc->unit].unit_handle);

    return raw;
}

int64_t esp32encoder_resume_count(ESP32Encoder *enc)
{
    if (!enc || !encoders[enc->unit].attached) return 0;

    int raw = 0;
    pcnt_unit_get_count(encoders[enc->unit].unit_handle, &raw);
    pcnt_unit_start(encoders[enc->unit].unit_handle);

    return raw;
}