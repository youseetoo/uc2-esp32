# T2.1: neopixel — Adafruit NeoPixel driver (leaf component, NeoPixel-leaf until T5.3).
# T5.3: converted from the Arduino-ESP32 build (esp.c uses the modern IDF RMT API).
# Gated on UC2_LED_CONTROLLER (the laser app doesn't use it → don't build it).
# Note: the `ESP32` arch macro is #define'd inside Adafruit_NeoPixel.cpp (raw-IDF
# 6.x doesn't auto-define it the way the Arduino build did) — a CMake
# target_compile_definitions on the component target is not available here.
if(CONFIG_UC2_LED_CONTROLLER)
    idf_component_register(SRCS "Adafruit_NeoPixel.cpp" "esp.c"
        INCLUDE_DIRS "."
        REQUIRES driver esp_common)
else()
    idf_component_register(SRCS "" INCLUDE_DIRS ".")
endif()
