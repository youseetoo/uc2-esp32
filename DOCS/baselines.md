# T0.3 — Size baselines (C++ / PlatformIO, before port)

Index: `../PORT_PLAN_STEPS.md`. Measured 2026-10-08 on branch `port/idf-c`
(@ `68ce3a9` + T0.2 platform-pin fix below) with full `pio run -e <env>` builds
(`baselines_run.ps1`, logs in `$env:TEMP\uc2_baselines\`). All 17 CI envs build
green. Numbers are for the **release** configuration of each env; the `_debug`
variants (and `esp32dev`, the two M5GFX envs) are not baselined.

## Method

- **flash used** = size of `.pio/build/<env>/firmware.bin` (the actual flashed
  image, including app header + segment alignment).
- **flash free** = *app partition* size minus flash used. Capacity is the app
  partition, not the whole chip: `custom_partition.csv` → factory 2 MB (classic
  envs), `custom_partition_esp32s3.csv` → app0/app1 1.25 MB OTA (S3 envs),
  `custom_partition_esp32s3_waveshare.csv` → 1.5 MB OTA (waveshare). This is the
  ceiling the C/IDF app must also respect in later phases.
- **IRAM used** = ELF sections (objdump `-h`) with VMA in the SRAM-IRAM regions
  (`iram0_0_seg` + `rtc_iram_seg`). The flash-mapped `iram0_2_seg` (classic
  `0x400D0020…`, S3 `0x42000020…`) and `drom0_0_seg` are **not** IRAM.
- **RAM used** = ELF ALLOC sections with VMA in `dram0_0_seg` + `rtc_data_seg`
  + `rtc_slow_seg`. **RAM free** = region capacity per the arduino-esp32
  `tools/sdk/<target>/ld/memory.ld` minus used:
  classic = 0x2C200 + 2×0x2000 (192.5 KB); S3 = 0x54700 + 0x2000 (353.5 KB).
  (IRAM capacity is separate: classic 136 KB, S3 370.5 KB — see
  `baselines_parse.py` for the exact ranges; it is *not* subtracted from the
  RAM column on either chip, matching how the Arduino toolchain budgets them.)
- Totals per chip come from the linker script of the *installed* framework
  version, so the "free" columns are budget-relative, not raw SRAM-die-relative.

## Toolchain (from build logs)

| Group | Platform | framework-espidf | framework-arduinoespressif32 |
|-------|----------|------------------|------------------------------|
| classic envs (board `esp32dev`) | platform-espressif32 `#v6.3.2` | 4.4.4 | 2.0.15 |
| S3 envs (board `seeed_xiao_esp32s3`) | espressif32 `6.9.0` | 4.4.7 | 2.0.15 |
| `waveshare_esp32s3_ledarray` | platform-espressif32 `#v6.3.2` | 4.4.4 | 2.0.17 |

Toolchain: xtensa-esp32/esp32s3 `8.4.0+2021r2-patch5`.

## Baselines

| env | flash used | flash free | RAM used | RAM free | IRAM used |
|-----|-----------:|-----------:|---------:|---------:|----------:|
| UC2_2 | 1278256 (1248.3 KB) | 818896 (799.7 KB) | 91428 (89.3 KB) | 105692 (103.2 KB) | 98339 (96.0 KB) |
| UC2_3 | 1296288 (1265.9 KB) | 800864 (782.1 KB) | 92060 (89.9 KB) | 105060 (102.6 KB) | 100791 (98.4 KB) |
| UC2_4 | 1288544 (1258.3 KB) | 808608 (789.7 KB) | 91996 (89.8 KB) | 105124 (102.7 KB) | 99767 (97.4 KB) |
| UC2_WEMOS | 1292512 (1262.2 KB) | 804640 (785.8 KB) | 92020 (89.9 KB) | 105100 (102.6 KB) | 99767 (97.4 KB) |
| seeed_xiao_esp32s3 | 915648 (894.2 KB) | 395072 (385.8 KB) | 120808 (118.0 KB) | 233240 (227.8 KB) | 80739 (78.8 KB) |
| seeed_xiao_esp32s3_ledservo | 787792 (769.3 KB) | 522928 (510.7 KB) | 92240 (90.1 KB) | 261808 (255.7 KB) | 60923 (59.5 KB) |
| seeed_xiao_esp32s3_ledring | 766048 (748.1 KB) | 544672 (531.9 KB) | 90140 (88.0 KB) | 263908 (257.7 KB) | 58991 (57.6 KB) |
| UC2_canopen_slave_motor_release | 876944 (856.4 KB) | 433776 (423.6 KB) | 109252 (106.7 KB) | 244796 (239.1 KB) | 61307 (59.9 KB) |
| UC2_canopen_slave_accelmotor_release | 862048 (841.8 KB) | 448672 (438.2 KB) | 103464 (101.0 KB) | 250584 (244.7 KB) | 58583 (57.2 KB) |
| UC2_canopen_slave_galvo_release | 804480 (785.6 KB) | 506240 (494.4 KB) | 106428 (103.9 KB) | 247620 (241.8 KB) | 63239 (61.8 KB) |
| UC2_canopen_slave_laser_release | 769520 (751.5 KB) | 541200 (528.5 KB) | 97908 (95.6 KB) | 256140 (250.1 KB) | 58991 (57.6 KB) |
| UC2_canopen_slave_led_release | 802496 (783.7 KB) | 508224 (496.3 KB) | 101352 (99.0 KB) | 252696 (246.8 KB) | 60923 (59.5 KB) |
| UC2_canopen_slave_gpio_release | 777536 (759.3 KB) | 533184 (520.7 KB) | 99204 (96.9 KB) | 254844 (248.9 KB) | 59835 (58.4 KB) |
| UC2_canopen_bridge_ps4_usbhost_release | 913680 (892.3 KB) | 397040 (387.7 KB) | 107008 (104.5 KB) | 247040 (241.2 KB) | 68067 (66.5 KB) |
| UC2_canopen_master_release | 1353040 (1321.3 KB) | 744112 (726.7 KB) | 101164 (98.8 KB) | 95956 (93.7 KB) | 101327 (99.0 KB) |
| UC2_canopen_standalone_v4_release | 1359664 (1327.8 KB) | 737488 (720.2 KB) | 101308 (98.9 KB) | 95812 (93.6 KB) | 102867 (100.5 KB) |
| waveshare_esp32s3_ledarray | 728704 (711.6 KB) | 844160 (824.4 KB) | 96944 (94.7 KB) | 257104 (251.1 KB) | 64887 (63.4 KB) |

Tightest margins (watch during the port):

- **UC2_canopen_master_release / UC2_canopen_standalone_v4_release**: ~55 % of
  the 2 MB app partition and ~99/100.5 KB of 136 KB classic IRAM already used;
  RAM free is the lowest of the fleet (~93–94 KB). These are the envs where a
  C port must not grow.
- **UC2_2/3/4/WEMOS** (BT boards): 61–62 % flash, 70–74 % classic IRAM, ~103 KB
  RAM free.
- All S3 envs are comfortably inside their 1.25/1.5 MB OTA partitions.

## T0.2 defect found and fixed during T0.3

3 of the 17 envs did **not** build on the fresh branch:
`seeed_xiao_esp32s3_ledservo`, `seeed_xiao_esp32s3_ledring`,
`waveshare_esp32s3_ledarray`.

Cause: they pinned `platform = https://github.com/platformio/platform-espressif32.git`
**without a tag**, which by 2026-10 resolves to a platform shipping
`framework-espidf` 5.5.5, while their `platform_packages` pins arduino-esp32
2.0.15/2.0.17 ("Arduino-esp32 can be used with ESP-IDF versions between 4.4.0
and 4.4.99, but a newer version is detected: 5.5.5").

Fix (in `platformio.ini`, committed with this doc): pin each env to the same
platform as the env it extends — `espressif32@6.9.0` for the two led* envs
(base `UC2_3_Xiao`) and `…#v6.3.2` for waveshare (base `esp32_framework`).
After the fix all 17 build green and are included in the table above.

## T2.11 — PIO-era cleanup + stale-list decision record (2026-10-15)

Cleanup commit `51503ea` (companion `6bd4258`: the neopixel link-order fix it surfaced in
`waveshare_ledarray` + `canopen_slave_led`) retired the PIO-era layout; all S3 apps
re-verified green.

- **Moved to new structure:** `lib/{AS5311-PWM, ESP32-TWAI-CAN-master, ESP32_CanOpenNode}` +
  `copy_PinConfig.py` → `legacy/`; `custom_partition*.csv` → `partitions/`; per-app
  `PinConfig.h` → app-local `apps/<app>/main/` (wired via `UC2_APP_MAIN_DIR`).
  `platformio.ini` stays at root until T6.4 (the legacy pipeline ships until the T6.1–T6.3 cutover).
- **Deleted (D3-confirmed stale):** 96 top-level `sdkconfig*` (all but the 3 kept
  `sdkconfig.defaults[.esp32/.esp32s3]`) and 46 `main/config/*/PinConfig.h` PIO shadows —
  the `UC2_*`, `seeed_xiao_*`, `waveshare_*`, `esp32dev`, `m5stack_dial` env variants
  (full list: `git show --stat 51503ea`). The BT boards' `UC2_*` / `UC2_canopen_master` /
  `UC2_canopen_standalone_v4` shadows are removed now; their Phase-3 app-local PinConfig.h
  is created fresh at T3.x.

## Reproduction

```powershell
# builds (resumable; one log per env in $env:TEMP\uc2_baselines)
powershell -NoProfile -ExecutionPolicy Bypass -File baselines_run.ps1
# table
C:\Python314\python.exe baselines_parse.py
```

`pio` is not on PATH: `C:\Users\troop\.platformio\penv\Scripts\pio.exe`.
