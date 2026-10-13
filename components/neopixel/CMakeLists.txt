# T5.3: NeoPixel (Adafruit_NeoPixel) — WS2812/WS2811 driver, extracted from the
# Arduino build (was .pio/libdeps/seeed_xiao_esp32s3/Adafruit NeoPixel/).
# Kept as a leaf until T5.3 (component-layout.md §2).
#
# T2.1: pulled forward so xiao_ledservo (raw-IDF, uc2_led) can link it.
# Gated on UC2_LED_CONTROLLER so apps without LED (e.g. canopen_slave_laser)
# don't compile it (its Adafruit_NeoPixel.cpp needs the raw-IDF arch fix, T5.3).
if(CONFIG_UC2_LED_CONTROLLER)
    idf_component_register(SRCS "Adafruit_NeoPixel.cpp" "esp.c"
        INCLUDE_DIRS ".")
else()
    # Header-only when LED is off (no .cpp compiled for apps like laser).
    idf_component_register(INCLUDE_DIRS ".")
endif()
