# T2.1: xiao_ledring app component — main.c entry (app_main -> uc2_app_start()).
# I2C-slave laser node: no uc2_led (no WS2812). i2c_slave driver for the slave path.
idf_component_register(SRCS "main.c" INCLUDE_DIRS "."
    REQUIRES uc2_app uc2_core uc2_serial uc2_canopen uc2_io uc2_periph
             uc2_hal uc2_od cjson dac_module
             esp_driver_i2c esp_driver_gpio driver esp_driver_ledc
             esp_driver_tsens nvs_flash app_update)
# PinConfig.h: app-local main/ holds the ledring (uc2_hal.h) PinConfig; shadows the
# shared main/ for this app's whole build (via -I main/ in the top-level CMakeLists).
target_include_directories(${COMPONENT_LIB} PRIVATE ${CMAKE_SOURCE_DIR}/main)
