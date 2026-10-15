# T3.1 — BT/PS4 inventory

Scope: the unported BT boards — `uc2_2`, `uc2_3`, `uc2_4`, `uc2_wemos` (ESP32, BT/PS4) — plus
`canopen_master` (ESP32, `UC2_FORCE_BT_CLASSIC_ONLY`) and `canopen_standalone_v4` (ESP32).

## BT code (PIO-era location)
`main/src/bt/`:
- `BtController.cpp` (22.9 KB) + `.h` — pairing/scan/connect state machine, bond-device list.
- `HidController.cpp` (12.8 KB) + `.h` — HID report handling (PS4).
- `esp_hid_gap.c` (30.4 KB, **C**) + `.h` — GAP/HID transport layer (classic + BLE).
- `HidGamePad.h`, `PS4TrackpadParser.h` — PS4 report parsing.
- Vendor `lib/PSController` (C++ leaf, D2) — stays a leaf.

Gates: `UC2_BLUETOOTH` (component registration), `UC2_BTHID` (HID-specific sources).

## Stack usage (the decisive finding)
The code uses **both** transports, as a **HID host** (it connects *to* the controller):
- **Classic BR/EDR** — `esp_bt_gap_*` (get/remove bond device, set_pin/set_security_param,
  start_discovery, `esp_bt_gap_register_callback`, resolve_eir_data).
- **BLE** — `esp_ble_gap_*` (set_scan_params, start_scanning, security_rsp, adv_init),
  `esp_ble_adv_data_t`, `esp_ble_auth_req_t`; `CONFIG_BT_BLE_ENABLED`. The PS4 is a **BLE HID device**.
- HID host: `CONFIG_BT_HID_HOST_ENABLED`, `esp_hid_*` (scan results, usage-from-COD/appearance).

All of these are **raw ESP-IDF Bluedroid API names** — not Arduino wrappers.

## Init sequence (already raw-IDF, not Arduino auto-init)
- `esp_hid_gap.c`: `esp_bt_controller_init(&bt_cfg)` → `esp_bt_controller_enable(mode)` → `esp_bluedroid_init()`.
- `BtController.cpp`: `esp_bt_controller_init(&bt_cfg)` → `esp_bt_controller_enable(ESP_BT_MODE_CLASSIC_BT)`
  **or** `ESP_BT_MODE_BTDM` (branched — the code already selects classic-only vs dual at runtime).

→ No Arduino BT auto-init dependency; the port does **not** need to add an init sequence.

## Arduino.h dependency (small)
Only **two** BT files include it: `BtController.cpp:8` and `HidController.h:24`. `esp_hid_gap.c` is
pure C (no Arduino). Everything else is standard `esp_*` / C headers. Stripping these two includes
(replacing whatever they provide — types / logging) is the bulk of the seam rework.

## RAM (static estimate — [HW] verification pending T3.7)
Bluedroid dual-mode (classic+BLE) on ESP32 (320 KB) ≈ 100–150 KB static + ~30–60 KB heap for
controller buffers. The classic-only path is lighter. If dual-mode is over budget on a specific
board, drop to `ESP_BT_MODE_CLASSIC_BT` at runtime (already supported) and trim BT buffer sizes.

## Conclusion
Bluedroid is the only IDF BT stack that does classic **and** BLE in one controller, and the code
already speaks its API. → **Bluedroid, dual-mode, HID host** (see `bt.md`). NimBLE rejected
(BLE-only). Classic-only rejected (drops the BLE/PS4 path the code implements).
