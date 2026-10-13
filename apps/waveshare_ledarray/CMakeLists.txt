cmake_minimum_required(VERSION 3.16.0)
# T2.2: waveshare_ledarray — Waveshare ESP32S3 64x64 LED matrix (WS2812 RMT ch 0, no motors).
# Shared components from top-level components/ (uc2_core/serial/canopen/io/periph/led).
set(EXTRA_COMPONENT_DIRS "${CMAKE_CURRENT_LIST_DIR}/../../components"
                         "${CMAKE_CURRENT_LIST_DIR}/components")
# UC2_APP_MAIN_DIR: app-local main/ holds the waveshare PinConfig.h; uc2_modinc.cmake
# prepends it so it shadows the shared main/PinConfig.h (laser) for ALL components.
set(ENV{UC2_APP_MAIN_DIR} "${CMAKE_CURRENT_LIST_DIR}/main")
include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(waveshare_ledarray)
