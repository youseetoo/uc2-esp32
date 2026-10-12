cmake_minimum_required(VERSION 3.16.0)
# T1.5: first real IDF app — CANopen slave laser node (XIAO ESP32S3).
# T1.9: controllers now in shared components (uc2_core/serial/canopen/io/periph).
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components"
                         "${CMAKE_CURRENT_LIST_DIR}/components")
# -DESP32S3_MODEL_XIAO: board model.
# I2C_BRIDGE_CONTROLLER + CAN_CONTROLLER_CANOPEN: laser board feature combo — bare
# #defines gating code across the shared module components (used in .cpp AND .h in
# uc2_core/serial/canopen/periph). Applied globally via CXX_FLAGS because IDF
# component targets reject external target_compile_definitions ("not built by this
# project"). All module code is C++, so CXX_FLAGS covers it.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DESP32S3_MODEL_XIAO=1 -DI2C_BRIDGE_CONTROLLER -DCAN_CONTROLLER_CANOPEN")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(canopen_slave_laser)
