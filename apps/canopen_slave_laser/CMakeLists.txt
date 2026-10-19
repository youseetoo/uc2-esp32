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
# UC2_APP_MAIN_DIR: app-local main/ holds the canopen_slave_laser PinConfig.h (T2.11 —
# previously fell through to the root main/PinConfig.h PIO copy target).
set(ENV{UC2_APP_MAIN_DIR} "${CMAKE_CURRENT_LIST_DIR}/main")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(canopen_slave_laser)
# T4.1: pinconfig.c (main) intentionally re-initializes fields already set by
# PINCONFIG_DEFAULTS_FIELDS (board overrides after the defaults); GCC 15
# (gnu23 + -Wextra + -Werror) promotes -Woverride-init to an error.
target_compile_options(__idf_main PRIVATE -Wno-override-init)

# T4.1: the board's pinconfig.c (in the `main` component) defines the single
# extern `pinConfig`. CMake links the uc2_* component libs BEFORE libmain.a,
# and GNU ld does not rescan archives — so pinConfig references from the
# uc2_* libs (main.cpp, RoutingTable.cpp, ...) would stay undefined. Wrap the
# project component libs in one --start-group attached to __idf_main (same
# pattern uc2_3 uses for its cyclic uc2_* REQUIRES web): the linker iterates
# the group to fixpoint and resolves pinConfig regardless of CMake's order.
# Re-listing each lib is a harmless duplicate; if(TARGET) skips components
# gated off per-app (uc2_joystick/neopixel etc. compile no sources here).
set(_uc2_proj_comps
    uc2_app uc2_core uc2_serial uc2_bt uc2_io uc2_periph uc2_motor uc2_canopen
    uc2_led uc2_hal uc2_od uc2_dial)
set(_uc2_group_libs "")
foreach(_uc2_c ${_uc2_proj_comps})
    if(TARGET __idf_${_uc2_c})
        list(APPEND _uc2_group_libs "LINKER:${CMAKE_BINARY_DIR}/esp-idf/${_uc2_c}/lib${_uc2_c}.a")
    endif()
endforeach()
# The pinConfig DEFINITION is in libmain.a (the `main` component's pinconfig.c).
# It must be INSIDE the group: the uc2_* group resolves among themselves and
# stops, so a pinConfig reference would still dangle against a later libmain.a.
# Append libmain.a to the group (harmless duplicate of the normal libmain.a).
list(APPEND _uc2_group_libs "LINKER:${CMAKE_BINARY_DIR}/esp-idf/main/libmain.a")
if(_uc2_group_libs)
    target_link_libraries(__idf_main PRIVATE "LINKER:--start-group" ${_uc2_group_libs} "LINKER:--end-group")
endif()
