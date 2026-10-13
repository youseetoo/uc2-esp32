# T2.1: xiao_ledservo app component — main.c entry (app_main -> uc2_app_start()).
idf_component_register(SRCS "main.c" INCLUDE_DIRS "."
    REQUIRES uc2_app uc2_core uc2_serial uc2_canopen uc2_io uc2_periph uc2_led
             uc2_hal canopen_node uc2_od cjson dac_module
             esp_usb_host_hid driver esp_driver_ledc
             esp_driver_tsens nvs_flash app_update)
# PinConfig.h: app-local main/ holds the ledservo (uc2_hal.h) PinConfig; shadows the
# shared main/ for this app's whole build (via -I main/ in the top-level CMakeLists).
target_include_directories(${COMPONENT_LIB} PRIVATE ${CMAKE_SOURCE_DIR}/main)
