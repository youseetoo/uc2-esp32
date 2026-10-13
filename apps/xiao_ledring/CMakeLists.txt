cmake_minimum_required(VERSION 3.16.0)
# T2.1: xiao_ledring — XIAO ESP32S3 I2C-slave laser node (laser + digital-in +
# I2C-slave-laser + message). Shared components from top-level components/.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components"
                         "${CMAKE_CURRENT_LIST_DIR}/components")
# -DESP32S3_MODEL_XIAO: board model.
# -I main/: app-local PinConfig.h (UC2_3_Xiao_Slave_Laser) shadows the shared
#   main/PinConfig.h (laser) for ALL components (CMAKE_CXX_FLAGS -I precedes a
#   component's target_include_directories). Per-app PinConfig rework is T2.11.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DESP32S3_MODEL_XIAO=1 -I${CMAKE_CURRENT_LIST_DIR}/main")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(xiao_ledring)
