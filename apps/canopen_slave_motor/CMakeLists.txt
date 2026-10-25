cmake_minimum_required(VERSION 3.16.0)
# T2.7: canopen_slave_motor — multi-axis (A/X/Y/Z) stage-motor CANopen slave
# (XIAO ESP32-S3). FAS (FastAccelStepper, RMT backend) steppers + TMC drivers +
# linear-encoder (PCNT) feedback + homing + axis control. One CANopen node
# (CAN_ID_MOT_X) drives the X axis; the other axes are allocated but off-board.
# Shared components live in the top-level components/ dir (uc2_core/serial/
# canopen/io/periph/motor + fastaccelstepper).
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components"
                         "${CMAKE_CURRENT_LIST_DIR}/components")
# -DESP32S3_MODEL_XIAO: board model (bare #define gating code across shared components).
# MOTOR/HOME/USE_FASTACCEL/TMC/LINEAR_ENCODER/AXIS/USE_PCNT: the motor stack (raw-IDF
# port in uc2_motor, compiled under the matching CONFIG_UC2_* flags). Applied globally via
# CXX_FLAGS because IDF component targets reject external target_compile_definitions.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DESP32S3_MODEL_XIAO=1 -DMOTOR_CONTROLLER -DHOME_MOTOR -DUSE_FASTACCEL -DTMC_CONTROLLER -DLINEAR_ENCODER_CONTROLLER -DAXIS_CONTROLLER -DUSE_PCNT_COUNTER")
# T2.7: 5-axis variant builds (mirrors the PIO build-frame loop). UC2_CAN_AXIS
# (X/Y/Z/A) selects the CAN node id consumed by main/pinconfig.c
# (.CAN_ID_CURRENT = CAN_AXIS_ID). T4.3: numeric values (the PIO-era symbolic
# CAN_ID_MOT_<AXIS> tokens were PinConfig STRUCT FIELDS — unusable as a C
# initializer). Values match pinconfig_defaults.h (CAN_ID_MOT_A..Z = 10..13).
# Default (empty/absent) = X axis (11).
if(DEFINED ENV{UC2_CAN_AXIS} AND NOT "$ENV{UC2_CAN_AXIS}" STREQUAL "")
    string(TOUPPER "$ENV{UC2_CAN_AXIS}" _uc2_axis)
    if(_uc2_axis STREQUAL "A")
        set(_uc2_can_axis_id 10)
    elseif(_uc2_axis STREQUAL "Y")
        set(_uc2_can_axis_id 12)
    elseif(_uc2_axis STREQUAL "Z")
        set(_uc2_can_axis_id 13)
    else()
        set(_uc2_can_axis_id 11)  # X (default) or unknown -> X
    endif()
    message(STATUS "T2.7: canopen_slave_motor axis variant ${_uc2_axis} (CAN_AXIS_ID=${_uc2_can_axis_id})")
else()
    set(_uc2_can_axis_id 11)
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DCAN_AXIS_ID=${_uc2_can_axis_id}")
# UC2_APP_MAIN_DIR: app-local main/ holds the canopen_slave_motor PinConfig.h.
set(ENV{UC2_APP_MAIN_DIR} "${CMAKE_CURRENT_LIST_DIR}/main")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(canopen_slave_motor)
