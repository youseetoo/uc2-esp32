# T2.1: uc2_led — WS2812 (Ws2812Rmt bare-metal RMT) + SignalController, gated on UC2_LED_CONTROLLER.
# Gating per component-layout.md §3. When the flag is off the component registers
# headers-only (no sources), so apps that don't enable LED (e.g. canopen_slave_laser)
# compile nothing from uc2_led — clean regression.
include(${CMAKE_CURRENT_LIST_DIR}/../uc2_modinc.cmake)
set(srcs "")
if(CONFIG_UC2_LED_CONTROLLER)
    set(srcs "led/LedController.cpp" "led/Ws2812Rmt.cpp" "signal/SignalController.cpp")
endif()
if(srcs)
    idf_component_register(SRCS ${srcs}
        INCLUDE_DIRS ${UC2_MODULE_INCLUDE_DIRS}
        REQUIRES uc2_core uc2_serial uc2_periph uc2_canopen uc2_io cjson driver nvs_flash)
else()
    # UC2_LED_CONTROLLER off: headers-only registration (headers still resolvable for
    # any component via uc2_modinc.cmake's shared include list).
    idf_component_register(INCLUDE_DIRS ${UC2_MODULE_INCLUDE_DIRS})
endif()
