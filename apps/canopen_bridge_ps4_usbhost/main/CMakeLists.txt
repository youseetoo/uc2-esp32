# T2.10: canopen_bridge_ps4_usbhost app entry point.
# main.c is a thin shim: the real logic lives in components/uc2_app/main.cpp,
# compiled by uc2_app (T1.4) and gated by the same #ifdefs as the PIO build.
# The DS4 USB-host/router sources are compiled by uc2_joystick, gated by
# CONFIG_UC2_JOYSTICK_USBHOST_PROVIDER (see components/uc2_joystick/CMakeLists.txt).
idf_component_register(SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES uc2_app uc2_joystick)
# Link group to resolve the uc2_app <-> uc2_joystick <-> uc2_canopen cycle
# (DeviceRouter references JoystickRouter when JOYSTICK_USBHOST_PROVIDER is set;
# same pattern as T2.6/T2.9).
target_link_libraries(__idf_main PRIVATE
    "LINKER:--start-group"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_app/libuc2_app.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_joystick/libuc2_joystick.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_canopen/libuc2_canopen.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_core/libuc2_core.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_serial/libuc2_serial.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_hal/libuc2_hal.a"
    # usb_host / esp_usb_host_hid sit BEFORE libuc2_joystick.a in IDF's natural
    # order, but JoystickUsbHost references usb_host_install / hid_host_install —
    # so they must be in the group too (iterative resolution, order-agnostic).
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/usb_host/libusb_host.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/esp_usb_host_hid/libesp_usb_host_hid.a"
    "LINKER:--end-group")
