# T5.1: tca9555 — C-ified from C++ class to flat C functions.
# Uses the uc2_wire_c.h C-facing I2C API.
idf_component_register(SRCS "tca9555.c"
                       INCLUDE_DIRS "include"
                       REQUIRES uc2_hal)