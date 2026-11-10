cmake_minimum_required(VERSION 3.16.0)
# T2.5: canopen_slave_galvo — galvo scanner CANopen slave (XIAO ESP32-S3).
# Single CANopen node (CAN_ID_GALVO_0) drives the MCP4822 dual 12-bit SPI DAC
# galvo (high-speed RMT 3-trigger raster/line scanner) for laser projection.
# Shared components from top-level components/ (uc2_core/serial/canopen/io/periph).
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components"
                         "${CMAKE_CURRENT_LIST_DIR}/components")
# -DESP32S3_MODEL_XIAO: board model (bare #define gating code across shared components).
# GALVO_CONTROLLER: enable the galvo scanner (raw-IDF port in uc2_periph/scanner,
# compiled by uc2_periph under CONFIG_UC2_GALVO_CONTROLLER). Applied globally via
# CXX_FLAGS because IDF component targets reject external target_compile_definitions
# ("not built by this project"). All module code is C++.
# T4.4: module sources are now C — the macro must also reach the C
# compilation. Inlined in the quoted set(): a list variable expands ;-joined
# and pollutes the build/toolchain/{c,cxx}flags response file.
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} -DESP32S3_MODEL_XIAO=1 -DGALVO_CONTROLLER")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DESP32S3_MODEL_XIAO=1 -DGALVO_CONTROLLER")
# UC2_APP_MAIN_DIR: app-local main/ holds the canopen_slave_galvo PinConfig.h;
# uc2_modinc.cmake prepends it so it shadows the shared main/PinConfig.h (laser).
set(ENV{UC2_APP_MAIN_DIR} "${CMAKE_CURRENT_LIST_DIR}/main")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(canopen_slave_galvo)

# T4.1: pinConfig is DEFINED in libmain.a (the app's main/pinconfig.c). The
# uc2_* libs reference it only after they are themselves extracted, so wrap
# every built project component lib in one --start-group with main LAST: the
# linker iterates the group to fixpoint and libmain.a is re-scanned after the
# libs that need pinConfig.
set(_uc2_proj_comps
    uc2_app uc2_core uc2_serial uc2_bt uc2_io uc2_periph uc2_motor uc2_canopen
    uc2_led uc2_dial uc2_hal uc2_od tca9555 pscontroller neopixel
    esp_usb_host_hid dac_module onewire_ds18b20 tmcstepper
    main)
set(_uc2_group_libs "")
foreach(_uc2_c ${_uc2_proj_comps})
    if(TARGET __idf_${_uc2_c})
        # neopixel registers as an EMPTY stub (target exists, no archive)
        # when UC2_LED_CONTROLLER is off — skip it or the link fails.
        if(NOT _uc2_c STREQUAL "neopixel" OR CONFIG_UC2_LED_CONTROLLER)
            list(APPEND _uc2_group_libs "LINKER:${CMAKE_BINARY_DIR}/esp-idf/${_uc2_c}/lib${_uc2_c}.a")
        endif()
    endif()
endforeach()
if(_uc2_group_libs)
    target_link_libraries(__idf_main PRIVATE "LINKER:--start-group" ${_uc2_group_libs} "LINKER:--end-group")
endif()

# T4.1: pinconfig.c re-initializes PINCONFIG_DEFAULTS_FIELDS overrides;
# GCC 15 (gnu23 + -Wextra + -Werror) promotes -Woverride-init to an error.
target_compile_options(__idf_main PRIVATE -Wno-override-init)
