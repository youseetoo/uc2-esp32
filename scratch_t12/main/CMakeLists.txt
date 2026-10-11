set(UC2_MAIN M:/repos/uc2-esp32/main)

idf_component_register(SRCS "main.cpp"
                       "${UC2_MAIN}/src/buzzer/BuzzerController.cpp"
                       "${UC2_MAIN}/src/config/NVSConfig.cpp"
                       INCLUDE_DIRS "${UC2_MAIN}"
                                    "${UC2_MAIN}/src/buzzer"
                                    "${UC2_MAIN}/src/config"
                       REQUIRES canopen_node uc2_od cjson usb_host esp_usb_host_hid
                                 dac_module driver esp_driver_ledc nvs_flash uc2_hal)
