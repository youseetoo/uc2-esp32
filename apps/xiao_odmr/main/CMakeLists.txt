# T2.9: xiao_odmr app component — main.c entry (app_main -> uc2_app_start()).
idf_component_register(SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES uc2_app uc2_core uc2_serial uc2_canopen uc2_io uc2_periph uc2_led uc2_motor
             uc2_hal canopen_node uc2_od cjson dac_module
             esp_usb_host_hid driver esp_driver_ledc
             esp_driver_tsens nvs_flash app_update neopixel
             esp_driver_rmt esp_hal_rmt onewire_ds18b20
             accelstepper esp32_encoder)
# Link group to resolve the uc2_app <-> uc2_motor <-> uc2_canopen cycle (T2.6 pattern).
target_link_libraries(__idf_main PRIVATE
    "LINKER:--start-group"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_app/libuc2_app.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_motor/libuc2_motor.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_canopen/libuc2_canopen.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_periph/libuc2_periph.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_io/libuc2_io.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_led/libuc2_led.a"
    # T2.10 regression: uc2_led's LedController.o is only pulled inside this
    # group, so libneopixel.a (Adafruit_NeoPixel) must be re-scanned here too or
    # the RMT/NeoPixel symbols stay undefined (natural order puts neopixel before
    # the group). Same --start-group pattern as the RMT fix in 68791e0.
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/neopixel/libneopixel.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_serial/libuc2_serial.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_core/libuc2_core.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_hal/libuc2_hal.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/accelstepper/libaccelstepper.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/esp32_encoder/libesp32_encoder.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/onewire_ds18b20/libonewire_ds18b20.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/esp_driver_pcnt/libesp_driver_pcnt.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/esp_hal_pcnt/libesp_hal_pcnt.a"
    "LINKER:--end-group")
