// T4.1: EncoderBackend — C API over the C++ ESP32Encoder library.
// This file stays C++: the esp32_encoder vendor library is C++ (static
// members, heap alloc); everything it touches is wrapped in extern "C"
// functions that operate on the pure-C struct from EncoderBackend.h.
static const char *TAG = "AxisEncoder";
#include "EncoderBackend.h"
#include "esp_log.h"
#include <PinConfig.h>

// ESP32Encoder is only meaningful when the PCNT counter is compiled in.
#ifdef USE_PCNT_COUNTER
#ifdef ESP_IDF_VERSION_MAJOR
#if ESP_IDF_VERSION_MAJOR >= 4
#include <soc/soc_caps.h>
#include "ESP32Encoder.h"
#define AXIS_ENCODER_HW 1
#endif
#endif
#endif

#ifdef AXIS_ENCODER_HW
// Atomic count reads vs the PCNT ISR.
static portMUX_TYPE axisEncMux = portMUX_INITIALIZER_UNLOCKED;
#endif

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

#ifdef AXIS_ENCODER_HW
    ESP32Encoder::useInternalWeakPullResistors = puType::none;
    ESP32Encoder::isrServiceCpuCore = 0; // isolate from serial/main on core 1

    // Pin the encoder to an EXPLICIT PCNT unit rather than relying on
    // ESP32Encoder's "first free slot" behaviour. attach() scans encoders[]
    // for the first NULL entry; by reserving every slot below the target with
    // a never-attached placeholder, attach() lands exactly on unitIndex.
    // Placeholders are inert: they register no ISR and are never dereferenced.
    static ESP32Encoder slotReservation[MAX_ESP32_ENCODERS];
    int unitIndex = be->unitIndex;
    if (unitIndex < 0 || unitIndex >= MAX_ESP32_ENCODERS)
    {
        log_e("PCNT unit %d out of range [0..%d)", unitIndex, MAX_ESP32_ENCODERS);
        return false;
    }
    for (int i = 0; i < unitIndex; i++)
    {
        if (ESP32Encoder::encoders[i] == nullptr)
            ESP32Encoder::encoders[i] = &slotReservation[i];
    }

    ESP32Encoder *enc = new ESP32Encoder();
    enc->clearCount();
    enc->attachFullQuad(pinA, pinB);
    enc->setCount(0);
    enc->setFilter(glitchFilter);

    if ((int)enc->unit != unitIndex)
    {
        // Reservation failed to steer the slot (another encoder already owned
        // it). Not fatal, but surface it so the collision is diagnosable.
        log_e("Encoder landed on PCNT unit %d, expected %d", (int)enc->unit, unitIndex);
        unitIndex = (int)enc->unit;
        be->unitIndex = unitIndex;
    }
    if (be->unitIndex == ENCODER_BACKEND_FAS_PCNT_UNIT)
    {
        log_e("Encoder ended up on FAS PCNT unit %d — count/step corruption likely", be->unitIndex);
    }

    be->encoderImpl = enc;
    be->present = true;
    log_i("Encoder attached: A=%d B=%d unit=%d filter=%u invert=%d",
             pinA, pinB, be->unitIndex, glitchFilter, invert);
    return true;
#else
    log_e("USE_PCNT_COUNTER not defined — encoder disabled");
    return false;
#endif
}

int64_t encoder_backend_get_count(const EncoderBackend *be)
{
#ifdef AXIS_ENCODER_HW
    if (be->present && be->encoderImpl != nullptr)
    {
        ESP32Encoder *enc = static_cast<ESP32Encoder *>(be->encoderImpl);
        portENTER_CRITICAL(&axisEncMux);
        int64_t count = enc->getCount();
        portEXIT_CRITICAL(&axisEncMux);
        // SINGLE point of direction inversion.
        return be->invertDir ? -count : count;
    }
#endif
    return 0;
}

void encoder_backend_zero(EncoderBackend *be)
{
#ifdef AXIS_ENCODER_HW
    if (be->present && be->encoderImpl != nullptr)
    {
        ESP32Encoder *enc = static_cast<ESP32Encoder *>(be->encoderImpl);
        portENTER_CRITICAL(&axisEncMux);
        enc->setCount(0);
        portEXIT_CRITICAL(&axisEncMux);
    }
#endif
}

void encoder_backend_set_glitch_filter(EncoderBackend *be, uint16_t value)
{
#ifdef AXIS_ENCODER_HW
    if (be->present && be->encoderImpl != nullptr)
    {
        static_cast<ESP32Encoder *>(be->encoderImpl)->setFilter(value);
    }
#endif
}

} // extern "C"
