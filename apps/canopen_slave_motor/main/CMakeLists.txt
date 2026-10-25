# T2.7: app entry shim. The real app (main.cpp + all gated sources) lives in
# the uc2_app component; main.c here only calls uc2_app_start(). uc2_motor
# compiles the FAS (FastAccelStepper) stepper + home/axis/tmc sources.
idf_component_register(SRCS "main.c" "pinconfig.c"
                       INCLUDE_DIRS "."
                       REQUIRES uc2_app uc2_motor)
# T4.3: pinconfig.c re-initializes fields already set by PINCONFIG_DEFAULTS_FIELDS
# (board overrides after the defaults); GCC 15 (gnu23 + -Wextra + -Werror) promotes
# -Woverride-init to an error.
target_compile_options(__idf_main PRIVATE -Wno-override-init)

# T2.7: link-order fix for the uc2_app <-> uc2_motor <-> uc2_canopen cycle
# (same --start-group technique as the accelmotor T2.6 + neopixel RMT T2.1).
# Group the archives so the linker iterates them; include fastaccelstepper
# (the FAS RMT backend) + the driver deps (rmt/pcnt) that FAS / esp32_encoder
# need. Without the group the RMT/PCNT objects are not pulled in (system
# components link before user ones).
target_link_libraries(__idf_main PRIVATE
    "LINKER:--start-group"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_app/libuc2_app.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_motor/libuc2_motor.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_canopen/libuc2_canopen.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_axis/libuc2_axis.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_io/libuc2_io.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_led/libuc2_led.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_serial/libuc2_serial.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_core/libuc2_core.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_hal/libuc2_hal.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/tmcstepper/libtmcstepper.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/esp32_encoder/libesp32_encoder.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/accelstepper/libaccelstepper.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/fastaccelstepper/libfastaccelstepper.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/esp_driver_pcnt/libesp_driver_pcnt.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/esp_hal_pcnt/libesp_hal_pcnt.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/esp_driver_rmt/libesp_driver_rmt.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/esp_hal_rmt/libesp_hal_rmt.a"
    # T4.3: pinConfig is DEFINED in libmain.a (pinconfig.c). The uc2_* libs
    # reference it, so libmain.a must be INSIDE the group: the uc2_* group
    # resolves among themselves and stops, so a pinConfig reference would
    # otherwise dangle against a later libmain.a (same pattern as canopen_slave_laser).
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/main/libmain.a"
    "LINKER:--end-group")
