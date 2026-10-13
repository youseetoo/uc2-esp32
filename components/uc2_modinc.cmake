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
    # uc2_io
    ${CMAKE_CURRENT_LIST_DIR}/uc2_io/digitalin
    ${CMAKE_CURRENT_LIST_DIR}/uc2_io/digitalout
    ${CMAKE_CURRENT_LIST_DIR}/uc2_io/laser
    ${CMAKE_CURRENT_LIST_DIR}/uc2_io/encoder
    # uc2_periph
    ${CMAKE_CURRENT_LIST_DIR}/uc2_periph/i2c
    ${CMAKE_CURRENT_LIST_DIR}/uc2_periph/tmp102
    ${CMAKE_CURRENT_LIST_DIR}/uc2_periph/fan
    # board PinConfig (main/PinConfig.h) — for now the laser board's PinConfig lives
    # in the repo main/; T2.11 reworks boards/ to make it per-app. Listed first.
    ${CMAKE_CURRENT_LIST_DIR}/../main
    # PinConfigDefault.h (the PinConfig base struct) lives in uc2_app/ — include-only
    # (no link dep), so this does NOT create a component dependency on uc2_app.
    ${CMAKE_CURRENT_LIST_DIR}/uc2_app
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
    ${CMAKE_CURRENT_LIST_DIR}/../main/src/thermal
    ${CMAKE_CURRENT_LIST_DIR}/../main/src/tmc)
