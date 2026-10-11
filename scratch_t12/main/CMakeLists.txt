set(UC2_MAIN M:/repos/uc2-esp32/main)

idf_component_register(SRCS "main.cpp"
                       "${UC2_MAIN}/src/buzzer/BuzzerController.cpp"
                       "${UC2_MAIN}/src/config/NVSConfig.cpp"
                       "${UC2_MAIN}/src/i2c/I2cBridge.cpp"
                       INCLUDE_DIRS "${UC2_MAIN}"
                                    "${UC2_MAIN}/src/buzzer"
                                    "${UC2_MAIN}/src/config"
                                    "${UC2_MAIN}/src/i2c"
                       REQUIRES canopen_node uc2_od cjson usb_host esp_usb_host_hid
                                 dac_module driver esp_driver_ledc nvs_flash uc2_hal)
# UC2_canopen_slave_gpio board combo (I2C_BRIDGE_CONTROLLER always pairs with CAN_CONTROLLER_CANOPEN)
target_compile_definitions(${COMPONENT_LIB} PRIVATE I2C_BRIDGE_CONTROLLER CAN_CONTROLLER_CANOPEN)
