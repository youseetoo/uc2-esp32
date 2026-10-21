# T2.10: uc2_joystick — DS4-over-USB-OTG HID host + CANopen router.
# Raw-IDF port from main/src/joystick (per component-layout.md §1). Gating per
# main/CMakeLists.txt: both sources compile only under JOYSTICK_USBHOST_PROVIDER.
include(${CMAKE_CURRENT_LIST_DIR}/../uc2_modinc.cmake)
set(srcs "")
if(CONFIG_UC2_JOYSTICK_USBHOST_PROVIDER)
    list(APPEND srcs
        # T4.1: pure C (joystick_usbhost_* API); the router stays a C++ island
        # around the C++ CANopenModule::writeSDO_* calls.
        "joystick_usbhost.c"
        "JoystickRouter.cpp")
endif()
idf_component_register(SRCS ${srcs}
    INCLUDE_DIRS ${UC2_MODULE_INCLUDE_DIRS}
    REQUIRES uc2_core uc2_app uc2_canopen uc2_motor uc2_hal cjson nvs_flash driver
             usb_host esp_usb_host_hid)
#
# NOTE: the bare feature macro JOYSTICK_USBHOST_PROVIDER (the #ifdef guard in
# BOTH sources, and in uc2_app/main.cpp + uc2_canopen/DeviceRouter.cpp) is
# defined globally by the consuming app's top-level CMakeLists.txt
# (apps/canopen_bridge_ps4_usbhost/CMakeLists.txt) via CMAKE_CXX_FLAGS, exactly
# like -DESP32S3_MODEL_XIAO=1. It is NOT taken from PinConfig.h: JoystickUsbHost
# never includes PinConfig.h, and uc2_joystick compiles against the shared
# main/PinConfig.h (the app-local shadow is only on uc2_app's include path),
# which does not define the macro. Defining it here per-component would not
# reach uc2_app/uc2_canopen (they are this component's deps, not consumers).
