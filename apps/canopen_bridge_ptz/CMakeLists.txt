cmake_minimum_required(VERSION 3.16.0)
# T2.8: canopen_bridge_ptz — RS-485 Pelco-D PTZ keyboard -> CANopen bridge (XIAO ESP32-S3).
# Shared components from top-level components/ (uc2_core/serial/canopen/...). The PTZ
# sources live in uc2_canopen (ptz/), gated by UC2_PTZ_KEYBOARD_CONTROLLER.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components"
                         "${CMAKE_CURRENT_LIST_DIR}/components")
# -DESP32S3_MODEL_XIAO: board model (bare #define gating code across shared components).
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DESP32S3_MODEL_XIAO=1")
# UC2_APP_MAIN_DIR: app-local main/ holds the canopen_bridge_ptz PinConfig.h;
# uc2_modinc.cmake prepends it so it shadows the shared main/PinConfig.h (laser).
set(ENV{UC2_APP_MAIN_DIR} "${CMAKE_CURRENT_LIST_DIR}/main")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(canopen_bridge_ptz)
