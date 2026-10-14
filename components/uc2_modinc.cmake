# T1.9: shared include-dir list for the split uc2_* components.
# Each component's CMakeLists does:
#   include(${CMAKE_CURRENT_LIST_DIR}/../uc2_modinc.cmake)
# then references ${UC2_MODULE_INCLUDE_DIRS} in idf_component_register(INCLUDE_DIRS ...).
#
# Inside this file CMAKE_CURRENT_LIST_DIR == <repo>/components (this file's dir),
# so copied module dirs are ./uc2_*/<mod> and non-copied headers are ../main/src/<mod>.
# Copied modules (compiled by the components) are listed first; the non-copied modules
# (motor, home, led, ...) still live in main/src/ (headers only, until their own T1.6 split).
set(UC2_MODULE_INCLUDE_DIRS
    # uc2_core
    ${CMAKE_CURRENT_LIST_DIR}/uc2_core/state
    ${CMAKE_CURRENT_LIST_DIR}/uc2_core/config
    ${CMAKE_CURRENT_LIST_DIR}/uc2_core/qid
    ${CMAKE_CURRENT_LIST_DIR}/uc2_core/message
    ${CMAKE_CURRENT_LIST_DIR}/uc2_core/buzzer
    # uc2_serial
    ${CMAKE_CURRENT_LIST_DIR}/uc2_serial/serial
    # uc2_canopen
    ${CMAKE_CURRENT_LIST_DIR}/uc2_canopen/canopen
    ${CMAKE_CURRENT_LIST_DIR}/uc2_canopen/gpio_can
    # T2.8: ptz (RS-485 Pelco-D keyboard -> CANopen bridge, raw-IDF port from
    # main/src/ptz, compiled by uc2_canopen) — listed so bare includes
    # (#include "PtzKeyboard.h") resolve for ALL components.
    ${CMAKE_CURRENT_LIST_DIR}/uc2_canopen/ptz
    # uc2_io
    ${CMAKE_CURRENT_LIST_DIR}/uc2_io/digitalin
    ${CMAKE_CURRENT_LIST_DIR}/uc2_io/digitalout
    ${CMAKE_CURRENT_LIST_DIR}/uc2_io/laser
    ${CMAKE_CURRENT_LIST_DIR}/uc2_io/encoder
    # uc2_periph
    ${CMAKE_CURRENT_LIST_DIR}/uc2_periph/i2c
    ${CMAKE_CURRENT_LIST_DIR}/uc2_periph/tmp102
    ${CMAKE_CURRENT_LIST_DIR}/uc2_periph/fan
    # T2.5: galvo scanner (raw-IDF port, compiled by uc2_periph) — listed before
    # ../main/src/scanner (line below) so the ported copy wins for #include "GalvoController.h".
    ${CMAKE_CURRENT_LIST_DIR}/uc2_periph/scanner
    # board PinConfig (main/PinConfig.h) — for now the laser board's PinConfig lives
    # in the repo main/; T2.11 reworks boards/ to make it per-app. Listed first.
    ${CMAKE_CURRENT_LIST_DIR}/../main
    # PinConfigDefault.h (the PinConfig base struct) lives in uc2_app/ — include-only
    # (no link dep), so this does NOT create a component dependency on uc2_app.
    ${CMAKE_CURRENT_LIST_DIR}/uc2_app
    # T2.6: uc2_motor (stepper/encoder/axis/home/pid/tmc, raw-IDF port from
    # main/src/{motor,home,axis,tmc,pid}) — listed BEFORE ../main/src/{axis,home,
    # motor,pid,tmc} below so the ported copies win for #include "FocusMotor.h" etc.
    ${CMAKE_CURRENT_LIST_DIR}/uc2_motor/axis
    ${CMAKE_CURRENT_LIST_DIR}/uc2_motor/home
    ${CMAKE_CURRENT_LIST_DIR}/uc2_motor/motor
    ${CMAKE_CURRENT_LIST_DIR}/uc2_motor/pid
    ${CMAKE_CURRENT_LIST_DIR}/uc2_motor/tmc
    # non-copied (headers only, still in main/src/)
    ${CMAKE_CURRENT_LIST_DIR}/../main/src/analogin
    ${CMAKE_CURRENT_LIST_DIR}/../main/src/analogout
    ${CMAKE_CURRENT_LIST_DIR}/../main/src/axis
    ${CMAKE_CURRENT_LIST_DIR}/../main/src/bt
    ${CMAKE_CURRENT_LIST_DIR}/../main/src/dac
    ${CMAKE_CURRENT_LIST_DIR}/../main/src/dial
    ${CMAKE_CURRENT_LIST_DIR}/../main/src/heat
    ${CMAKE_CURRENT_LIST_DIR}/../main/src/home
    ${CMAKE_CURRENT_LIST_DIR}/../main/src/image
    ${CMAKE_CURRENT_LIST_DIR}/../main/src/joystick
    ${CMAKE_CURRENT_LIST_DIR}/uc2_led/led
    ${CMAKE_CURRENT_LIST_DIR}/../main/src/motor
    ${CMAKE_CURRENT_LIST_DIR}/../main/src/pid
    ${CMAKE_CURRENT_LIST_DIR}/../main/src/ptz
    ${CMAKE_CURRENT_LIST_DIR}/../main/src/scanner
    ${CMAKE_CURRENT_LIST_DIR}/uc2_led/signal
    # T2.1: neopixel (Adafruit_NeoPixel.h) — for LedController.h (in uc2_led) which is
    # included by uc2_canopen/uc2_serial; in the shared list so ALL components resolve it.
    ${CMAKE_CURRENT_LIST_DIR}/neopixel
    # T2.4: thermal moved from main/src/thermal (raw-IDF port, compiled by uc2_periph).
    ${CMAKE_CURRENT_LIST_DIR}/uc2_periph/thermal
    ${CMAKE_CURRENT_LIST_DIR}/../main/src/tmc)
# T2.1: per-app PinConfig override. An app sets UC2_APP_MAIN_DIR to its own main/
# (in the top-level CMakeLists before project()); it is prepended so its PinConfig.h
# shadows the shared main/PinConfig.h (laser) for ALL components. Empty for the laser
# (which uses the shared main/ PinConfig directly).
# T2.1: per-app PinConfig override. The app sets the UC2_APP_MAIN_DIR environment
# var (top-level CMakeLists, before project()); it is prepended so the app's
# main/PinConfig.h shadows the shared main/PinConfig.h (laser) for ALL components.
# (ENV channel — a plain/CACHE CMake var does not reach ESP-IDF component scope.)
if(DEFINED ENV{UC2_APP_MAIN_DIR})
    list(PREPEND UC2_MODULE_INCLUDE_DIRS "$ENV{UC2_APP_MAIN_DIR}")
endif()
