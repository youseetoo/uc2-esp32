#pragma once

#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stdbool.h>

// ESP32Encoder C API — raw-IDF port using handle-based PCNT API (driver/pulse_cnt.h).
// The 16-bit PCNT counter saturates at ±32767; software accumulation in count
// provides unbounded int64 results via esp32encoder_get_count().

#define ESP32ENCODER_LIB_VERSION "0.1.0"
#define MAX_ESP32_ENCODERS 6

typedef void (*enc_isr_cb_t)(void *data);

// Encoder type: single = count on A falling edges only; full = quadrature
typedef enum {
    ENCODER_SINGLE = 0,
    ENCODER_FULL = 1,
} encType;

// Encoder handle
typedef struct ESP32Encoder {
    int unit;
    bool attached;
} ESP32Encoder;

ESP32Encoder *esp32encoder_create(bool always_interrupt, enc_isr_cb_t isr_cb, void *isr_cb_data);
void esp32encoder_delete(ESP32Encoder *enc);
void esp32encoder_attach(ESP32Encoder *enc, int a, int b, encType et);
void esp32encoder_detach(ESP32Encoder *enc);
int64_t esp32encoder_get_count(ESP32Encoder *enc);
void esp32encoder_set_count(ESP32Encoder *enc, int64_t value);
int64_t esp32encoder_clear_count(ESP32Encoder *enc);
int64_t esp32encoder_pause_count(ESP32Encoder *enc);
int64_t esp32encoder_resume_count(ESP32Encoder *enc);

#ifdef __cplusplus
}
#endif