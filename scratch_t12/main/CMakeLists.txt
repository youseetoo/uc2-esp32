set(UC2_MAIN M:/repos/uc2-esp32/main)

idf_component_register(SRCS "main.cpp"
                       "${UC2_MAIN}/src/buzzer/BuzzerController.cpp"
                        "${UC2_MAIN}/src/config/NVSConfig.cpp"
                        "${UC2_MAIN}/src/config/ConfigController.cpp"
                        "${UC2_MAIN}/src/encoder/InterruptController.cpp"
                        "${UC2_MAIN}/src/digitalin/DigitalInController.cpp"
                        "${UC2_MAIN}/src/digitalout/DigitalOutController.cpp"
                        "${UC2_MAIN}/src/fan/FanController.cpp"
                        "${UC2_MAIN}/src/i2c/I2cBridge.cpp"
                        "${UC2_MAIN}/src/i2c/i2c_slave_dial.cpp"
                        "${UC2_MAIN}/src/tmp102/Tmp102Controller.cpp"
                         "${UC2_MAIN}/src/dial/DialController.cpp"
                          "${UC2_MAIN}/src/message/MessageController.cpp"
                          "${UC2_MAIN}/src/qid/QidRegistry.cpp"
                           "${UC2_MAIN}/src/state/State.cpp"
                            "${UC2_MAIN}/src/laser/LaserController.cpp"
                            "${UC2_MAIN}/src/canopen/RoutingTable.cpp"
                            "${UC2_MAIN}/src/canopen/OtaBinaryReceive.cpp"
                            "${UC2_MAIN}/src/canopen/CanOpenOTAStreaming.cpp"
                            "${UC2_MAIN}/src/canopen/CanOpenOTA.cpp"
                         INCLUDE_DIRS "${UC2_MAIN}"
                                     "${UC2_MAIN}/src/buzzer"
                                     "${UC2_MAIN}/src/config"
                                      "${UC2_MAIN}/src/digitalin"
                                      "${UC2_MAIN}/src/digitalout"
                                     "${UC2_MAIN}/src/fan"
                                     "${UC2_MAIN}/src/i2c"
                                     "${UC2_MAIN}/src/tmp102"
                                      "${UC2_MAIN}/src/dial"
                                       "${UC2_MAIN}/src/encoder"
                                         "${UC2_MAIN}/src/message"
                                         "${UC2_MAIN}/src/qid"
                                          "${UC2_MAIN}/src/state"
                                          "${UC2_MAIN}/src/laser"
                                          "${UC2_MAIN}/src/canopen"
                        REQUIRES canopen_node uc2_od cjson usb_host esp_usb_host_hid
                                 dac_module driver esp_driver_ledc esp_driver_tsens
                                 nvs_flash uc2_hal app_update)
# UC2_canopen_slave_gpio board combo (I2C_BRIDGE_CONTROLLER always pairs with CAN_CONTROLLER_CANOPEN)
target_compile_definitions(${COMPONENT_LIB} PRIVATE I2C_BRIDGE_CONTROLLER CAN_CONTROLLER_CANOPEN)
# Note: DialController.cpp's SdoEmit/RoutingTable path is guarded inside the
# file by (UC2_SDOEMIT_AVAILABLE || ARDUINO), so it compiles without the
# (unconverted) CANopen emit layer in this IDF scratch build. When
# SdoEmit/CANopenModule are converted, define UC2_SDOEMIT_AVAILABLE (here or
# in main/CMakeLists.txt) to re-enable the path.
