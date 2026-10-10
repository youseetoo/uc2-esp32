cmake_minimum_required(VERSION 3.16.0)
# T1.2 acceptance: scratch app that REQUIRES all five moved components.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../components")
# Mirror the PIO reference envs (UC2_canopen_*_xiao): the S3 boards in this
# repo are XIAO variants, where dac_module compiles as an empty TU. The
# legacy SENS/RTC-IO DAC register code (driver/dac.h, soc/sens_reg.h) exists
# only for the original ESP32/S2 — the S3 never had it in IDF 4.4 either.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DESP32S3_MODEL_XIAO=1")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(scratch_t12)
