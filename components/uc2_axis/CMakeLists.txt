# T4.1: uc2_axis — closed-loop axis / feedback layer (design v2). Split out of
# uc2_motor (its axis/ subdirectory) so the C conversion can build the module
# as pure C.
# Pure C except EncoderBackend.cpp — the C++ island wrapping the ESP32Encoder
# library behind the extern "C" EncoderBackend.h API.
include(${CMAKE_CURRENT_LIST_DIR}/../uc2_modinc.cmake)
idf_component_register(SRCS "axis/AxisController.c"
                       "axis/AxisCalibration.c"
                       "axis/AxisNotify.c"
                       "axis/AxisWatchdog.c"
                       "axis/EncoderMonitor.c"
                       "axis/EncoderBackend.cpp"
                       INCLUDE_DIRS "axis" ${UC2_MODULE_INCLUDE_DIRS}
                        # uc2_motor NOT a REQUIRES (would cycle: uc2_canopen ->
                        # uc2_axis -> uc2_motor -> uc2_canopen). The
                        # axis_motor_c.* shims (in uc2_motor) satisfy the motor
                        # calls at app link time; axis_motor_c.h resolves
                        # through UC2_MODULE_INCLUDE_DIRS.
                        REQUIRES uc2_core uc2_hal uc2_serial
                                esp32_encoder driver esp_system)
