cmake_minimum_required(VERSION 3.16.0)
# T2.6: canopen_slave_accelmotor — single-axis X stage-motor CANopen slave
# (XIAO ESP32-S3). One CANopen node (CAN_ID_MOT_X) drives an AccelStepper
# stepper (TMC2209 over UART) with linear-encoder feedback + homing.
# Shared components live in the top-level components/ dir (uc2_core/serial/
# canopen/io/periph/motor).
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components"
                         "${CMAKE_CURRENT_LIST_DIR}/components")
# -DESP32S3_MODEL_XIAO: board model (bare #define gating code across shared components).
# MOTOR/HOME/USE_ACCELSTEP/TMC/ENCODER/LINEAR_ENCODER: the motor stack (raw-IDF port in
# uc2_motor, compiled under the matching CONFIG_UC2_* flags). Applied globally via CXX_FLAGS
# because IDF component targets reject external target_compile_definitions.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DESP32S3_MODEL_XIAO=1 -DMOTOR_CONTROLLER -DHOME_MOTOR -DUSE_ACCELSTEP -DTMC_CONTROLLER -DENCODER_CONTROLLER -DLINEAR_ENCODER_CONTROLLER")
# UC2_APP_MAIN_DIR: app-local main/ holds the canopen_slave_accelmotor PinConfig.h.
set(ENV{UC2_APP_MAIN_DIR} "${CMAKE_CURRENT_LIST_DIR}/main")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(canopen_slave_accelmotor)
