cmake_minimum_required(VERSION 3.16.0)
# T2.5: canopen_slave_galvo — galvo scanner CANopen slave (XIAO ESP32-S3).
# Single CANopen node (CAN_ID_GALVO_0) drives the MCP4822 dual 12-bit SPI DAC
# galvo (high-speed RMT 3-trigger raster/line scanner) for laser projection.
# Shared components from top-level components/ (uc2_core/serial/canopen/io/periph).
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components"
                         "${CMAKE_CURRENT_LIST_DIR}/components")
# -DESP32S3_MODEL_XIAO: board model (bare #define gating code across shared components).
# GALVO_CONTROLLER: enable the galvo scanner (raw-IDF port in uc2_periph/scanner,
# compiled by uc2_periph under CONFIG_UC2_GALVO_CONTROLLER). Applied globally via
# CXX_FLAGS because IDF component targets reject external target_compile_definitions
# ("not built by this project"). All module code is C++.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DESP32S3_MODEL_XIAO=1 -DGALVO_CONTROLLER")
# UC2_APP_MAIN_DIR: app-local main/ holds the canopen_slave_galvo PinConfig.h;
# uc2_modinc.cmake prepends it so it shadows the shared main/PinConfig.h (laser).
set(ENV{UC2_APP_MAIN_DIR} "${CMAKE_CURRENT_LIST_DIR}/main")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(canopen_slave_galvo)
