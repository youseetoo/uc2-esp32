# Vendored USB Host driver (HCD + USBH + USB Host API).
#
# Origin: ESP-IDF 5.5.1, components/usb (Espressif Systems, BSD-2-Clause) —
# see README.md. IDF 6.1 removed the host driver from the distribution, so
# this project carries it as a local component (T1.3 action 3).
#
# 6.1 adaptations vs. the 5.5 original:
#   - hal/usb_*_hal.h now live in the esp_hal_usb component (was: hal)
#   - esp_private/sleep_usb.h + periph_ctrl.h live in esp_hw_support
#   - Kconfig: dropped `depends on IDF_EXPERIMENTAL_FEATURES` on
#     USB_HOST_EXT_PORT_RESET_ATTEMPTS (option removed from IDF 6.1 core)

idf_build_get_property(target IDF_TARGET)

if(${target} STREQUAL "linux")
    return() # This component is not supported by the POSIX/Linux simulator
endif()

set(srcs)
# T5.4: always expose the include dirs (a consumer needs usb/usb_host.h to
# compile regardless of component-registration order; the Kconfig is not
# reliably set at registration). Header-only is harmless; SRCs stay Kconfig-gated.
set(include "include")
set(priv_includes "private_include")
# As CONFIG_SOC_USB_OTG_SUPPORTED comes from Kconfig, it is not evaluated yet
# when components are being registered.
# Thus, always add the (private) requirements, regardless of Kconfig
set(priv_requires esp_driver_gpio esp_mm esp_hal_usb esp_hw_support esp_rom)  # usb_phy driver relies on gpio driver API

# Explicitly add psram component for esp32p4, as the USB-DWC internal DMA can access PSRAM on esp32p4
if(${target} STREQUAL "esp32p4")
    list(APPEND priv_requires esp_psram)
endif()

if(CONFIG_SOC_USB_OTG_SUPPORTED)
    list(APPEND srcs "src/hcd_dwc.c"
                     "src/enum.c"
                     "src/hub.c"
                     "src/usb_helpers.c"
                     "src/usb_host.c"
                     "src/usb_private.c"
                     "src/usbh.c"
                     "src/usb_phy.c"
                     )
endif()

if(CONFIG_USB_HOST_HUBS_SUPPORTED)
    list(APPEND srcs "src/ext_hub.c"
                     "src/ext_port.c")
endif()

idf_component_register(SRCS ${srcs}
                       INCLUDE_DIRS ${include}
                       PRIV_INCLUDE_DIRS ${priv_includes}
                       PRIV_REQUIRES ${priv_requires}
                       )

if(CONFIG_COMPILER_STATIC_ANALYZER AND CMAKE_C_COMPILER_ID STREQUAL "GNU") # TODO GCC-366 (segfault)
    set_property(SOURCE src/usb_host.c PROPERTY COMPILE_FLAGS -fno-analyzer)
endif()
