cmake_minimum_required(VERSION 3.16.0)
# T3.5: UC2 CANopen master — classic ESP32, all controllers + CANopen master +
# classic-BT HID (PS4). PIO env UC2_canopen_master_release. RAM-tight: drives the
# full slave constellation (laser/led/galvo/gpio/motor) over CAN; its axes are
# CANopen-routed to slaves, so NEITHER USE_FASTACCEL nor USE_ACCELSTEP is set
# (Step 0 — no local stepping backend needed; FocusMotor compiles its
# CANopen-routed path).
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components")
# Board + feature flags (from PIO env UC2_canopen_master build_flags). Bare
# #defines gating code across the shared module components (used in .cpp AND .h).
# Applied globally via CMAKE_(C|CXX)_FLAGS because IDF component targets reject
# external target_compile_definitions ("not built by this project"). Both are
# needed: the T4.1 C port split the modules into .c TUs that read the same
# #ifdef flags (e.g. MotorJsonParser.c's STAGE_SCAN block). NODE_ROLE is
# value-checked (NODE_ROLE == 1), so it keeps its =1 value; the rest use =1 too
# for parity with the PIO flags.
# string(JOIN) keeps this a plain space-joined string: a bare set() would make a
# CMake list, and "${_uc2_feature_flags}" then expands to ONE ;-joined token in
# the flags response file (compiler sees -DNODE_ROLE=1;-D... and every later
# flag is swallowed into NODE_ROLE's value).
string(JOIN " " _uc2_feature_flags
    -DNODE_ROLE=1 -DLASER_CONTROLLER=1 -DDIGITAL_IN_CONTROLLER=1 -DMESSAGE_CONTROLLER=1
    -DCAN_BUS_ENABLED=1 -DDIAL_CONTROLLER=1 -DMOTOR_CONTROLLER=1 -DHOME_MOTOR=1
    -DBTHID=1 -DTMC_CONTROLLER=1 -DLED_CONTROLLER=1 -DBLUETOOTH=1 -DSTAGE_SCAN=1
    -DGALVO_CONTROLLER=1 -DCAN_CONTROLLER_CANOPEN=1 -DTMP102_CONTROLLER=1
    -DFAN_CONTROLLER=1 -DCANOPEN_OTA_NO_RECEIVER=1 -DUC2_FORCE_BT_CLASSIC_ONLY=1)
# Self-healing purge: the pre-string(JOIN) WIP line appended the feature flags
# as ONE ;-joined token (-DNODE_ROLE=1;-D...=1). It leaked into the
# build/toolchain/{c,cxx}flags response files, and IDF's response-file dedup
# only matches whole tokens, so the stale blob survives reconfigures and
# re-defines NODE_ROLE & co. (-Werror 'redefined'). The pollution lives in the
# FILES (CMake inlines them into the flags each configure), so purge the blob
# token from the files before project() runs; no-op once clean. A plain
# -DNODE_ROLE=1 (no semicolons) never matches.
if(EXISTS "${CMAKE_BINARY_DIR}/toolchain")
    foreach(_uc2_rf cflags cxxflags asmflags)
        set(_uc2_rf_path "${CMAKE_BINARY_DIR}/toolchain/${_uc2_rf}")
        if(EXISTS "${_uc2_rf_path}")
            file(READ "${_uc2_rf_path}" _uc2_rf_content)
            string(REGEX REPLACE "-DNODE_ROLE=[^ ;]+(;[A-Za-z0-9_=-]+)+" " "
                   _uc2_rf_content "${_uc2_rf_content}")
            file(WRITE "${_uc2_rf_path}" "${_uc2_rf_content}")
        endif()
    endforeach()
endif()
set(CMAKE_C_FLAGS "${CMAKE_C_FLAGS} ${_uc2_feature_flags}")
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} ${_uc2_feature_flags}")
# UC2_APP_MAIN_DIR: app-local main/ holds the canopen_master PinConfig.h.
set(ENV{UC2_APP_MAIN_DIR} "${CMAKE_CURRENT_LIST_DIR}/main")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(canopen_master)

# T3.3: the uc2_* components form a dense, cyclic REQUIRES web (heavy header
# cross-inclusion via uc2_modinc.cmake). CMake cannot topologically order a
# cycle (e.g. uc2_canopen <-> uc2_motor), so it breaks it arbitrarily and real
# symbol edges end up unresolved at the final link. Wrap every built project
# component lib in one --start-group, attached to __idf_main (present in every
# app; all project component targets exist at this point, post-project()). It
# lands in the final link's LINK_LIBRARIES, so the linker iterates the group to
# fixpoint and resolves all project cross-deps regardless of CMake's chosen
# order. Re-listing each lib is a harmless duplicate; if(TARGET) skips
# components gated off per-app.
set(_uc2_proj_comps
    uc2_app uc2_core uc2_serial uc2_bt uc2_io uc2_periph uc2_motor uc2_canopen
    uc2_led uc2_dial uc2_hal uc2_od tca9555 pscontroller neopixel
    esp_usb_host_hid dac_module onewire_ds18b20 tmcstepper
    # T4.1: the app's own component LAST — pinconfig.c defines the global
    # pinConfig that every C-converted module references; those references
    # only materialize while the second-pass libs above are scanned, so
    # libmain.a must be re-scanned after them or pinConfig stays undefined.
    main)
set(_uc2_group_libs "")
foreach(_uc2_c ${_uc2_proj_comps})
    if(TARGET __idf_${_uc2_c})
        list(APPEND _uc2_group_libs "LINKER:${CMAKE_BINARY_DIR}/esp-idf/${_uc2_c}/lib${_uc2_c}.a")
    endif()
endforeach()
if(_uc2_group_libs)
    target_link_libraries(__idf_main PRIVATE "LINKER:--start-group" ${_uc2_group_libs} "LINKER:--end-group")
endif()

# T4.1: pinconfig.c (main) intentionally re-initializes fields already set by
# PINCONFIG_DEFAULTS_FIELDS (board overrides after the defaults); GCC 15
# (gnu23 + -Wextra + -Werror) promotes -Woverride-init to an error.
target_compile_options(__idf_main PRIVATE -Wno-override-init)
