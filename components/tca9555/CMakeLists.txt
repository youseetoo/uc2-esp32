# T3.4: tca9555 — vendored RobTillaart TCA9555 0.4.4 (Apache-2.0) I2C 16-channel
# port-expander driver, retargeted from the Arduino TwoWire/Wire API onto the
# uc2::Wire_ shim (uc2_hal). C++ leaf, ported as-is (C-ification deferred T5.1).
#
# Only consumer path: tca_controller.h (USE_TCA9535 boards UC2_2/3/4/wemos), which
# is pulled in by uc2_app / uc2_io / uc2_motor under that flag.
idf_component_register(SRCS "TCA9555.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES uc2_hal)
