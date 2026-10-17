# USB Host HID (C) — needs the USB host driver (no Arduino, no shim).
# usb_host: vendored USB Host driver (IDF 5.5.1) — IDF 6.x removed it from the
# distribution (see components/usb_host/README.md).
#
# Gated on CONFIG_SOC_USB_OTG_SUPPORTED — the SAME switch usb_host itself uses for
# its sources. The USB-Host (DWC) controller exists only on S3/C3/C6/...; a classic
# ESP32 has no USB host, so usb_host registers no sources/headers there. Without this
# gate, hid_host.c is compiled for the classic ESP32 and fails on the now-absent
# usb/usb_host.h. Register an empty stub on targets without USB-OTG host support so
# the shared components that REQUIRES this unconditionally (uc2_core, uc2_app, ...)
# still link on classic-ESP32 apps (e.g. apps/uc2_3).
if(NOT CONFIG_SOC_USB_OTG_SUPPORTED)
    file(WRITE ${CMAKE_CURRENT_BINARY_DIR}/esp_usb_host_hid_stub.c
         "/* esp_usb_host_hid: no USB-OTG host on this target (stub) */\n")
    idf_component_register(SRCS ${CMAKE_CURRENT_BINARY_DIR}/esp_usb_host_hid_stub.c
                           INCLUDE_DIRS ".")
else()
    idf_component_register(SRCS "hid_host.c" INCLUDE_DIRS "."
        REQUIRES driver PRIV_REQUIRES usb_host)
    # The vendored usb_host registers empty sources when its Kconfig is not yet
    # set at registration, so its include dir does not propagate through
    # REQUIRES/PRIV_REQUIRES. Add it directly so hid_host.c finds usb/usb_host.h.
    target_include_directories(${COMPONENT_LIB} PRIVATE
        ${CMAKE_CURRENT_LIST_DIR}/../usb_host/include)
endif()
