# T2.5: canopen_slave_galvo app entry point.
# main.c is a thin shim: the real logic lives in components/uc2_app/main.cpp,
# compiled by uc2_app (T1.4) and gated by the same #ifdefs as the PIO build.
# The scanner objects come from uc2_periph (gated by CONFIG_UC2_GALVO_CONTROLLER).
idf_component_register(SRCS "main.c"
    INCLUDE_DIRS "."
    REQUIRES uc2_app uc2_periph)
