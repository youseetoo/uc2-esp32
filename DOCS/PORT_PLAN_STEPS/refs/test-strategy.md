# Ref: Test strategy

Index: `../PORT_PLAN_STEPS.md`. Owner steps: T0.5, T4.7, T4.8, T6.3.

## 1. Suites

1. **168-test Unity suite + mocks** (from `main_pioclean_mocks_tests`, D5): adopted in
   T0.5; lives in `test/unity/`; runs **on the host** (no target build) via Unity + the
   suite's existing mocks (CANopen/TWAI). Re-runs in CI as a PR gate from T0.5 onward
   (job `host-tests`: `make test` = `test/native/run.sh` + `test/unity/run.sh`).
2. **Native host tests** (`test/native/run.sh`, C++11): kept as-is through Phase 4; after
   T4.6 the harness may be C-ified (either acceptable).
3. **C-only compile gate** (T4.7): a CI job compiling every `uc2_*` source with
   `gcc -x c -std=c99 -Wall -Wextra -Werror` against host stubs (or as a separate target
   component build) so C++ creeps in loudly.
4. **`make canopen-check`** (existing `Makefile` target + `canopen-registry-check.yml`):
   guards OD/EDS codegen — unchanged, always on.
5. **Target-level tests (optional, low priority)**: `testFeedbackStepper` rig stays on PIO
   until the T6.4 decision; no IDF `component_test` required for v1 cutover.

## 2. Per-phase test gates

| Phase exit | Gate (must be green before the phase is marked done) |
|---|---|
| 0 | `make test` green on clean machine (168 + native); baselines committed |
| 1 | laser app: `make test` green; `[HW]` T1.7 smoke; idf CI job green on PR |
| 2 | every T2.x: app builds in IDF (legacy PIO pipeline green until T6.1); suite green; `[HW]` board smoke; sizes ±10 % |
| 3 | suite green; BT smoke (pairing + trackpad); RAM headroom within baseline band |
| 4 | C-only gate green; suite green against C code; zero C++ in `uc2_*`; laser+motor+master spot-smokes |
| 5 | suite green; leaf list final (only D2-chosen components contain C++); LED/motor boards smoke-identical |
| 6 | tagged release artifacts identical in shape to last PIO release; Web Flasher verified `[HW]` |

## 3. Regression comparison

A/B against `DOCS/baselines/` at every `[HW]` gate: serial transcripts (key-set diff), CAN
traces (state-machine sequence), step-timing traces (T2.7), RAM headroom (T3.7). Any diff
beyond the documented delta (log format from ESP_LOG vs Arduino `log_*` is acceptable;
protocol bytes are not) is a stop-the-line event.
