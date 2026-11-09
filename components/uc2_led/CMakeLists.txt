# T2.1: uc2_led — WS2812 (pure C RMT driver via ws2812_rmt.c), DotStar (Adafruit),
# HUB75 (Adafruit Protomatter), and SignalController. Gated on UC2_LED_CONTROLLER.
# T5.3: NeoPixel dependency replaced with ws2812_rmt C driver for the WS2812 path.
# DotStar and HUB75 still use Adafruit C++ libs (deferred to T5.x).
include(${CMAKE_CURRENT_LIST_DIR}/../uc2_modinc.cmake)
if(CONFIG_UC2_LED_CONTROLLER)
  list(APPEND UC2_MODULE_INCLUDE_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/led
    ${CMAKE_CURRENT_LIST_DIR}/signal)
  # T4.1: LedController is now pure C (LedController.c); the Adafruit/
  # ws2812_rmt leaf drivers live in led_strip_c.cpp (C++ shim).
  # REQUIRES: neopixel is still needed for DotStar/HUB75 (deferred to T5.x).
  idf_component_register(
    SRCS "led/LedController.c" "led/led_strip_c.cpp" "led/ws2812_rmt.c" "signal/SignalController.c" "signal/signal_neopixel_c.cpp"
    INCLUDE_DIRS ${UC2_MODULE_INCLUDE_DIRS} ${CMAKE_CURRENT_LIST_DIR}/../neopixel
    REQUIRES uc2_core uc2_hal uc2_serial uc2_canopen uc2_periph cjson neopixel
             driver esp_driver_rmt esp_driver_gpio esp_driver_periph_ctrl
             esp_common hal freertos)

else()
  # UC2_LED_CONTROLLER off (e.g. the CANopen laser app): register a header-only
  # stub so REQUIRES from other components still resolve.
  file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/uc2_led_stub.c "/* uc2_led: UC2_LED_CONTROLLER off */\n")
  idf_component_register(
    SRCS ${CMAKE_CURRENT_BINARY_DIR}/uc2_led_stub.c
    INCLUDE_DIRS ${UC2_MODULE_INCLUDE_DIRS}
    REQUIRES uc2_core uc2_hal)
endif()