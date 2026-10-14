# T2.6: canopen_slave_accelmotor — single-axis X stage-motor CANopen slave
# (XIAO ESP32-S3).
#
# Feature set: base + UC2_MOTOR_CONTROLLER, UC2_HOME_MOTOR, UC2_USE_ACCELSTEP,
# UC2_TMC_CONTROLLER, UC2_LINE_ENCODER_CONTROLLER, UC2_AXIS_CONTROLLER.
#
# Layout mirrors apps/canopen_slave_led (T2.4): top-level CMakeLists,
# sdkconfig.defaults + config/partitions_4mb.csv, main/ (app entry + PinConfig).
cmake_minimum_required(VERSION 3.16)

# T2.1: per-app PinConfig shadow — prepended to the include dirs by
# uc2_modinc.cmake so the app's main/PinConfig.h wins for ALL components.
set(UC2_APP_MAIN_DIR "${CMAKE_CURRENT_LIST_DIR}/main")

# T1.3: extra component dirs for canopen_node / usb_hid + BT (same as galvo).
set(EXTRA_COMPONENT_DIRS
    "$ENV{IDF_PATH}/examples/common_components/protocol_components"
    "$ENV{IDF_PATH}/components/bt/host/bluedroid"
)

# Feature macros: applied globally (ALL components) so main.cpp's #ifdefs and
# the uc2_motor sources' internal #ifdefs see them. The app's PinConfig.h also
# defines them (guarded #ifndef) for TUs that include it.
idf_build_set_property(COMPILE_OPTIONS "-DESP32S3_MODEL_XIAO=1"
                       "-DMOTOR_CONTROLLER"
                       "-DHOME_MOTOR"
                       "-DUSE_ACCELSTEP"
                       "-DTMC_CONTROLLER"
                       "-DLINEAR_ENCODER_CONTROLLER"
                       "-DAXIS_CONTROLLER"
                       "-DDIGITAL_IN_CONTROLLER"
                       "-DMESSAGE_CONTROLLER"
                       "-DCAN_RECEIVE_MOTOR"
                       "-DCAN_BUS_ENABLED"
                       "-DCAN_CONTROLLER_CANOPEN"
                       "-DUSE_PCNT_COUNTER"
                       APPEND)

project(canopen_slave_accelmotor)
