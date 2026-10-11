# Ref: CI cutover per workflow + gate mechanics

Index: `../PORT_PLAN_STEPS.md`. Owner steps: T1.8, T2.12, T3.8, T6.1–T6.5.

## 1. `build-and-release.yaml` (17-env matrix → IDF)

- **Phase 1 (T1.8)**: add `idf-build` job (laser only) — runs on `pull_request` + `push` to
  `main` + tags + dispatch, alongside the unchanged `build-all-firmware` (PIO) job.
  Release jobs (`create-release`, `publish-binaries-repo`) stay on PIO artifacts.
- **Phase 2/3 (T2.12, T3.8)**: grow the `idf-build` matrix one app per verified board.
  The job builds each app (`idf.py -D IDF_TARGET=<t> -D PROJECT_DIR=apps/<app> build`),
  produces `bootloader.bin`, `partitions.bin`, `app.bin` under the build dir, and runs the
  **same esptool `merge_bin`** recipe (bootloader @ 0x0 (S3) / 0x1000 (ESP32), partitions @
  0x8000, boot_app0 @ 0xe000 — from the IDF `esp_rom` component, not `~/.platformio`, app @
  0x10000) → `build/fw-images/<board_id>-idf.bin`. Artifact names keep the `<board_id>`
  convention with an `-idf` suffix until cutover.
- **Cutover (T6.1, D6)**: when every matrix board is verified, replace the PIO job with the
  IDF matrix: drop `-idf` suffix, delete the PIO setup/cache steps, keep **unchanged**:
  `create-release` (index JSON), `publish-binaries-repo` (mirror + jsDelivr rewrite),
  release body, `API_TOKEN_GITHUB`. The workflow becomes: checkout → IDF setup → build
  matrix (17 apps, `fail-fast: false`) → merge_bin + manifest per app → upload → release →
  mirror. `bootloader.bin`/`partitions.bin` come from each app's own build (matches the
  build-frame "freshly compiled bootloader" rule).
- **IDF runner setup (pick at T1.8, pin in workflow)**: prefer a self-hosted runner or
  container image with IDF (per D1) + ccache; cache `~/.espressif` and `build/` dirs per
  app. If using the `espressif/setup-espressif` action, pin the exact IDF version string.

## 2. `build-frame.yaml` (FRAME firmware server image)

- **Cutover (T6.2)**: replace the "Extract environment list" + "Build all environments" PIO
  logic with an explicit **app list** (no more `pio project config` scraping):
  `UC2_3`→`apps/uc2_3`, `seeed_xiao_esp32s3`→`apps/xiao_odmr`, and the `UC2_canopen` prefix
  → `apps/canopen_{master,standalone_v4,slave_motor,slave_accelmotor,slave_galvo,
  slave_laser,slave_led,slave_gpio,bridge_ps4_usbhost,bridge_ptz}` — verified: exactly these
  10 `UC2_canopen_*_release` envs exist in `platformio.ini` (lines 503–900); the workflow
  also skips `UC2_canopen_xiao_base` (line 93: fails in CI on missing CAN lib files) — keep
  that exclusion in the app list.
  **Keep the per-axis motor loop**: for `canopen_slave_motor` build 5 times with
  `UC2_CAN_AXIS_ID` set to `""/X/Y/Z/A` (pass via `sdkconfig.defaults` substitution or
  `idf.py -D UC2_CAN_AXIS_ID=…`); keep artifact names `esp32_<ENV>_mot<A>[_merged].bin`
  naming-compatible (the FRAME server downloads by name). Merge recipe identical to the PIO
  one (`--flash_mode keep` semantics: use the app's own bootloader; the IDF bootloader
  already bakes flash params — use `esptool merge_bin` with explicit params read from
  `sdkconfig`, as the workflow already does for chip/boot-addr selection). GHCR image +
  release jobs unchanged.
- The Dockerfile at repo root (server image) is firmware-build-agnostic — unchanged.

## 3. Unchanged workflows

- `build-odmr-external.yaml` — external repo, its own PIO project: **untouched**.
- `canopen-registry-check.yml` — Python codegen drift check: **untouched**; keep running as
  a PR gate (guards `uc2_od` codegen, which the port does not change).

## 4. Per-board retirement checklist (D6 — the gate that lets a board cut over)

A board's PIO env may be removed from the release matrix only when ALL of:
1. `idf.py build` green in CI for that app (≥ 3 consecutive green PRs/tags);
2. `[HW]` smoke checklist (§5) passed on the IDF binary;
3. size within ±10 % of the `DOCS/baselines.md` row;
4. one tagged release built with the IDF binary flashed to ≥ 1 user/hardware unit without
   regression report (or the user explicitly waives, recorded in `DOCS/baselines.md`).

Retirement = remove the env from the `build-all-firmware` matrix + delete its `_release`
env from `platformio.ini` (keep non-release variants until T6.6) + note in the checklist.

## 5. Per-board smoke checklist (used by T1.7, T2.x, T3.7, T4.x spot-checks)

1. Boots to the same startup banner/log level as PIO baseline.
2. Serial JSON: `{"task":"/get_state"}` (or the board's root get) returns the same key set
   as the baseline capture; one act round-trip per enabled controller.
3. CANopen boards: NMT startup → OPERATIONAL (master-verified); SDO read of node id + one
   feature SDO; TPDO sync observed.
4. Board-specific: laser fires / LED strip pattern / motor jog+home (step count) / galvo
   pattern / GPIO TPDO sniff / PTZ key dispatch / PS4 pairing (BT boards) / USB-HID DS4.
5. Size + internal-heap-free within budget (±10 % / baseline band).
6. 5-minute soak: no watchdog reset, no log spam, RAM stable.

## 6. CI gate mechanics (throughout the port)

- Invariant: **every commit on `port/idf-c` builds green in IDF** (single build system).
  No dual-build; PIO is not part of the CI.
- The `idf-build` job grows one app per verified board (T1.8 → T2.12 → T3.8).
- Escalation: if a board fails its §4 checklist twice, park it (record reason),
  continue with others; the parked-board list is tracked in `DOCS/baselines.md`.
