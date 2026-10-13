# T2.1: uc2_led — WS2812 (Adafruit NeoPixel via RMT) + SignalController, gated on UC2_LED_CONTROLLER.
# REQUIRES neopixel (T5.3, pulled forward) for the Adafruit_NeoPixel.h class API.
include(${CMAKE_CURRENT_LIST_DIR}/../uc2_modinc.cmake)
if(CONFIG_UC2_LED_CONTROLLER)
  list(APPEND UC2_MODULE_INCLUDE_DIRS
    ${CMAKE_CURRENT_LIST_DIR}/led
    ${CMAKE_CURRENT_LIST_DIR}/signal)
  idf_component_register(
    SRCS "led/LedController.cpp" "led/Ws2812Rmt.cpp" "signal/SignalController.cpp"
    INCLUDE_DIRS ${UC2_MODULE_INCLUDE_DIRS}
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
