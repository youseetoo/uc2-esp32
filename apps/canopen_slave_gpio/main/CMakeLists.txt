# T2.3: canopen_slave_gpio app entry point.
# main.c is a thin shim: the real logic lives in components/uc2_app/main.cpp,
# compiled by uc2_app (T1.4) and gated by the same #ifdefs as the PIO build.
idf_component_register(SRCS "main.c"
    INCLUDE_DIRS "." "boards/canopen_slave_gpio"
    REQUIRES uc2_app)
