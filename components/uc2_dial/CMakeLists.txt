# T3.5: uc2_dial — the CANopen-master dial controller. Raw-IDF port of
# main/src/dial, COPIED here (main/ keeps its copy for the PIO reference until
# T6.5, mirroring how uc2_motor/uc2_periph carry ported copies alongside main/).
#
# Gated by UC2_DIAL_CONTROLLER (Kconfig, components/uc2_core/Kconfig): only
# canopen_master enables it, so every other app builds the stub (empty archive).
#
# The dial's CANopen-SDO command path (RoutingTable/SdoEmit) compiles only when
# UC2_SDOEMIT_AVAILABLE is defined (see DialController.cpp). Until SdoEmit.h is
# ported to direct IDF (it still #includes Arduino.h) that macro is NOT set, so
# the three send* functions build green as no-ops and the dial is inert over CAN.
# Porting SdoEmit.h + defining UC2_SDOEMIT_AVAILABLE is the follow-up before the
# [HW] constellation smoke.
include(${CMAKE_CURRENT_LIST_DIR}/../uc2_modinc.cmake)
set(srcs "")
if(CONFIG_UC2_DIAL_CONTROLLER)
    list(APPEND srcs "dial/DialController.cpp")
else()
    list(APPEND srcs "stub.c")
endif()
idf_component_register(SRCS ${srcs}
    INCLUDE_DIRS ${UC2_MODULE_INCLUDE_DIRS}
    REQUIRES uc2_app uc2_core uc2_hal uc2_motor uc2_canopen cjson driver freertos)
