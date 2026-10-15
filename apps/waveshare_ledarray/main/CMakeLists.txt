# T2.2: waveshare_ledarray app component — main.c entry (app_main -> uc2_app_start()).
idf_component_register(SRCS "main.c" INCLUDE_DIRS "."
    REQUIRES uc2_app uc2_core uc2_serial uc2_canopen uc2_io uc2_periph uc2_led
             uc2_hal canopen_node uc2_od cjson dac_module
             esp_usb_host_hid driver esp_driver_ledc
             esp_driver_tsens nvs_flash app_update neopixel
             esp_driver_rmt esp_hal_rmt)
# PinConfig.h: app-local main/ holds the waveshare (uc2_hal.h) PinConfig; shadows the
# shared main/ for this app's whole build (via -I main/ in the top-level CMakeLists).
target_include_directories(${COMPONENT_LIB} PRIVATE ${CMAKE_SOURCE_DIR}/main)
# Link-group fix (T2.10 pattern, see xiao_odmr/main/CMakeLists.txt): for this app's
# component set IDF's natural link order places libneopixel.a BEFORE libuc2_led.a, so
# the Adafruit_NeoPixel symbols referenced by uc2_led/LedController.o are never
# re-scanned and stay undefined at link time. Wrapping the uc2_* libs + neopixel in
# --start-group/--end-group makes the linker resolve them iteratively (order-agnostic).
target_link_libraries(__idf_main PRIVATE
    "LINKER:--start-group"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_app/libuc2_app.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_canopen/libuc2_canopen.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_periph/libuc2_periph.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_io/libuc2_io.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_led/libuc2_led.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/neopixel/libneopixel.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_serial/libuc2_serial.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_core/libuc2_core.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_hal/libuc2_hal.a"
    "LINKER:--end-group")
