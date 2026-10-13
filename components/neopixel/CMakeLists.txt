# T5.3: NeoPixel (Adafruit_NeoPixel) — WS2812/WS2811 driver, extracted from the
# Arduino build (was .pio/libdeps/seeed_xiao_esp32s3/Adafruit NeoPixel/).
# Kept as a leaf until T5.3 (component-layout.md §2).
#
# T2.1: pulled forward so xiao_ledservo (raw-IDF, uc2_led) can link it.
set(srcs "Adafruit_NeoPixel.cpp" "esp.c")
idf_component_register(SRCS ${srcs}
    INCLUDE_DIRS ".")
