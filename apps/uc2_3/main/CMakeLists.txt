# T3.4: app component — main.c entry (app_main -> uc2_app_start()).
# Controllers live in the shared components (uc2_core/uc2_serial/uc2_bt/uc2_io/
# uc2_periph/uc2_motor); this is a thin shim. uc2_bt (BLE HID host) + uc2_motor
# replace the CAN components the S3 slave apps carry.
idf_component_register(SRCS "main.c" INCLUDE_DIRS "."
    REQUIRES uc2_app uc2_core uc2_serial uc2_bt uc2_io uc2_periph uc2_motor
             uc2_hal cjson dac_module driver esp_driver_ledc esp_driver_tsens
             nvs_flash app_update)
# PinConfig.h: main/ holds the converted (uc2_hal.h) PinConfig.
target_include_directories(${COMPONENT_LIB} PRIVATE ${CMAKE_SOURCE_DIR}/../main)
