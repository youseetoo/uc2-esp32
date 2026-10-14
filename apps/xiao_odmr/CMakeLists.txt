cmake_minimum_required(VERSION 3.16.0)
# T2.9: xiao_odmr — XIAO ESP32S3 ODMR frame (motor + laser + LED + digIn + heat).
# Shared components from top-level components/ (uc2_core/serial/canopen/io/periph/led/motor).
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components"
                         "${CMAKE_CURRENT_LIST_DIR}/components")
# CXX_FLAGS: motor stack #ifdefs (uc2_motor gates on these at source level).
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DESP32S3_MODEL_XIAO=1 -DMOTOR_CONTROLLER -DHOME_MOTOR -DUSE_ACCELSTEP -DLINEAR_ENCODER_CONTROLLER -DSTAGE_SCAN")
# UC2_APP_MAIN_DIR: app-local main/ holds the xiao_odmr PinConfig.h; uc2_modinc.cmake
# prepends it so it shadows the shared main/PinConfig.h (laser) for ALL components.
set(ENV{UC2_APP_MAIN_DIR} "${CMAKE_CURRENT_LIST_DIR}/main")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(xiao_odmr)
