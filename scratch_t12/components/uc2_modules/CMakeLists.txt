set(UC2_MAIN M:/repos/uc2-esp32/main)

# T1.4 scratch link-order fix: the per-env module TUs must link AFTER uc2_app,
# because uc2_app/main.cpp.obj (uc2_app_start) references them. If they stay in
# the app component (libmain.a, which also holds app_main/main.c), CMake places
# libmain.a BEFORE the libuc2_app.a that pulls in main.cpp.obj, so the linker
# never re-scans libmain.a for the controllers -> undefined refs.
#
# Registering them as their own component (uc2_modules) puts libuc2_modules.a
# after libuc2_app.a in the link line, so the controllers resolve. The source
# files still live in main/src (T2.12 moves them into per-controller components);
# only the CMake registration is split here.
#
# Source gating (refs/component-layout.md §3) mirrors the old main/CMakeLists.txt:
# a module TU compiles only when its UC2_* Kconfig feature is enabled
# (symbols live in components/uc2_core/Kconfig, sourced via uc2_app/Kconfig).
set(srcs
    # App entry (moved here from uc2_app so it shares an archive with the
    # controllers it references — intra-archive resolution avoids the
    # cross-archive multi-pass issue on this linker setup).
    "${CMAKE_CURRENT_LIST_DIR}/../../../components/uc2_app/main.cpp"
    # Always-compiled core chain (unconditional set in old main/CMakeLists.txt)
    "${UC2_MAIN}/src/buzzer/BuzzerController.cpp"
    "${UC2_MAIN}/src/config/NVSConfig.cpp"
    "${UC2_MAIN}/src/config/ConfigController.cpp"
    "${UC2_MAIN}/src/encoder/InterruptController.cpp"
    "${UC2_MAIN}/src/state/State.cpp"
    "${UC2_MAIN}/src/qid/QidRegistry.cpp"
    "${UC2_MAIN}/src/serial/SerialProcess.cpp"
    # Routing table + device router — always compiled (old CMakeLists)
    "${UC2_MAIN}/src/canopen/RoutingTable.cpp"
    "${UC2_MAIN}/src/canopen/DeviceRouter.cpp"
    # I2cBridge: internally gated by #ifdef I2C_BRIDGE_CONTROLLER (PinConfig-
    # gated); compiles to an empty TU when that is off.
    "${UC2_MAIN}/src/i2c/I2cBridge.cpp"
    # i2c_slave_dial: no UC2_* Kconfig symbol — compiled unconditionally, as before.
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
# Tmp102: old CMakeLists listed it under both FAN_CONTROLLER and TMP102_CONTROLLER
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
                        # uc2_app first: its include dir provides PinConfigDefault.h
                        # (main/PinConfig.h includes it), and it pins the link order
                        # (uc2_app links before uc2_modules -> controllers resolve
                        # main.cpp.obj's refs).
                        REQUIRES uc2_app canopen_node uc2_od cjson usb_host
                                esp_usb_host_hid dac_module driver esp_driver_ledc
                                esp_driver_tsens nvs_flash uc2_hal uc2_core app_update)
# UC2_canopen_slave_gpio board combo (I2C_BRIDGE_CONTROLLER always pairs with
# CAN_CONTROLLER_CANOPEN). Module TUs still gate on these bare -D defines
# internally; that flips per-file as each TU is ported (T2.x).
target_compile_definitions(${COMPONENT_LIB} PRIVATE I2C_BRIDGE_CONTROLLER CAN_CONTROLLER_CANOPEN)
