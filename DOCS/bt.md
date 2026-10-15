# T3.2 — BT stack decision (D2/D1-adjacent, Phase 3)

## Decision
**Bluedroid, dual-mode (classic BR/EDR + BLE), HID *host* profile.**

## Why (evidence from `bt-inventory.md`)
- The code uses **both** `esp_bt_gap_*` (classic) and `esp_ble_gap_*`/`esp_ble_adv_*` (BLE).
  Bluedroid is the **only** IDF stack that supports both in a single controller; NimBLE is
  BLE-only (no classic) → rejected.
- The PS4 is a **BLE HID device**; a classic-only stack would drop that path. The plan's
  "classic-only" default flips to dual-mode per T3.2's own rule ("NimBLE fallback only if the code
  uses BLE" — it does, so keep Bluedroid + enable the BLE subset).
- The code already calls the raw-IDF init (`esp_bt_controller_init/enable`, `esp_bluedroid_init`)
  and speaks Bluedroid's GAP/HID API directly → the port is mostly **sdkconfig + strip 2
  `Arduino.h` includes**, not a rewrite.
- `canopen_master` sets `UC2_FORCE_BT_CLASSIC_ONLY`; the code already branches to
  `ESP_BT_MODE_CLASSIC_BT` there, so a classic-only board still works within a dual-enabled stack.

## sdkconfig fragment (BT apps' `sdkconfig.defaults` ONLY — never root)
```
CONFIG_BT_ENABLED=y
CONFIG_BT_CLASSIC_ENABLED=y
CONFIG_BT_BLUEDROID_ENABLED=y
CONFIG_BT_BLE_ENABLED=y
CONFIG_BT_HID_HOST_ENABLED=y
```
Buffer sizing / per-board classic-only tuned at T3.7 ([HW] RAM headroom).

## What breaks if wrong
- BLE disabled but a board needs PS4-over-BLE → no pairing.
- NimBLE chosen → classic gamepads + the `esp_bt_gap_*` calls don't exist → build break.
- RAM overflow on a 320 KB ESP32 → watchdog; fix by runtime classic-only + smaller BT buffers.

## Rollback
Doc-only (T3.3 not started).
