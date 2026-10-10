# USB Host HID (C) — needs the USB host driver + the Arduino.h shim.
idf_component_register(SRCS "hid_host.c" INCLUDE_DIRS "." REQUIRES driver uc2_arduino_shim)
