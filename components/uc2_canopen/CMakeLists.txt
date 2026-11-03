# T1.9: uc2_canopen — CANopen module, routing table, device router, OTA trio,
# gpio_can slave (per-controller split). Gating per component-layout.md §3.
include(${CMAKE_CURRENT_LIST_DIR}/../uc2_modinc.cmake)
set(srcs
    "canopen/RoutingTable.c"
    # T4.4: split of the old DeviceRouter.cpp (behavior-identical; the dispatch
    # chain is C — device_router.c, step 4f — per-command handlers in
    # serial_dispatch.c, C since step 4g).
    "canopen/device_router.c"
    "canopen/serial_dispatch.c")
if(CONFIG_UC2_GPIO_CAN_SLAVE_CONTROLLER OR CONFIG_UC2_PTZ_KEYBOARD_CONTROLLER
   OR CONFIG_UC2_JOYSTICK_USBHOST_PROVIDER)
    # T4.4 step 4g: C++ island for the still-C++ bridges (GpioCanSlave /
    # PtzKeyboard / JoystickRouter shims called from serial_dispatch.c /
    # device_router.c). Sections are #ifdef'd on the per-app PinConfig
    # macros, so it compiles to an empty object where no bridge is present
    # (same pattern as canopen_tpdo_galvo.cpp). Drop once 4h + 4i land.
    list(APPEND srcs "canopen/serial_dispatch_island.cpp")
endif()
if(CONFIG_UC2_CAN_CONTROLLER_CANOPEN)
    list(APPEND srcs
        # T4.4: split of the old CANopenModule.cpp (behavior-identical; shared
        # state in canopen_module_priv.h). module core is C (T4.4 step 4e,
        # was canopen_module.cpp); sdo is C (T4.4 step 4b).
        "canopen/canopen_module.c"
        "canopen/canopen_sdo.c"
        # T4.4 step 4c: TPDO C-ified (was canopen_tpdo.cpp; C API
        # canopen_tpdo_sync_* + CANopenModule:: shims in CANopenModule.h).
        "canopen/canopen_tpdo.c"
        # T4.4 step 4c: C++ island for the galvo TPDO sync (GalvoController.h
        # is C++-only). Body is #ifdef GALVO_CONTROLLER, so it compiles to an
        # empty object on non-galvo apps.
        "canopen/canopen_tpdo_galvo.cpp"
        "canopen/canopen_ota.c")
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
