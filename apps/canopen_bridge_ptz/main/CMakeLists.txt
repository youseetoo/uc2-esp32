# T2.8: canopen_bridge_ptz app entry point.
# main.c is a thin shim: the real logic lives in components/uc2_app/main.cpp,
# compiled by uc2_app (T1.4) and gated by the same #ifdefs as the PIO build.
# The PTZ keyboard/router sources are compiled by uc2_canopen (ptz/), gated by
# CONFIG_UC2_PTZ_KEYBOARD_CONTROLLER (see components/uc2_canopen/CMakeLists.txt).
idf_component_register(SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES uc2_app)
