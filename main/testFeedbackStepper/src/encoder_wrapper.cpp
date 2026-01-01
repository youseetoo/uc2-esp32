#include <Arduino.h>
#include <ESP32Encoder.h>

// XIAO ESP32S3 pins from your config:
static constexpr int ENC_A_PIN = 6;  // GPIO_NUM_6
static constexpr int ENC_B_PIN = 5;  // GPIO_NUM_5

static ESP32Encoder g_encoder;

void encoder_init() {
    // Use weak pull-ups, adjust if your encoder needs DOWN
    ESP32Encoder::useInternalWeakPullResistors = puType::up;

    g_encoder.attachFullQuad(ENC_A_PIN, ENC_B_PIN);
    g_encoder.clearCount();
}

int32_t encoder_get() {
    return static_cast<int32_t>(g_encoder.getCount());
}

void encoder_set_zero() {
    g_encoder.setCount(0);
}
