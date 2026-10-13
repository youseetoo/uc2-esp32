# UC2-ESP32: Port to C + ESP-IDF — Orchestrator Execution Plan (index, v2 split)

> **Pure ESP-IDF 6.1 build. No PlatformIO. No shims. Direct IDF calls.**

The high-level plan **`DOCS/PORT_PLAN_C_ESPIDF.md`** (v2, 2026-07-24) is **SUPERSEDED by
this steps plan** — its `uc2_hal` shim approach was dropped in favor of converting `main/`
to direct IDF calls (T1.3; the T1.2 `uc2_arduino_shim` component is deleted, commit 095f687).
Its repo-fact sections remain the provenance record: 33 module dirs / 65 cpp files, ~100
`Arduino.h` includes, cJSON the only JSON lib, ESPAsyncWebServer/ArduinoJson/LVGL unused,
CANopenNode C stack with IDF TWAI driver in `lib/ESP32_CanOpenNode`, Ws2812Rmt first-party
bare-metal RMT with FAS owning the shared RMT IRQ, 21 PIO envs (17 in CI), 4 GitHub
workflows, 96 stale `sdkconfig*` + 45 stale `main/config` dirs, partial IDF scaffolding on
`main_espidf`, 168-test Unity suite on `main_pioclean_mocks_tests`.

**This file is the index/spine only.** Every step lives in its own self-contained md under
`steps/` — hand off **only that step file** (plus this index for sequencing context) to the
executor agent.

## 1. How to use (orchestrator protocol)

1. **One file per step.** `steps/Tx.y.md` contains goal, context, actions/files, commands,
   acceptance criteria, effort, rollback, and decision gates — enough to execute without
   reading anything else. Read a `refs/` file only when the step says to.
2. **Sequencing**: work top-to-bottom in §2. A task may start only when (a) all its `←`
   dependencies are done, and (b) every `(D#)` gate is resolved (§3).
3. **Resume across sessions**: checkboxes exist in the index AND each step file. When a
   step's acceptance criteria pass, flip both, and commit the docs together with the code.
4. **Branch model**: all work on `port/idf-c` (branched from `main_espidf`, which holds the
   partial IDF scaffolding). **IDF-only build** (no PIO, no dual-build): the code is
   converted to direct IDF (no shim layer). `main/` is deleted in T6.5 once every board
   is verified on IDF.
5. **Size budget**: every app, at every phase exit, within **±10 %** of its PIO baseline
   flash/RAM (`DOCS/baselines.md`, T0.3). A breach is a stop-the-line event.
6. **`[HW]`** = needs a physical board; queue it to the user, never skip its acceptance.
7. **Reference docs**: `refs/env-app-mapping.md` (all 21 env→app tables + Kconfig mechanics),
   `refs/component-layout.md` (full tree, app skeleton, source gating),
   `refs/arduino-idf-mapping.md` (Arduino→IDF table + leaf-first port order),
   `refs/ci-cutover.md` (per-workflow cutover + single-build gate mechanics),
   `refs/test-strategy.md`, `refs/risk-rollback.md`.
8. **File-tool discipline**: this repo's docs are edited with file tools only (no shell for
   read/write — PowerShell 5.1 mangles BOM/non-ASCII); shell/PowerShell is allowed for
   builds and `git`.

## 2. Step spine

### Phase 0 — Baseline & decisions (1–2 d)
- [x] **T0.1** [T0.1.md](steps/T0.1.md) — Resolve decisions D1–D7 with the user · 1 h · *(D1–D7)* — **done 2026-10-08: all 7 = plan recommendations**
- [x] **T0.2** [T0.2.md](steps/T0.2.md) — Branch `port/idf-c` off `main_espidf`; reference PIO build green · 15 min · ← T0.1
- [x] **T0.3** [T0.3.md](steps/T0.3.md) — PIO size baselines for all 17 CI envs → `DOCS/baselines.md` · 2 h · ← T0.2
- [ ] **T0.4** [T0.4.md](steps/T0.4.md) — `[HW]` serial JSON + CANopen trace baselines → `DOCS/baselines/` · 4 h · ← T0.2
- [x] **T0.5** [T0.5.md](steps/T0.5.md) — *(D5)* Adopt 168-test Unity suite from `main_pioclean_mocks_tests` as host tests · 1–2 d · ← T0.2 — **done 2026-10-08 (2bd1283, 8a79a6b): 154/0/5 on MSVC**

