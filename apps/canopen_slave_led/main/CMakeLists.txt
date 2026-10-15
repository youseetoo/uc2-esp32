# T2.4: canopen_slave_led app entry point.
# main.c is a thin shim: the real logic lives in components/uc2_app/main.cpp,
# compiled by uc2_app (T1.4) and gated by the same #ifdefs as the PIO build.
idf_component_register(SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES uc2_app uc2_led uc2_periph neopixel)
# Link-group fix (T2.10 pattern, see xiao_odmr/main/CMakeLists.txt): for this app's
# component set IDF's natural link order places libneopixel.a BEFORE libuc2_led.a, so
# the Adafruit_NeoPixel symbols referenced by uc2_led/LedController.o are never
# re-scanned and stay undefined at link time. This app also carries the uc2_app <->
# uc2_motor <-> uc2_canopen cluster, so wrap the whole uc2_* libs + neopixel + the
# stepper/encoder/onewire/canopen_node cluster in --start-group/--end-group to make
# the linker resolve them iteratively (order-agnostic).
target_link_libraries(__idf_main PRIVATE
    "LINKER:--start-group"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_app/libuc2_app.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_motor/libuc2_motor.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_canopen/libuc2_canopen.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_periph/libuc2_periph.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_io/libuc2_io.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_led/libuc2_led.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/neopixel/libneopixel.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_serial/libuc2_serial.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_core/libuc2_core.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_hal/libuc2_hal.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_od/libuc2_od.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/tmcstepper/libtmcstepper.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/fastaccelstepper/libfastaccelstepper.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/accelstepper/libaccelstepper.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/esp32_encoder/libesp32_encoder.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/onewire_ds18b20/libonewire_ds18b20.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/canopen_node/libcanopen_node.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/esp_driver_pcnt/libesp_driver_pcnt.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/esp_hal_pcnt/libesp_hal_pcnt.a"
    "LINKER:--end-group")
