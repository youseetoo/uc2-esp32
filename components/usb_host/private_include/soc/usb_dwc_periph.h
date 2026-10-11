/*
 * Vendored-driver compatibility header.
 *
 * IDF 5.5 shipped this peripheral description as `soc/usb_dwc_periph.h`
 * (in the `soc` component). IDF 6.1 renamed it to `soc/usb_periph.h` and
 * moved it into the `esp_hal_usb` component. The vendored 5.5 sources
 * (hcd_dwc.c, usb_host.c, usb_phy.c) still `#include "soc/usb_dwc_periph.h"`,
 * so this one-line forwarding header keeps `src/` byte-identical to the
 * upstream 5.5 files (see ../README.md).
 *
 * The deprecated `usb_phy_signal_conn_t` / `usb_otg_periph_signal` block that
 * 5.5 also carried in this header is NOT used by the vendored sources, so no
 * back-compat is needed here.
 */
#pragma once
#include "soc/usb_periph.h"
