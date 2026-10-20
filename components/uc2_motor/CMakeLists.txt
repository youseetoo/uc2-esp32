# T2.6: uc2_motor — stepper / encoder / axis / home / pid / tmc, raw-IDF port of
# main/src/{motor,home,axis,tmc,pid}. Compiled by canopen_slave_accelmotor (and any
# app that REQUIRES uc2_motor). The real code is gated by the #ifdef flags the app
# applies via CXX_FLAGS: MOTOR_CONTROLLER, HOME_MOTOR, USE_ACCELSTEP, TMC_CONTROLLER,
# ENCODER_CONTROLLER, LINEAR_ENCODER_CONTROLLER (an app that links uc2_motor without
# these just gets empty translation units).
include(${CMAKE_CURRENT_LIST_DIR}/../uc2_modinc.cmake)
set(srcs
    "home/HomeMotor.cpp"
    "motor/AccelStep.cpp"
    "motor/FAccelStep.cpp"
    "motor/FocusMotor.c"
    "motor/FocusScan.cpp"
    "motor/axis_motor_c.cpp"
    "motor/focusmotor_qid_c.cpp"
    "motor/MotorGamePad.cpp"
    "motor/MotorJsonParser.cpp"
    "motor/StageScan.cpp"
    "pid/PidController.c"
    "tmc/TMCController.cpp")
idf_component_register(SRCS ${srcs}
    INCLUDE_DIRS ${UC2_MODULE_INCLUDE_DIRS}
    REQUIRES uc2_core uc2_hal uc2_periph uc2_io uc2_canopen uc2_serial uc2_app uc2_led tmcstepper accelstepper esp32_encoder fastaccelstepper
             cjson tmcstepper accelstepper esp32_encoder fastaccelstepper driver esp_driver_rmt nvs_flash
             esp_timer esp_driver_gpio tca9555)
