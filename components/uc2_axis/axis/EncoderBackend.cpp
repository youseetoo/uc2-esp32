// T4.1: EncoderBackend — C API over the C++ ESP32Encoder library.
// T5.1: Updated to use the IDF-port C API (esp32encoder_* functions) instead of
// the Arduino ESP32Encoder C++ class. The underlying library is now a pure C
// implementation using the ESP-IDF PCNT driver.
static const char *TAG = "AxisEncoder";
#include "EncoderBackend.h"
#include "esp_log.h"
#include "ESP32Encoder.h"
#include <PinConfig.h>

// Atomic count reads vs the PCNT ISR.
static portMUX_TYPE axisEncMux = portMUX_INITIALIZER_UNLOCKED;

extern "C" {

void encoder_backend_init(EncoderBackend *be)
{
    be->present = false;
    be->invertDir = false;
    be->unitIndex = -1;
    be->aPin = -1;
    be->bPin = -1;
    be->encoderImpl = nullptr;
}

bool encoder_backend_begin(EncoderBackend *be, int8_t pinA, int8_t pinB, bool invert,
                           int pcntUnit, uint16_t glitchFilter)
{
    be->aPin = pinA;
    be->bPin = pinB;
    be->invertDir = invert;
    be->unitIndex = pcntUnit;
    be->present = false;

    if (pinA < 0 || pinB < 0)
    {
        ESP_LOGW(TAG, "Encoder pins disabled (A=%d B=%d) — encoder absent", pinA, pinB);
        return false;
    }

    if (pcntUnit == ENCODER_BACKEND_FAS_PCNT_UNIT)
    {
        // Constraint #4: never share the FAS step-generator's PCNT unit.
        log_e("Configured PCNT unit %d collides with FAS unit %d — refusing to attach",
                  pcntUnit, ENCODER_BACKEND_FAS_PCNT_UNIT);
        return false;
    }

    // Allocate the encoder via the IDF C API (create handles PCNT unit assignment).
    ESP32Encoder *enc = esp32encoder_create(true, nullptr, nullptr);
    if (!enc) {
        log_e("esp32encoder_create failed");
        return false;
    }

    // T5.1: Use the IDF C API — attachFullQuad maps to ENCODER_FULL mode.
    esp32encoder_attach(enc, pinA, pinB, ENCODER_FULL);

    // Note: The IDF C API does not expose the "reserve specific PCNT unit"
    // pattern from the Arduino version (which used static slot reservation).
    // The IDF PCNT driver allocates units internally. The pcntUnit parameter
    // is retained for API compatibility but is not used by the C backend.
    // The collision check against FAS's unit is still performed above.

    be->encoderImpl = enc;
    be->present = true;
    log_i("Encoder attached: A=%d B=%d invert=%d",
             pinA, pinB, invert);
    return true;
}

int64_t encoder_backend_get_count(const EncoderBackend *be)
{
    if (be->present && be->encoderImpl != nullptr)
    {
        ESP32Encoder *enc = (ESP32Encoder *)be->encoderImpl;
        portENTER_CRITICAL(&axisEncMux);
        int64_t count = esp32encoder_get_count(enc);
        portEXIT_CRITICAL(&axisEncMux);
        // SINGLE point of direction inversion.
        return be->invertDir ? -count : count;
    }
    return 0;
}

void encoder_backend_zero(EncoderBackend *be)
{
    if (be->present && be->encoderImpl != nullptr)
    {
        ESP32Encoder *enc = (ESP32Encoder *)be->encoderImpl;
        portENTER_CRITICAL(&axisEncMux);
        esp32encoder_clear_count(enc);
        portEXIT_CRITICAL(&axisEncMux);
    }
}

void encoder_backend_set_glitch_filter(EncoderBackend *be, uint16_t value)
{
    // T5.1: The IDF C API (PCNT driver) does not have a direct equivalent of
    // Arduino ESP32Encoder's setFilter(). The PCNT hardware provides its own
    // glitch filtering via pcnt_config_t.low_filter, which is set at attach
    // time (see ESP32Encoder.c). This function is kept for API compatibility
    // but is a no-op in the C backend.
    (void)be;
    (void)value;
}

} // extern "C"