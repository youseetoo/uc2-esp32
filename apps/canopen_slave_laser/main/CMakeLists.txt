# T1.5: app component — main.c + uc2_modules (module TUs from main/src/).
# PinConfig for this board: boards/canopen_slave_laser/ (component-layout.md §2).
idf_component_register(SRCS "main.c" INCLUDE_DIRS "."
                       REQUIRES uc2_app uc2_core uc2_hal uc2_modules
                               canopen_node uc2_od cjson dac_module
                               esp_usb_host_hid driver esp_driver_ledc
                               esp_driver_tsens nvs_flash app_update)
# PinConfig.h: main/ holds the converted (uc2_hal.h) PinConfig until T2.11
# reworks boards/ to drop Arduino.h.
target_include_directories(${COMPONENT_LIB} PRIVATE ${CMAKE_SOURCE_DIR}/../main)
