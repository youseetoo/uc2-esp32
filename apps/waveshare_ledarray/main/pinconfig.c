// T6.6: waveshare_ledarray — board pin table, pure C.
//
// Only the fields this board wires up are set; the rest are zeroed by the
// compiler (global const object). Former C++ derived struct
// `UC2_ESP32S3_WAVESHARE_LEDARRAY : PinConfig` (lived in PinConfig.h).
#include "pinconfig_defaults.h"

const struct PinConfig pinConfig = {
    /* board overrides */
    .pindefName = "UC2_esp32s3_waveshare_ledarray",
    .BAUDRATE = 115200,

    /* LED array */
    .LED_PIN = GPIO_NUM_14,
    .LED_COUNT = 64,
};
