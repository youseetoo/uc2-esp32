# T2.3: canopen_slave_gpio app entry point.
# main.c is a thin shim: the real logic lives in components/uc2_app/main.cpp,
# compiled by uc2_app (T1.4) and gated by the same #ifdefs as the PIO build.
# T4.4 4h: pinconfig.c (T4.1 pure-C board table) added.
idf_component_register(SRCS "main.c" "pinconfig.c"
    INCLUDE_DIRS "."
    REQUIRES uc2_app uc2_hal)
# T4.4 4h: pinconfig.c re-initializes fields already set by
# PINCONFIG_DEFAULTS_FIELDS (board overrides after the defaults);
# GCC 15 (gnu23 + -Wextra + -Werror) promotes -Woverride-init to an error.
target_compile_options(__idf_main PRIVATE -Wno-override-init)

# T4.4 4h: link-order fix for the uc2_app <-> uc2_canopen <-> uc2_io/<->
# uc2_periph cycle (same --start-group technique as canopen_slave_motor T4.3).
# Group the archives so the linker iterates them. pinConfig is DEFINED in
# libmain.a (pinconfig.c) and referenced by the uc2_* libs, so libmain.a
# must be INSIDE the group: the uc2_* group resolves among themselves and
# stops, so a pinConfig reference would otherwise dangle against a later
# libmain.a (same pattern as canopen_slave_laser).
target_link_libraries(__idf_main PRIVATE
    "LINKER:--start-group"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_app/libuc2_app.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_canopen/libuc2_canopen.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_io/libuc2_io.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_periph/libuc2_periph.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_serial/libuc2_serial.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_core/libuc2_core.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/uc2_hal/libuc2_hal.a"
    "LINKER:${CMAKE_BINARY_DIR}/esp-idf/main/libmain.a"
    "LINKER:--end-group")
