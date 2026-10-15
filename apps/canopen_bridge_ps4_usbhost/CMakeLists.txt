cmake_minimum_required(VERSION 3.16.0)
# T2.10: canopen_bridge_ps4_usbhost — DS4-over-USB-OTG -> CANopen bridge (XIAO ESP32-S3).
# Shared components from top-level components/ (uc2_core/serial/canopen/...). The joystick
# sources live in uc2_joystick, gated by UC2_JOYSTICK_USBHOST_PROVIDER.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components"
                         "${CMAKE_CURRENT_LIST_DIR}/components")
# -DESP32S3_MODEL_XIAO: board model (bare #define gating code across shared components).
# -DJOYSTICK_USBHOST_PROVIDER: bare #define gating the DS4 USB-host provider. Must be a
# global CXX flag (not just the app PinConfig.h shadow): JoystickUsbHost.cpp never includes
# PinConfig.h, and uc2_joystick compiles against the shared main/PinConfig.h (the app-local
# shadow is only on uc2_app's include path) — so the macro would be undefined there and both
# joystick sources would compile to empty objects -> undefined JoystickUsbHost::begin at link.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DESP32S3_MODEL_XIAO=1 -DJOYSTICK_USBHOST_PROVIDER=1")
# UC2_APP_MAIN_DIR: app-local main/ holds the canopen_bridge_ps4_usbhost PinConfig.h;
# uc2_modinc.cmake prepends it so it shadows the shared main/PinConfig.h (laser).
set(ENV{UC2_APP_MAIN_DIR} "${CMAKE_CURRENT_LIST_DIR}/main")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(canopen_bridge_ps4_usbhost)
