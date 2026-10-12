cmake_minimum_required(VERSION 3.16.0)
# T1.5: first real IDF app — CANopen slave laser node (XIAO ESP32S3).
# Components: repo components/ + local uc2_modules (module TUs from main/src/,
# until T1.6+ moves them into per-controller components).
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components"
                         "${CMAKE_CURRENT_LIST_DIR}/components")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DESP32S3_MODEL_XIAO=1")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(canopen_slave_laser)