### Phase 1 — IDF skeleton + first app (2–4 d)
- [x] **T1.1** [T1.1.md](steps/T1.1.md) — *(D1)* Distilled `sdkconfig.defaults` + target overlays · 0.5 d · ← T0.1, T0.2 — **done 2026-10-08: root 4.6 KB + esp32/s3 overlays, all option names verified vs IDF 6.1**
- [x] **T1.2** [T1.2.md](steps/T1.2.md) — Move the 5 already-C components verbatim + wire top-level CMake · 0.5 d · ← T1.1 — **done 2026-10-10: 5 components moved verbatim (+ per-comp CMakeLists, cjson submodule repointed); scratch_t12 links all five clean under idf.py (esp32s3); PIO reference green, Δ −0.002 %**
- [x] **T1.3** [T1.3.md](steps/T1.3.md) — Convert `main/` to direct IDF (no shim; removes T1.2's `uc2_arduino_shim`) · 3–5 d · ← T1.1 — **done 2026-10-13: laser on direct IDF, 359,616 B / 73 % free; residual 3 vestigial `#include "Arduino.h"` in dormant headers (clean in Ph2/4)**
- [x] **T1.4** [T1.4.md](steps/T1.4.md) — Feature Kconfig symbol set + `uc2_app` glue + source-gating pattern · 0.5 d · ← T1.3
- [x] **T1.5** [T1.5.md](steps/T1.5.md) — *(D3, D7)* App #1 scaffold: `apps/canopen_slave_laser` + `boards/` PinConfig · 0.5 d · ← T1.4 — **done 2026-10-13: scaffold + build.sh + sdkconfig; laser green 359,616 B**
- [x] **T1.6** [T1.6.md](steps/T1.6.md) — Port the laser app to direct IDF (no shim) · 1–2 d · ← T1.5 — **done 2026-10-13: laser green 359,616 B (app-local `uc2_modules`; per-controller split = T1.9)**
- [ ] **T1.7** [T1.7.md](steps/T1.7.md) — *(D1)* Build + flash + `[HW]` laser/CANopen smoke; size ±10 % · 0.5 d + `[HW]` · ← T1.6
- [ ] **T1.8** [T1.8.md](steps/T1.8.md) — *(D6)* CI `idf-build` job (laser only) alongside PIO matrix · 0.5 d · ← T1.7
- [x] **T1.9** [T1.9.md](steps/T1.9.md) — Split `main/src/` into per-controller components (for app-sharing) · 1–2 d · ← T1.6 — **done 2026-10-13: 5 shared comps uc2_core/uc2_serial/uc2_canopen/uc2_io/uc2_periph; main/src untouched**

### Phase 2 — All non-BT boards (1.5–3 w; each task is an individually verifiable board unit)
- [ ] **T2.1** [T2.1.md](steps/T2.1.md) — `apps/xiao_ledservo` + `apps/xiao_ledring` · 2–3 d · ← T1.8, T1.9
- [ ] **T2.2** [T2.2.md](steps/T2.2.md) — `apps/waveshare_ledarray` (WS2812 RMT ch 0 on S3) · 1–2 d · ← T1.8
- [ ] **T2.3** [T2.3.md](steps/T2.3.md) — `apps/canopen_slave_gpio` (gpio_can + I2cBridge) · 1–2 d · ← T1.8
- [ ] **T2.4** [T2.4.md](steps/T2.4.md) — `apps/canopen_slave_led` (LED + thermal + laser) · 2 d · ← T1.8
- [ ] **T2.5** [T2.5.md](steps/T2.5.md) — `apps/canopen_slave_galvo` (MCP4822 SPI) · 2–3 d · ← T1.8
- [ ] **T2.6** [T2.6.md](steps/T2.6.md) — *(D2)* `apps/canopen_slave_accelmotor` · 2–3 d · ← T2.3
- [ ] **T2.7** [T2.7.md](steps/T2.7.md) — *(D2)* `apps/canopen_slave_motor` (FAS RMT-IRQ + `[HW]` step timing; 5 per-axis builds) · 3–5 d · ← T2.6
- [ ] **T2.8** [T2.8.md](steps/T2.8.md) — `apps/canopen_bridge_ptz` (RS-485 Pelco-D) · 2 d · ← T1.8
- [ ] **T2.9** [T2.9.md](steps/T2.9.md) — `apps/xiao_odmr` (largest S3 app; IS31FL3741 check) · 3–5 d · ← T2.7
- [ ] **T2.10** [T2.10.md](steps/T2.10.md) — `apps/canopen_bridge_ps4_usbhost` (esp_usb_host_hid, no BT) · 1–2 d · ← T1.8
- [ ] **T2.11** [T2.11.md](steps/T2.11.md) — Cleanup: stale `sdkconfig*`, `main/config`→`boards/`, `partitions/`, `legacy/`, retire copy script · 1 d · ← T2.1–T2.10
- [ ] **T2.12** [T2.12.md](steps/T2.12.md) — CI `idf-build` matrix expansion + per-board retirement checklists · 0.5 h/board · ← T2.1–T2.11
- [ ] **T2.13** [T2.13.md](steps/T2.13.md) — Phase-2 gate summary · 0.5 d · ← T2.12

### Phase 3 — Bluetooth boards (1–2 w; highest risk)
- [ ] **T3.1** [T3.1.md](steps/T3.1.md) — BT/PS4 inventory + `[HW]` internal-RAM headroom baseline · 1 d · ← T2.13
- [ ] **T3.2** [T3.2.md](steps/T3.2.md) — *(D2)* Pick BT stack (recommend Bluedroid classic-only); `DOCS/bt.md` · 1–2 d · ← T3.1
- [ ] **T3.3** [T3.3.md](steps/T3.3.md) — Port `uc2_bt` (BtController/HidController/esp_hid_gap.c) onto IDF BT · 3–5 d · ← T3.2
- [ ] **T3.4** [T3.4.md](steps/T3.4.md) — `apps/uc2_2` + `uc2_3` + `uc2_wemos` + `uc2_4` (BT boards) · 3–5 d · ← T3.3
- [ ] **T3.5** [T3.5.md](steps/T3.5.md) — `apps/canopen_master` (all controllers, RAM-tight) · 3–5 d · ← T3.4
- [ ] **T3.6** [T3.6.md](steps/T3.6.md) — `apps/canopen_standalone_v4` (OTA sender+receiver off) · 2 d · ← T3.5
- [ ] **T3.7** [T3.7.md](steps/T3.7.md) — `[HW]` PS4 pairing/trackpad + RAM headroom verification on all BT boards · 2 d · ← T3.4–T3.6
- [ ] **T3.8** [T3.8.md](steps/T3.8.md) — CI `idf-build` matrix += BT apps + retirement checklists · 0.5 d · ← T3.7

### Phase 4 — C conversion of first-party code (2–4 w; parallelizable per module)
- [ ] **T4.1** [T4.1.md](steps/T4.1.md) — C-ify leaf math/state: `pid/ axis/ signal/ state/ config/` · 1–2 d · ← T3.8
- [ ] **T4.2** [T4.2.md](steps/T4.2.md) — C-ify `uc2_core` + `uc2_serial` · 2–3 d · ← T4.1
- [ ] **T4.3** [T4.3.md](steps/T4.3.md) — C-ify `uc2_motor` (FocusMotor/homing/stage scan) + `Ws2812Rmt` · 2–3 d · ← T4.1
- [ ] **T4.4** [T4.4.md](steps/T4.4.md) — C-ify `uc2_canopen`; split CANopenModule (138 KB) + DeviceRouter (104 KB) · 1–2 w · ← T4.2
- [ ] **T4.5** [T4.5.md](steps/T4.5.md) — *(D2)* C-ify `uc2_bt` (Bt/Hid) + PSController leaf decision · 2–3 d · ← T3.7, T4.2
- [ ] **T4.6** [T4.6.md](steps/T4.6.md) — C-ify `uc2_periph` (i2c/sensors/dac/tca95xx/onewire/thermal) · 1–2 d · ← T4.1
- [ ] **T4.7** [T4.7.md](steps/T4.7.md) — C-ify `uc2_dial`+`uc2_scanner`+`uc2_pixel_buf`+`uc2_joystick`; first-party audit + C-only CI gate job · 1–2 d · ← T4.2–T4.6
- [ ] **T4.8** [T4.8.md](steps/T4.8.md) — Phase-4 gate: zero C++ in `uc2_*`; suite green; T2.7/T3.7 re-smokes · 0.5 d · ← T4.7

### Phase 5 — Vendor libraries + final purge (1–4 w; parallelizable; T5.2 skipped by default per D2=(b))
- [ ] **T5.1** [T5.1.md](steps/T5.1.md) — *(D2=(b))* C-ify vendor leaves: accelstepper, onewire_ds18b20, encoder, tca95xx, DAC_Module (upstream/ ref copy per lib; TMC+FAS stay C++ leaves) · 1–2 w · ← T4.8
- [ ] **T5.2** [T5.2.md](steps/T5.2.md) — *(D2=(b))* **SKIPPED BY DEFAULT** — FAS stays a C++ leaf; execute only on user revisit · 1 w · ← T4.3
- [ ] **T5.3** [T5.3.md](steps/T5.3.md) — NeoPixel → C RMT LED-strip driver in `uc2_pixel_buf` (WS2812 timing identical) · 2–4 d · ← T4.8
- [ ] **T5.4** [T5.4.md](steps/T5.4.md) — *(D4)* M5GFX 3 envs: port to IDF as C++ leaves (no PIO) + `DOCS/m5gfx.md` · 1–2 w · ← T4.7
- [ ] **T5.5** [T5.5.md](steps/T5.5.md) — Final `Arduino.h` purge + shim-component removal + `DOCS/cpp-leaves.md` freeze · 1 d · ← T5.1–T5.4

### Phase 6 — CI cutover, legacy removal, final gate (~1–2 w)
- [ ] **T6.1** [T6.1.md](steps/T6.1.md) — *(D6)* Per-board cutover wave 1: 11 non-BT boards (4-item checklist gate, `refs/ci-cutover.md` §4) · 1–2 d · ← T5.5
- [ ] **T6.2** [T6.2.md](steps/T6.2.md) — *(D6)* Per-board cutover wave 2: 6 BT boards + whole-rig constellation check · 1–2 d · ← T6.1
- [ ] **T6.3** [T6.3.md](steps/T6.3.md) — Retire PIO dev matrix (CI single-system); `build-frame.yaml` → IDF list; odmr/registry workflows stay unchanged · 0.5 d · ← T6.2
- [ ] **T6.4** [T6.4.md](steps/T6.4.md) — `platformio.ini` → `legacy/` (all envs ported — moves wholesale); README IDF-only · 0.5 d · ← T6.3
- [ ] **T6.5** [T6.5.md](steps/T6.5.md) — *(D6)* Delete `main/` (uniqueness audit first; keep `manifest.json` if still consumed) · 0.5 d · ← T6.4
- [ ] **T6.6** [T6.6.md](steps/T6.6.md) — *(D2)* Final C++-leaf sweep vs `DOCS/cpp-leaves.md` + repo hygiene · 1 d · ← T6.5
- [ ] **T6.7** [T6.7.md](steps/T6.7.md) — Phase-6 gate: real release tag + full-rig `[HW]` verification + merge `port/idf-c` → `main` · 0.5 d · ← T6.6

**Total ≈ 6–10 weeks** (≈ 4–6 if D2 = C++-leaves and BT goes smoothly). Critical path:
T0 → T1 → T2 → T3; Phases 4–5 run per-module and can be interleaved from T3.8.

## 3. Decisions table (mirrors §7 of PORT_PLAN_C_ESPIDF.md)

Status: **ALL 7 CONFIRMED 2026-10-08** — the user accepted every plan recommendation
(T0.1 satisfied). "Gates" lists tasks that were gated by each row.

**Confirmed answers (2026-10-08):**
- **D1** = IDF **6.1** (`M:\.espressif\v6.1\esp-idf`); CI pins the same version.
- **D2** = **(b)** C core + C++ leaf set **`fastaccelstepper` + `tmc_stepper`** behind
  C backend headers (T5.2 therefore **skipped by default**; T5.1 scope excludes TMC).
- **D3** = **all 21 envs** in scope; `esp32dev` stays dev-only (no release app, not in CI).
- **D4** = **(b)** all three M5GFX envs (`m5stack_dial`, `m5stack_dial7`, `waveshare_d7`)
  **ported to IDF** as C++ leaves (M5GFX + M5Unified + M5Dial) — no PIO.
- **D5** = **adopt** the 168-test Unity suite from `main_pioclean_mocks_tests`.
- **D6** = **IDF-only** CI (no dual-build, no PIO). The code is converted to direct IDF
  (no shim layer). `main/` is deleted in T6.5.
- **D7** = **single repo** with `apps/`.

| ID | Decision | Plan recommendation | User must confirm | Gates (tasks) |
|----|----------|--------------------|-------------------|---------------|
| **D1** | IDF version | **6.1** (installed at `M:\.espressif\v6.1\esp-idf`; GrowControl migrated to 6.1 on 2026-10-06) vs 5.5.x LTS | Version number; whether CI uses the same version | T1.1, T1.7, T1.8 (all IDF builds); CI runner setup in T6.1 |
| **D2** | Strictness of "C" | **(b)** C core + small C++ leaf set (`fastaccelstepper`, `tmc_stepper`) behind C backend headers; revisit leaves later — vs (a) 100 % C incl. rewriting FAS/TMC | (a) vs (b); if (a), accept 2–3 extra weeks and RMT/timing risk | T2.6, T2.7 (FAS/TMC strategy), T3.2 (BT stack), T5.1, T5.2 |
| **D3** | Board scope | All 21 envs (17 CI + ptz + m5stack_dial + 7"; `esp32dev` dev-only) **or** only boards still shipped | Exact board list; fate of `esp32dev` | Which of T2.x/T3.x tasks exist; T2.11 cleanup list |
| **D4** | M5GFX boards (m5stack_dial, waveshare_7inch) | **Port to IDF** as C++ leaves (M5GFX + M5Unified + M5Dial) — no PIO | — (confirmed: port to IDF) | T5.4 (port M5GFX to IDF) |
| **D5** | Test suite | **Adopt** the 168-test Unity suite + mocks from `main_pioclean_mocks_tests` vs re-derive smaller | Adopt vs re-derive | T0.5, T4.8, T6.3 |
| **D6** | Cutover strategy | **IDF-only** CI (no dual-build, no PIO). The code is converted to direct IDF (no shim layer). `main/` is deleted in T6.5 | — (confirmed: IDF-only) | T1.8, T2.12, T3.8, T6.1, T6.5 |
| **D7** | Repo shape | **Single repo with `apps/`** (release pipeline stays one workflow) vs split firmware repos | Single vs split | All layout tasks; if split, this whole plan's paths change (re-plan needed) |

**Decisions gating Phase 1:** all resolved 2026-10-08, revised for IDF-only — D1 (6.1), D3 (all 21 envs;
`canopen_slave_laser` is app #1), D6 (IDF-only, no dual-build), D7 (single repo). D2/D4/D5 are
confirmed as well (Phases 3–5 unblocked). **No decision gates remain: T0.2 onward can
start immediately.**

## 4. Canonical commands (used verbatim by step files)

```bash
# IDF (D1 = 6.1): source export first
. M:/.espressif/v6.1/esp-idf/export.ps1        # Windows/PowerShell (or export.sh)

# Build one app (CWD-independent):
idf.py -D IDF_TARGET=esp32s3 -D PROJECT_DIR=apps/canopen_slave_laser build
# (each app dir gets a build.sh wrapper doing exactly this — created in T1.5)

# Build speed-ups (verified 2026-10-13, esp32s3 laser app):
#   - INCREMENTAL: `idf.py build` is incremental — do NOT nuke `build/` between iterations.
#     Only `Remove-Item build -Recurse -Force` after CMakeLists/REQUIRES/file-MOVE changes (a
#     stale CMake cache then gives "Cannot find source"). Include-only changes rebuild ~10–30 s,
#     not the ~120 s full build.
#   - CCACHE: set `CCACHE_ENABLE=1` (env) — big win on repeated builds across the 21 apps.
#   - Windows/PowerShell 5.1: no `&&` (chain with `;`). Full line:
#     . "M:\.espressif\v6.1\esp-idf\export.ps1" | Out-Null; cd <app>; idf.py build

# PIO is not part of the build (IDF-only). Size comparisons use the reference
# baselines already captured in DOCS/baselines.md (T0.3).

# Host tests:
make test            # = test/native/run.sh + test/unity/run.sh
make canopen-check   # codegen drift
```

## 5. Verification spot-check log (provenance)

- `platformio.ini` (908 lines): 21 envs; per-env feature flags transcribed into
  `refs/env-app-mapping.md`; `_release`/`_debug` variants confirmed; `UC2_canopen_master`
  has **no** USE_FASTACCEL/USE_ACCELSTEP flag; `UC2_WEMOS` extends `UC2_3_default`; S3 envs
  carry `CORE_DEBUG_LEVEL=5` in several release builds (parity requirement); xiao_odmr has
  IS31FL3741 in lib_deps. Exactly 10 `UC2_canopen_*_release` envs (platformio.ini lines
  503–900, incl. `bridge_ptz_release`).
- `main/CMakeLists.txt` (207 lines): source→feature map extracted — the authoritative
  gating logic for `refs/component-layout.md` §4.3; `REQUIRES driver`; main.cpp and
  DeviceRouter/RoutingTable always compiled.
- `build-and-release.yaml`: 17 matrix entries; merge_bin recipe (bootloader @ 0x0/0x1000,
  partitions @ 0x8000, boot_app0 @ 0xe000, app @ 0x10000, dio/40m/4MB); release + mirror
  jobs artifact-driven (build-system agnostic).
- `build-frame.yaml`: prefix matrix (UC2_3, seeed_xiao_esp32s3, UC2_canopen→`_release`
  envs); skips `UC2_canopen_xiao_base` (line 93: fails in CI on missing CAN lib files);
  motor per-axis loop via `PLATFORMIO_BUILD_FLAGS=-DCAN_AXIS_ID=CAN_ID_MOT_{X,Y,Z,A}`;
  artifact names `esp32_<ENV>[_mot<A>][_merged].bin`; fresh-bootloader merge rule.
- `.github/workflows/`: exactly 4 files confirmed — `build-and-release.yaml` (15,957 B),
  `build-frame.yaml` (15,864 B), `build-odmr-external.yaml` (7,398 B),
  `canopen-registry-check.yml` (1,642 B); the "unchanged" list in
  `refs/ci-cutover.md` matches these names.
- `main/src/bt`: BtController/HidController C++ + `esp_hid_gap.c` (30 KB, C).
- `main/src/canopen`: CANopenModule.cpp 138,340 B; DeviceRouter.cpp 104,132 B.
- `lib/`: ESP32_CanOpenNode (C), uc2_od, esp_usb_host_hid, PSController, DAC_Module,
  ESP32-TWAI-CAN-master + AS5311-PWM (unused → `legacy/`).
- `main/config/`: 45 dirs confirmed; live set = one per app in `refs/env-app-mapping.md`
  (plus the stale list in `refs/component-layout.md`).
- `test/`: `native/run.sh` + `test_stage_scan_order.cpp` + 2 orphaned PIO test files;
  branches present: `main`, `main_espidf`, `main_pioclean`, `main_pioclean_mocks_tests`,
  `ps4_trackpad`.
- `.gitmodules`: single submodule `main/cJSON` (DaveGamble/cJSON master) → repoint to
  `components/cjson` in T1.2.
- Top-level `CMakeLists.txt`: 3 lines (minimal IDF scaffolding on `main_espidf`).
