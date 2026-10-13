cmake_minimum_required(VERSION 3.16.0)
# T2.4: canopen_slave_led — illumination board CANopen slave (XIAO ESP32-S3).
# Single CANopen node (CAN_ID_LED_0, default 30) drives BOTH the NeoPixel ring
# stack (136 LEDs) and the high-power white LED PWM (logical laser 0), with
# NTC heat-sink thermal monitoring (rev. H).
# Shared components from top-level components/ (uc2_core/serial/canopen/io/periph/led).
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components"
                         "${CMAKE_CURRENT_LIST_DIR}/components")
# -DESP32S3_MODEL_XIAO: board model (bare #define gating code across shared components).
# LED_CONTROLLER + LASER_CONTROLLER + THERMAL_CONTROLLER + CAN feature combo:
# applied globally via CXX_FLAGS because IDF component targets reject external
# target_compile_definitions ("not built by this project"). All module code is C++.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DESP32S3_MODEL_XIAO=1 -DLED_CONTROLLER -DLASER_CONTROLLER -DTHERMAL_CONTROLLER")
# UC2_APP_MAIN_DIR: app-local main/ holds the canopen_slave_led PinConfig.h;
# uc2_modinc.cmake prepends it so it shadows the shared main/PinConfig.h (laser).
set(ENV{UC2_APP_MAIN_DIR} "${CMAKE_CURRENT_LIST_DIR}/main")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(canopen_slave_led)
