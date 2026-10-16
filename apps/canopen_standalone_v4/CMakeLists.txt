cmake_minimum_required(VERSION 3.16.0)
# T3.6: UC2 canopen standalone v4 — classic ESP32, HYBRID CAN mode. PIO env
# UC2_canopen_standalone_v4_release. Native FastAccelStepper motors A-Z (axes
# 0-3) + CAN satellites (4+); native lasers 0-3 + CAN laser 4; TCA9535 IO;
# classic-BT HID (PS4); OTA sender+receiver compiled OUT.
#
# Unlike canopen_master (T3.5), this board DOES local stepping: it drives its
# on-board axes with FastAccelStepper, so UC2_USE_FASTACCEL IS set (Step 0 for
# T3.5 was the opposite — fully CANopen-routed, no local backend). No dial.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components")
# Board + feature flags (from PIO env UC2_canopen_standalone_v4 build_flags). Bare
# #defines gating code across the shared module components (used in .cpp AND .h).
# Applied globally via CXX_FLAGS because IDF component targets reject external
# target_compile_definitions ("not built by this project"). All module code is C++,
# so CXX_FLAGS covers it. =1 kept for parity with the PIO flags (NODE_ROLE is
# value-checked: NODE_ROLE == 1).
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DNODE_ROLE=1 -DLASER_CONTROLLER=1 -DDIGITAL_IN_CONTROLLER=1 -DMESSAGE_CONTROLLER=1 -DCAN_BUS_ENABLED=1 -DCAN_CONTROLLER_CANOPEN=1 -DMOTOR_CONTROLLER=1 -DHOME_MOTOR=1 -DUSE_FASTACCEL=1 -DLED_CONTROLLER=1 -DBTHID=1 -DBLUETOOTH=1 -DSTAGE_SCAN=1 -DGALVO_CONTROLLER=1 -DDAC_CONTROLLER=1 -DUSE_TCA9535=1 -DCANOPEN_OTA_NO_RECEIVER=1 -DCANOPEN_OTA_NO_SENDER=1 -DUC2_FORCE_BT_CLASSIC_ONLY=1")
# UC2_APP_MAIN_DIR: app-local main/ holds the canopen_standalone_v4 PinConfig.h.
set(ENV{UC2_APP_MAIN_DIR} "${CMAKE_CURRENT_LIST_DIR}/main")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(canopen_standalone_v4)

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
    esp_usb_host_hid dac_module onewire_ds18b20 tmcstepper)
set(_uc2_group_libs "")
foreach(_uc2_c ${_uc2_proj_comps})
    if(TARGET __idf_${_uc2_c})
        list(APPEND _uc2_group_libs "LINKER:${CMAKE_BINARY_DIR}/esp-idf/${_uc2_c}/lib${_uc2_c}.a")
    endif()
endforeach()
if(_uc2_group_libs)
    target_link_libraries(__idf_main PRIVATE "LINKER:--start-group" ${_uc2_group_libs} "LINKER:--end-group")
endif()
