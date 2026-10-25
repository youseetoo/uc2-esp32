# T2.6: app entry shim. The real app (main.cpp + all gated sources) lives in
# the uc2_app component; main.c here only calls uc2_app_start(). uc2_motor
# (the new component for T2.6) compiles the motor/home/axis/tmc/pid sources.
idf_component_register(SRCS "main.c" "pinconfig.c"
                       INCLUDE_DIRS "."
                       REQUIRES uc2_app uc2_motor)

# T4.3: pinconfig.c re-initializes fields already set by PINCONFIG_DEFAULTS_FIELDS
# (board overrides after the defaults); GCC 15 (gnu23 + -Wextra + -Werror) promotes
# -Woverride-init to an error.
target_compile_options(__idf_main PRIVATE -Wno-override-init)

# T2.6: link-order fix for the uc2_app <-> uc2_motor <-> uc2_canopen cycle.
# uc2_app's main.cpp (and uc2_canopen's CANopenModule) reference the
# motor/axis/home/tmc singletons defined in uc2_motor, while uc2_motor needs
# uc2_app's PinConfig/uc2_app.h. The default link order leaves uc2_motor's
# objects unpulled -> undefined AxisController::loop() etc. Group the three
# archives so the linker iterates them (same --start-group technique as the
# neopixel RMT fix in T2.1).
target_link_libraries(__idf_main PRIVATE
    "LINKER:--start-group"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_app/libuc2_app.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_motor/libuc2_motor.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_canopen/libuc2_canopen.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_periph/libuc2_periph.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_io/libuc2_io.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_led/libuc2_led.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_serial/libuc2_serial.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_core/libuc2_core.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_hal/libuc2_hal.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/tmcstepper/libtmcstepper.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/esp32_encoder/libesp32_encoder.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/accelstepper/libaccelstepper.a"
    # T2.6: system driver libs that esp32_encoder / uc2_motor / tmcstepper need
    # (the IDF build links system components before user ones, so without the
    # group the PCNT objects are not pulled in — same technique as neopixel RMT).
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/esp_driver_pcnt/libesp_driver_pcnt.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/esp_hal_pcnt/libesp_hal_pcnt.a"
    # T4.3: pinConfig is DEFINED in libmain.a (pinconfig.c). The uc2_* libs
    # reference it, so libmain.a must be INSIDE the group: the uc2_* group
    # resolves among themselves and stops, so a pinConfig reference would
    # otherwise dangle against a later libmain.a (same pattern as canopen_slave_motor).
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/main/libmain.a"
    "LINKER:--end-group")
