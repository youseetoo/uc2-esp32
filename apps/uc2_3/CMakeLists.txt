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
