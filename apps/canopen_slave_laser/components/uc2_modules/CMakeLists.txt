# T1.5: module TUs from main/src/ (temporary — T1.6+ moves them into per-controller
# components per refs/component-layout.md §1). Gating mirrors main/CMakeLists.txt.
set(UC2_MAIN "${CMAKE_CURRENT_LIST_DIR}/../../../../main")

set(srcs
    # App entry (co-located with controllers for intra-archive resolution)
    "${CMAKE_CURRENT_LIST_DIR}/../../../../components/uc2_app/main.cpp"
    # Always-compiled core chain
    "${UC2_MAIN}/src/buzzer/BuzzerController.cpp"
    "${UC2_MAIN}/src/config/NVSConfig.cpp"
    "${UC2_MAIN}/src/config/ConfigController.cpp"
    "${UC2_MAIN}/src/encoder/InterruptController.cpp"
    "${UC2_MAIN}/src/state/State.cpp"
    "${UC2_MAIN}/src/qid/QidRegistry.cpp"
    "${UC2_MAIN}/src/serial/SerialProcess.cpp"
    # Routing table + device router — always compiled
    "${UC2_MAIN}/src/canopen/RoutingTable.cpp"
    "${UC2_MAIN}/src/canopen/DeviceRouter.cpp"
    # I2cBridge: internally gated by #ifdef I2C_BRIDGE_CONTROLLER
    "${UC2_MAIN}/src/i2c/I2cBridge.cpp"
    # i2c_slave_dial: unconditional
    "${UC2_MAIN}/src/i2c/i2c_slave_dial.cpp")

if(CONFIG_UC2_DIGITAL_IN_CONTROLLER)
    list(APPEND srcs "${UC2_MAIN}/src/digitalin/DigitalInController.cpp")
endif()
if(CONFIG_UC2_DIGITAL_OUT_CONTROLLER)
    list(APPEND srcs "${UC2_MAIN}/src/digitalout/DigitalOutController.cpp")
endif()
if(CONFIG_UC2_DIAL_CONTROLLER)
    list(APPEND srcs "${UC2_MAIN}/src/dial/DialController.cpp")
endif()
if(CONFIG_UC2_LASER_CONTROLLER)
    list(APPEND srcs "${UC2_MAIN}/src/laser/LaserController.cpp")
endif()
if(CONFIG_UC2_MESSAGE_CONTROLLER)
    list(APPEND srcs "${UC2_MAIN}/src/message/MessageController.cpp")
endif()
if(CONFIG_UC2_TMP102_CONTROLLER OR CONFIG_UC2_FAN_CONTROLLER)
    list(APPEND srcs "${UC2_MAIN}/src/tmp102/Tmp102Controller.cpp")
endif()
if(CONFIG_UC2_FAN_CONTROLLER)
    list(APPEND srcs "${UC2_MAIN}/src/fan/FanController.cpp")
endif()
if(CONFIG_UC2_CAN_CONTROLLER_CANOPEN)
    list(APPEND srcs "${UC2_MAIN}/src/canopen/CANopenModule.cpp"
                      "${UC2_MAIN}/src/canopen/CanOpenOTA.cpp"
                      "${UC2_MAIN}/src/canopen/CanOpenOTAStreaming.cpp"
                      "${UC2_MAIN}/src/canopen/OtaBinaryReceive.cpp")
endif()
if(CONFIG_UC2_GPIO_CAN_SLAVE_CONTROLLER)
    list(APPEND srcs "${UC2_MAIN}/src/gpio_can/GpioCanSlave.cpp")
endif()

idf_component_register(SRCS ${srcs}
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
                                   "${UC2_MAIN}/src/serial"
                       REQUIRES uc2_app canopen_node uc2_od cjson usb_host
                               esp_usb_host_hid dac_module driver esp_driver_ledc
                               esp_driver_tsens nvs_flash uc2_hal uc2_core app_update)
# Board combo: I2C_BRIDGE_CONTROLLER + CAN_CONTROLLER_CANOPEN (laser slave)
target_compile_definitions(${COMPONENT_LIB} PRIVATE I2C_BRIDGE_CONTROLLER CAN_CONTROLLER_CANOPEN)
# Board PinConfig.h (main.cpp includes it) — main/ holds the converted version:
target_include_directories(${COMPONENT_LIB} PRIVATE ${CMAKE_SOURCE_DIR}/../main)
