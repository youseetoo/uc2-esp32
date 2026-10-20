# T1.9: uc2_canopen — CANopen module, routing table, device router, OTA trio,
# gpio_can slave (per-controller split). Gating per component-layout.md §3.
include(${CMAKE_CURRENT_LIST_DIR}/../uc2_modinc.cmake)
set(srcs
    "canopen/RoutingTable.cpp"
    "canopen/DeviceRouter.cpp")
if(CONFIG_UC2_CAN_CONTROLLER_CANOPEN)
    list(APPEND srcs
        "canopen/CANopenModule.cpp"
        "canopen/CanOpenOTA.cpp"
        "canopen/CanOpenOTAStreaming.cpp"
        "canopen/OtaBinaryReceive.cpp")
endif()
if(CONFIG_UC2_GPIO_CAN_SLAVE_CONTROLLER)
    list(APPEND srcs "gpio_can/GpioCanSlave.cpp")
endif()
if(CONFIG_UC2_PTZ_KEYBOARD_CONTROLLER)
    list(APPEND srcs
        "ptz/PtzKeyboard.cpp"
        "ptz/PtzRouter.cpp"
        "ptz/PelcoParser.cpp")
endif()
idf_component_register(SRCS ${srcs}
    INCLUDE_DIRS ${UC2_MODULE_INCLUDE_DIRS}
    REQUIRES uc2_core uc2_hal cjson nvs_flash driver app_update esp_driver_ledc
             tmcstepper accelstepper esp32_encoder
             esp_driver_tsens canopen_node uc2_od dac_module usb_host esp_usb_host_hid
              esp_driver_spi uc2_motor uc2_axis)
