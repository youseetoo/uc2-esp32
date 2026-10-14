# T2.6: Vendored teemuatlut/TMCStepper v0.7.3 (raw-IDF port, compiled by this component).
# Source copied from github.com/teemuatlut/TMCStepper (platformio.ini lib_deps
# teemuatlut/TMCStepper@^0.7.3, used when TMC_CONTROLLER=1 for the TMC2209 driver).
#
# Port: the library's #if defined(ARDUINO)/__has_include include branches pull the
# local shims in include/ (Arduino.h -> uc2_hal + `Stream` = uc2::Serial_; SPI.h ->
# a no-op hardware-SPI stub, since the accelmotor drives the TMC2209 over UART/Stream
# and the SPI-mode drivers are dead code here). The RPi bcm2835_* files are excluded
# (they are only for the bcm2835 target). SW_CAPABLE_PLATFORM is false (no
# SoftwareSerial.h shim), so the software-serial ctors are `= delete` and the
# accelmotor uses the `TMC2209Stepper(Stream*, ...)` constructor with &Serial1.
file(GLOB TMC_SRCS "source/*.cpp")
list(FILTER TMC_SRCS EXCLUDE REGEX "bcm2835_.*\\.cpp$")
idf_component_register(SRCS ${TMC_SRCS}
                       INCLUDE_DIRS "." "include"
                       REQUIRES uc2_hal)
