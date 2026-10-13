cmake_minimum_required(VERSION 3.16.0)
# T2.1: xiao_ledservo — XIAO ESP32S3 LED+servo node (laser + LED + digital-in + message).
# Shared components from top-level components/ (uc2_core/serial/canopen/io/periph/led).
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components"
                         "${CMAKE_CURRENT_LIST_DIR}/components")
# -DESP32S3_MODEL_XIAO: board model.
# -I main/: app-local PinConfig.h (UC2_ESP32S3_XIAO_LEDSERVO) shadows the shared
#   main/PinConfig.h (laser) for ALL components — CMAKE_CXX_FLAGS -I precedes a
#   component's target_include_directories in the compile line, so this dir wins.
#   (Per-app PinConfig rework lands in T2.11; this shadow is the T2.1 stopgap.)
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DESP32S3_MODEL_XIAO=1 -I${CMAKE_CURRENT_LIST_DIR}/main")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(xiao_ledservo)
