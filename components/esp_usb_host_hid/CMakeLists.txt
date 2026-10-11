# USB Host HID (C) — needs the USB host driver (no Arduino, no shim).
# usb_host: vendored USB Host driver (IDF 5.5.1) — IDF 6.x removed it from the
# distribution (see components/usb_host/README.md).
idf_component_register(SRCS "hid_host.c" INCLUDE_DIRS "." REQUIRES driver usb_host)
