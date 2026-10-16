cmake_minimum_required(VERSION 3.16.0)
# T3.4: UC2 Standalone V3 — classic ESP32 BT board (BLUETOOTH + BTHID, no CAN).
# Moved out of the PIO env UC2_3 (see platformio.ini); first BT board onto IDF.
# (no app-local components/ dir for this board — all controllers are shared components)
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components")
# Board + feature flags (from PIO env UC2_3 build_flags). Bare #defines gating code
# across the shared module components (used in .cpp AND .h). Applied globally via
# CXX_FLAGS because IDF component targets reject external target_compile_definitions
# ("not built by this project"). All module code is C++, so CXX_FLAGS covers it.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DBLUETOOTH -DBTHID -DMOTOR_CONTROLLER -DLASER_CONTROLLER -DLED_CONTROLLER -DDIGITAL_IN_CONTROLLER -DUSE_TCA9535 -DHOME_MOTOR -DMESSAGE_CONTROLLER -DDAC_CONTROLLER -DHEAT_CONTROLLER -DDS18B20_CONTROLLER -DUSE_FASTACCEL -DSTAGE_SCAN")
# UC2_APP_MAIN_DIR: app-local main/ holds the uc2_3 PinConfig.h (T2.11 — previously
# fell through to the root main/PinConfig.h PIO copy target).
set(ENV{UC2_APP_MAIN_DIR} "${CMAKE_CURRENT_LIST_DIR}/main")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(uc2_3)

# T3.3: the uc2_* components form a dense, cyclic REQUIRES web (heavy header
# cross-inclusion via uc2_modinc.cmake). CMake cannot topologically order a
# cycle (e.g. uc2_canopen <-> uc2_motor), so it breaks it arbitrarily and real
# symbol edges (MotorJsonParser, in uc2_motor, needed by uc2_canopen) end up
# unresolved at the final link. Wrap every built project component lib in one
# --start-group, attached to __idf_main (present in every app; all project
# component targets exist at this point, post-project()). It lands in the final
# link's LINK_LIBRARIES, so the linker iterates the group to fixpoint and
# resolves all project cross-deps regardless of CMake's chosen order. Re-listing
# each lib is a harmless duplicate; if(TARGET) skips components gated off per-app.
set(_uc2_proj_comps
    uc2_app uc2_core uc2_serial uc2_bt uc2_io uc2_periph uc2_motor uc2_canopen
    uc2_led uc2_hal uc2_od tca9555 pscontroller neopixel esp_usb_host_hid
    dac_module onewire_ds18b20)
set(_uc2_group_libs "")
foreach(_uc2_c ${_uc2_proj_comps})
    if(TARGET __idf_${_uc2_c})
        list(APPEND _uc2_group_libs "LINKER:${CMAKE_BINARY_DIR}/esp-idf/${_uc2_c}/lib${_uc2_c}.a")
    endif()
endforeach()
if(_uc2_group_libs)
    target_link_libraries(__idf_main PRIVATE "LINKER:--start-group" ${_uc2_group_libs} "LINKER:--end-group")
endif()
