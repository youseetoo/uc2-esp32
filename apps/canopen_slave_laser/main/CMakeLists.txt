# T1.5: app component — main.c entry (app_main -> uc2_app_start()).
# T1.9: controllers now live in the shared components (uc2_core/uc2_serial/
# uc2_canopen/uc2_io/uc2_periph); the app-local uc2_modules is gone.
idf_component_register(SRCS "main.c" INCLUDE_DIRS "."
    REQUIRES uc2_app uc2_core uc2_serial uc2_canopen uc2_io uc2_periph
             uc2_hal canopen_node uc2_od cjson dac_module
             esp_usb_host_hid driver esp_driver_ledc
             esp_driver_tsens nvs_flash app_update)
# PinConfig.h: main/ holds the converted (uc2_hal.h) PinConfig until T2.11.
target_include_directories(${COMPONENT_LIB} PRIVATE ${CMAKE_SOURCE_DIR}/../main)
