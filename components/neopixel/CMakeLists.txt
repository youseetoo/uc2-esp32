# T2.1: neopixel — Adafruit NeoPixel driver (leaf component, NeoPixel-leaf until T5.3).
# T5.3: converted from the Arduino-ESP32 build (esp.c uses the modern IDF RMT API).
# Gated on UC2_LED_CONTROLLER (the laser app doesn't use it → don't build it).
# Note: the `ESP32` arch macro is #define'd inside Adafruit_NeoPixel.cpp (raw-IDF
# 6.x doesn't auto-define it the way the Arduino build did) — a CMake
# target_compile_definitions on the component target is not available here.
if(CONFIG_UC2_LED_CONTROLLER)
    # Classic ESP32 has no DMA controller: on a stock ESP-IDF 6.1 tree the
    # esp_hal_dma component registers ZERO sources for target esp32, so no
    # libesp_hal_dma.a is ever built. Referencing it (REQUIRES propagates it
    # onto every dependent's link line, and the LINKER: path below hardcodes
    # the archive) then breaks the link of every classic-ESP32 app on CI.
    # (The local dev IDF carries an empty esp32/dma_stub.c patch that creates
    # the archive, but the CI image does not — the fix must work without it.)
    # No esp32 code references any esp_hal_dma symbol, so on esp32 drop it
    # from REQUIRES and from the link group; keep it everywhere else.
    idf_build_get_property(_uc2_neo_target IDF_TARGET)
    set(_uc2_neo_requires esp_driver_rmt esp_hal_rmt esp_driver_gpio esp_driver_periph_ctrl
                          esp_common hal freertos uc2_hal esp_hw_support)
    set(_uc2_neo_link
        "LINKER:--start-group"
        "LINKER:${CMAKE_BINARY_DIR}/esp-idf/esp_driver_rmt/libesp_driver_rmt.a"
        "LINKER:${CMAKE_BINARY_DIR}/esp-idf/esp_hal_rmt/libesp_hal_rmt.a"
        "LINKER:${CMAKE_BINARY_DIR}/esp-idf/esp_driver_dma/libesp_driver_dma.a"
        "LINKER:--end-group")
    if(NOT _uc2_neo_target STREQUAL "esp32")
        list(APPEND _uc2_neo_requires esp_hal_dma)
        list(APPEND _uc2_neo_link "LINKER:${CMAKE_BINARY_DIR}/esp-idf/esp_hal_dma/libesp_hal_dma.a")
    endif()
    # WHOLE_ARCHIVE: the IDF link line repeats the uc2_led component AFTER
    # neopixel, so uc2_led's Adafruit_NeoPixel references (signal_neopixel_c.cpp)
    # are only resolved if the Adafruit object is unconditionally defined at the
    # first neopixel occurrence. esp.c's rmt_* refs are resolved by the
    # --start-group below (added right after this lib).
    idf_component_register(SRCS "Adafruit_NeoPixel.cpp" "esp.c"
        INCLUDE_DIRS "."
        WHOLE_ARCHIVE
        REQUIRES ${_uc2_neo_requires})
    # IDF 6.x: the `driver` component was reorganized into esp_driver_* components.
    # REQUIRES may not propagate the include dirs (driver/gpio.h, driver/rmt_tx.h,
    # hal/rmt_types.h, uc2_hal/uc2_time.h) to this component's compile command, so
    # add them explicitly.
    target_include_directories(__idf_neopixel PRIVATE
        ${IDF_PATH}/components/esp_driver_gpio/include
        ${IDF_PATH}/components/esp_driver_rmt/include
        ${IDF_PATH}/components/esp_hal_rmt/include
        ${IDF_PATH}/components/esp_timer/include
        ${IDF_PATH}/components/esp_system/include
        ${CMAKE_CURRENT_LIST_DIR}/../uc2_hal/include
        ${CMAKE_CURRENT_LIST_DIR}/..)
    # Link order fix: the IDF build system links system components (esp_driver_rmt)
    # BEFORE user components (neopixel), so the RMT objects are not pulled in when
    # neopixel references them. Force the RMT library to be linked with --whole-archive
    # via target_link_libraries (adds the lib to the link line after neopixel).
    # Link order fix: the IDF build system links system components (esp_driver_rmt,
    # esp_hal_rmt, esp_driver_dma) BEFORE user components (neopixel), so the RMT/DMA
    # objects are not pulled in when neopixel references them. Use --start-group/
    # --end-group to make the linker process these libs repeatedly after neopixel.
    target_link_libraries(__idf_neopixel PRIVATE ${_uc2_neo_link})
else()
    idf_component_register(SRCS "" INCLUDE_DIRS ".")
endif()
