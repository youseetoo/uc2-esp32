#include "EncoderBackend.h"
#include "esp_log.h"

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

static const char *TAG = "AxisEncoder";

#ifdef AXIS_ENCODER_HW
// Atomic count reads vs the PCNT ISR.
static portMUX_TYPE axisEncMux = portMUX_INITIALIZER_UNLOCKED;
#endif

bool EncoderBackend::begin(int8_t pinA, int8_t pinB, bool invert, int pcntUnit,
                           uint16_t glitchFilter)
{
    aPin = pinA;
    bPin = pinB;
    invertDir = invert;
    unitIndex = pcntUnit;
    present = false;

    if (pinA < 0 || pinB < 0)
    {
        ESP_LOGW(TAG, "Encoder pins disabled (A=%d B=%d) — encoder absent", pinA, pinB);
        return false;
    }

    if (pcntUnit == FAS_PCNT_UNIT)
    {
        // Constraint #4: never share the FAS step-generator's PCNT unit.
        ESP_LOGE(TAG, "Configured PCNT unit %d collides with FAS unit %d — refusing to attach",
                 pcntUnit, FAS_PCNT_UNIT);
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
    if (unitIndex < 0 || unitIndex >= MAX_ESP32_ENCODERS)
    {
        ESP_LOGE(TAG, "PCNT unit %d out of range [0..%d)", unitIndex, MAX_ESP32_ENCODERS);
        return false;
    }
    for (int i = 0; i < unitIndex; i++)
    {
        if (ESP32Encoder::encoders[i] == nullptr)
            ESP32Encoder::encoders[i] = &slotReservation[i];
    }

    ESP32Encoder *enc = new ESP32Encoder();
    enc->clearCount();
    enc->attachFullQuad(aPin, bPin);
    enc->setCount(0);
    enc->setFilter(glitchFilter);

    if ((int)enc->unit != unitIndex)
    {
        // Reservation failed to steer the slot (another encoder already owned
        // it). Not fatal, but surface it so the collision is diagnosable.
        ESP_LOGE(TAG, "Encoder landed on PCNT unit %d, expected %d", (int)enc->unit, unitIndex);
        unitIndex = (int)enc->unit;
    }
    if (unitIndex == FAS_PCNT_UNIT)
    {
        ESP_LOGE(TAG, "Encoder ended up on FAS PCNT unit %d — count/step corruption likely", unitIndex);
    }

    encoderImpl = enc;
    present = true;
    ESP_LOGI(TAG, "Encoder attached: A=%d B=%d unit=%d filter=%u invert=%d",
             aPin, bPin, unitIndex, glitchFilter, invertDir);
    return true;
#else
    ESP_LOGW(TAG, "USE_PCNT_COUNTER not defined — encoder disabled");
    return false;
#endif
}

int64_t EncoderBackend::getCount()
{
#ifdef AXIS_ENCODER_HW
    if (present && encoderImpl != nullptr)
    {
        ESP32Encoder *enc = static_cast<ESP32Encoder *>(encoderImpl);
        portENTER_CRITICAL(&axisEncMux);
        int64_t count = enc->getCount();
        portEXIT_CRITICAL(&axisEncMux);
        // SINGLE point of direction inversion.
        return invertDir ? -count : count;
    }
#endif
    return 0;
}

void EncoderBackend::zero()
{
#ifdef AXIS_ENCODER_HW
    if (present && encoderImpl != nullptr)
    {
        ESP32Encoder *enc = static_cast<ESP32Encoder *>(encoderImpl);
        portENTER_CRITICAL(&axisEncMux);
        enc->setCount(0);
        portEXIT_CRITICAL(&axisEncMux);
    }
#endif
}

void EncoderBackend::setGlitchFilter(uint16_t value)
{
#ifdef AXIS_ENCODER_HW
    if (present && encoderImpl != nullptr)
    {
        static_cast<ESP32Encoder *>(encoderImpl)->setFilter(value);
    }
#endif
}
