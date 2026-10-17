# T5.4: app component — main.c entry (app_main -> uc2_app_start()). The real app
# lives in the uc2_app component; the motor stack (uc2_motor + FAS/TMC/encoder
# leaves) is pulled in via the --start-group in the root CMakeLists. main.c only
# needs uc2_app + uc2_motor.
idf_component_register(SRCS "main.c" INCLUDE_DIRS "."
    REQUIRES uc2_app uc2_core uc2_serial uc2_io uc2_periph uc2_motor uc2_canopen
             uc2_hal cjson dac_module driver esp_driver_ledc esp_driver_rmt
             esp_driver_pcnt esp_driver_tsens nvs_flash app_update)
# PinConfig.h: main/ holds the converted (uc2_hal.h) PinConfig.
target_include_directories(${COMPONENT_LIB} PRIVATE ${CMAKE_SOURCE_DIR}/../main)
