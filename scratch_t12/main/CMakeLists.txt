set(UC2_MAIN M:/repos/uc2-esp32/main)

idf_component_register(SRCS "main.cpp"
                       "${UC2_MAIN}/src/buzzer/BuzzerController.cpp"
                       INCLUDE_DIRS "${UC2_MAIN}"
                                    "${UC2_MAIN}/src/buzzer"
                       REQUIRES canopen_node uc2_od cjson usb_host esp_usb_host_hid
                                dac_module driver uc2_hal)
