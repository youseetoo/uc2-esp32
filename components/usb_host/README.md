# `usb_host` — vendored USB Host driver (IDF 5.5.1)

Vendored verbatim from **ESP-IDF v5.5.1, `components/usb`** (Espressif Systems,
**BSD-2-Clause** — the standard IDF license). IDF 6.1 removed the USB Host
driver from the distribution (only the `esp32s3_usb_host_ds4` example file
remains), so the project carries the driver as a local component.

Used by: `components/esp_usb_host_hid` (DS4 → CANopen bridge).

## What was copied

- `src/` ← 5.5 `enum.c ext_hub.c ext_port.c hcd_dwc.c hub.c usbh.c
  usb_helpers.c usb_host.c usb_phy.c usb_private.c` (unchanged)
- `include/` ← 5.5 `include/` (unchanged): `usb/usb_host.h`,
  `usb/usb_helpers.h`, `usb/usb_types_ch9.h`, `usb/usb_types_ch11.h`,
  `usb/usb_types_stack.h`, `esp_private/usb_phy.h`
- `private_include/` ← 5.5 `private_include/` (unchanged) + two added
  compatibility headers (deviations 3 + 4)
- `Kconfig` ← 5.5 `Kconfig` (1 deviation, see below)
- `CMakeLists.txt` ← rewritten for the `src/` layout + 6.1 component names

## Deviations from the 5.5 original

1. **CMakeLists.txt**: `hal/usb_*_hal.h` moved from the `hal` component to
   `esp_hal_usb` in 6.1, and `esp_private/{sleep_usb,periph_ctrl,esp_cache_private}.h`
   to `esp_hw_support` — so `PRIV_REQUIRES` is
   `esp_driver_gpio esp_mm esp_hal_usb esp_hw_support esp_rom`
   (5.5 relied on transitive deps for the last three).
2. **Kconfig**: `USB_HOST_EXT_PORT_RESET_ATTEMPTS` no longer depends on
   `IDF_EXPERIMENTAL_FEATURES` (that option no longer exists in 6.1 core;
    without the drop the int option would silently default to 0).
3. **`private_include/soc/usb_dwc_periph.h` (added)**: 6.1 renamed
   `soc/usb_dwc_periph.h` to `soc/usb_periph.h` (moved to `esp_hal_usb`), so
   this one-line forwarding header lets the unchanged `src/` files keep
   including the old name. Only the modern `usb_dwc_info` API is used by the
   vendored sources; the deprecated `usb_otg_periph_signal` block is not.
4. **`private_include/hal/usb_phy_types.h` (added)**: the 5.5 `hal` component
   shipped `hal/usb_phy_types.h` (4 PHY enums: `usb_phy_target_t`,
   `usb_phy_controller_t`, `usb_otg_mode_t`, `usb_phy_speed_t`). 6.1 folded
   those enums into `esp_hw_support`'s `esp_private/usb_phy.h` — which
   collides by name with the vendored copy of that header, so it can no longer
   be included by path. The 5.5 file is vendored verbatim instead, keeping the
   `src/` + `include/` trees byte-identical to upstream.

No other source-code changes were needed: the `esp_hal_usb` wrap/dwc HAL API
is source-compatible between 5.5 and 6.1 (verified by header diff:
`usb_wrap_hal.h` 10-line diff, `usb_dwc_hal.h` additive only).

## Update policy

If we ever move to an IDF release that ships a host driver again (or a
community `usb_host` component on the component registry), this directory is
replaced wholesale — it is not project code.
