cmake_minimum_required(VERSION 3.16.0)
# T5.4: waveshare_d7 (Waveshare ESP32-S3 7" screen board) — PIO env
# waveshare_esp32s3_7inchScreen. Despite the T5.4 "M5GFX" grouping, the board's
# PinConfig is a CAN MOTOR node (copied from the XIAO can-slave-motor config:
# single X motor + TMC + CAN_ID_MOT_X) and its PIO lib_deps are
# FastAccelStepper/TMCStepper/ESP32Encoder — NO M5GFX/M5Unified/M5Dial, no LVGL
# (the -DLV_CONF_INCLUDE_SIMPLE flag is vestigial). So this is a CAN motor node,
# not a display board: no M5 display stack is compiled in.
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components")
# Board + feature flags (from PIO env build_flags). Bare #defines gating code
# across the shared module components. Applied globally via CXX_FLAGS because IDF
# component targets reject external target_compile_definitions. NODE_ROLE is
# value-checked (== 2). ESP32S3_MODEL_XIAO selects the XIAO pin map.
set(CMAKE_CXX_FLAGS "${CMAKE_CXX_FLAGS} -DESP32S3_MODEL_XIAO=1 -DMOTOR_CONTROLLER=1 -DUSE_FASTACCEL=1 -DCAN_BUS_ENABLED=1 -DCAN_CONTROLLER_CANOPEN=1 -DMESSAGE_CONTROLLER=1 -DNODE_ROLE=2")
# UC2_APP_MAIN_DIR: app-local main/ holds the waveshare_d7 PinConfig.h.
set(ENV{UC2_APP_MAIN_DIR} "${CMAKE_CURRENT_LIST_DIR}/main")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(waveshare_d7)

# T3.3: the uc2_* components form a dense, cyclic REQUIRES web (heavy header
# cross-inclusion via uc2_modinc.cmake). CMake cannot topologically order a
# cycle, so it breaks it arbitrarily and real symbol edges end up unresolved at
# the final link. Wrap every built project component lib in one --start-group,
# attached to __idf_main. The motor stack (fastaccelstepper/accelstepper/
# tmcstepper/esp32_encoder) is included for the FAS RMT stepper + TMC + encoder
# leaves. if(TARGET) skips components gated off per-app; the SOURCES check skips
# feature-off components registered with no sources (they create a target but
# produce no lib<comp>.a, so adding them would reference a missing archive).
set(_uc2_proj_comps
    uc2_app uc2_core uc2_serial uc2_io uc2_periph uc2_motor uc2_canopen
    uc2_led uc2_hal uc2_od tca9555 dac_module onewire_ds18b20 tmcstepper
    esp32_encoder fastaccelstepper accelstepper)
set(_uc2_group_libs "")
foreach(_uc2_c ${_uc2_proj_comps})
    if(TARGET __idf_${_uc2_c})
        get_target_property(_uc2_csrcs __idf_${_uc2_c} SOURCES)
        if(_uc2_csrcs)
            list(APPEND _uc2_group_libs "LINKER:${CMAKE_BINARY_DIR}/esp-idf/${_uc2_c}/lib${_uc2_c}.a")
        endif()
    endif()
endforeach()
if(_uc2_group_libs)
    target_link_libraries(__idf_main PRIVATE "LINKER:--start-group" ${_uc2_group_libs} "LINKER:--end-group")
endif()
