# uc2-esp32: Axis Feedback & Closed-Loop Design (v2 — clean rewrite)

**Scope:** new `main/src/axis/` module replacing the current `encoder/` closed-loop code; CANopen slave motor node (`UC2_canopen_slave_motor`), one stepper + one AS5311 encoder per node.
**Approach:** clean-slate rewrite, not incremental repair. Old code stays behind a build flag until the new path is validated on hardware.

---

## 0. Constraints fixed in this round

| # | Constraint | Consequence for the design |
|---|---|---|
| 1 | **Only A/B pins wired.** No SSI/PWM, no index, no magnet-health lines. | The former WP5 (AS5311 absolute/index/health) is **dropped**. No absolute reference, no magnet diagnostics. Divergence detection and the watchdog must carry the whole fault-detection load. |
| 2 | **All firmware units are steps.** µm conversion happens in Python. | Every API, OD entry and event is in **steps** (or step-equivalents). Encoder counts are internal; exposed as diagnostics only. |
| 3 | **Microstep ratio may change.** | Calibration must be stored *with* the microstep setting it was taken at, and rescaled or invalidated when it changes. |
| 4 | **Do not touch FastAccelStepper's RMT/PCNT backend.** | Motor step generation stays exactly as-is (FAS owns `PCNT_UNIT_0`). The encoder must live within remaining PCNT units. Keep the existing unit-reservation approach; only make it explicit. |
| 5 | **Need async notification on lost steps / stall.** | Dedicated notification layer: CANopen **EMCY** + JSON event, not poll-only. |
| 6 | **Unified interface, invisible to ImSwitch.** | Move API unchanged. Encoder presence changes only an internal mode. |
| 7 | **Full rewrite preferred.** | New `axis/` module; old `encoder/` retired at the end. |
| 8 | **Fast blocking-axis watchdog → stop.** | Independent of the PID, high-priority, cheap. |
| 9 | **Expose via uc2-rest / uc2-canopen.** | Python-side WP with callbacks matching existing idioms. |
| 10 | **2 mm pole-pair pitch.** | Sets the scale regime — see §2. |

---

## 1. Why calibration becomes the centre of gravity

With A/B only, the encoder is a **purely relative** counter that starts at 0 on every boot, exactly like the stepper. Nothing in the system knows:

- the **sign** relation between commanded step direction and count direction,
- the **scale** (counts per step),
- the **offset** between the two origins,
- the **backlash/slack** between them on reversal.

Everything else — divergence detection, correction, servo — is built on those four numbers. They cannot be hardcoded (the existing code contains three mutually contradictory constants: `0.3125 µm/step`, `2 µm/count`, and `mumPerStep = 12.444`, with comments doubting 2 vs 4 µm). **They must be measured.**

The good news: for **lost-step detection you only need the differential relation**, not an absolute one. Boot-time origin alignment is therefore trivial — zero both counters together at startup and track error *relative to that*. Absolute positioning still requires homing, exactly as today.

## 2. Scale regime at 2 mm pole pitch

AS5311 quadrature output over a 2 mm pole pair, decoded ×4, lands in the region of ~1 µm/count (a 512 ppr ABI configuration gives 2048 counts per 2 mm ≈ 0.98 µm/count; some configurations give twice that). At a typical 3200 steps/mm this puts `countsPerStep` somewhere near 0.3–0.6 — i.e. **coarser than one microstep**, which matters a lot:

- A single lost microstep is **below encoder resolution**. Detection thresholds must be set in counts, and single-step losses are only visible as accumulated error.
- The verify tolerance cannot be tighter than ±1–2 counts of quantisation noise.
- Correction moves should be computed from accumulated error, not per-step.

**Do not hardcode any of this.** The calibration routine (WP3) measures the true value and its scatter, and that measured scatter directly sets the detection thresholds.

---

## 3. Mode model — your proposal, confirmed and named

Your four modes are correct and map cleanly. Naming and precise semantics:

| Mode | Encoder | Behaviour | Position authority |
|---|---|---|---|
| `MODE_OPEN_LOOP` | absent or ignored | Exactly today's behaviour. FAS emits steps; done = steps emitted. | Steps |
| `MODE_MONITOR` | required | Move open-loop. Continuously compare counts vs expected. **Notify** asynchronously on stall/lost steps/divergence. Never corrects, never moves on its own. | Steps |
| `MODE_CORRECT` | required | Move open-loop, then verify. If `|error| > tol`, issue bounded corrective move(s) (slack-aware), then **adopt the encoder-derived position as truth**. | Encoder (after each move) |
| `MODE_SERVO` | required | PID velocity loop drives to target in count-space; motion completes when the encoder says so. | Encoder (continuous) |

Three refinements worth building in:

1. **`MODE_MONITOR` is the safe default** when an encoder is present. It's diagnostic-only, cannot make things worse, and gives you the dataset to tune `MODE_CORRECT` thresholds. Promote per-axis once trusted.
2. **`MODE_CORRECT` must re-sync the step counter, not just the bookkeeping.** "Set the position to the one by the encoder" means: convert encoder position → steps, then call FAS `setCurrentPosition()` so the *next absolute move* computes from the corrected origin. Updating only a struct field leaves a stale FAS origin and the error silently returns on the following move.
3. **Slack handling belongs in `MODE_CORRECT`.** Your ~30-step reversal difference is calibrated backlash (WP3), not drift. Apply it as a feed-forward on direction reversal *and* widen the post-reversal verify tolerance; otherwise every reversal triggers a spurious correction.

## 4. The interface ImSwitch sees

The design goal is that **ImSwitch's existing code path does not change at all**.

```
// unchanged — same signature, same semantics, works with or without encoder
moveTo(axis, targetSteps, speed, isAbsolute)
stop(axis)
home(axis)
getPosition(axis) -> steps
```

Everything new is **additive and optional**:

```
setAxisMode(axis, mode)              // OPEN_LOOP | MONITOR | CORRECT | SERVO
getAxisFeedback(axis) -> {           // all in STEPS
    commandedSteps, measuredSteps, positionErrorSteps,
    mode, health, fault, calibrated, referenced }
resetAxis(axis, policy)              // clear fault + re-establish origin
calibrateAxis(axis, params)          // measure sign / scale / backlash
onAxisEvent(callback)                // async: STALL, LOST_STEPS, DIVERGENCE, CAL_INVALID
```

Encoderless axes report `measuredSteps == commandedSteps`, `positionErrorSteps == 0`, `mode == OPEN_LOOP`. The same calls work; nothing branches on encoder presence at the ImSwitch level.

### Core state (all step-based)

```
struct AxisFeedback {
    int32_t  commandedSteps;      // FAS position (authority in OPEN_LOOP)
    int32_t  measuredSteps;       // encoder counts -> steps via calibration
    int32_t  positionErrorSteps;  // measured - commanded, relative to origin
    int64_t  rawCounts;           // diagnostics only
    uint8_t  mode, health, fault;
    bool     calibrated;          // valid sign+scale for current microstepping
    bool     referenced;          // homed (absolute moves allowed)
};

enum AxisFault { NONE, STALL, LOST_STEPS, DIVERGENCE, TIMEOUT,
                 CAL_INVALID, CAL_FAILED, ENC_NOISE };
```

### Calibration record (persisted in NVS)

```
struct AxisCalibration {
    int32_t countsPerStep_q16;   // Q16.16 fixed point
    int8_t  countSign;           // +1 / -1 : step dir vs count dir
    int32_t backlashCounts;      // measured reversal slack
    uint16_t microstepsAtCal;    // invalidate/rescale on change
    uint16_t residualScatter;    // measured noise -> sets tolerances
    uint32_t timestamp; uint8_t quality;
};
```

**Microstep-change rule:** on detecting `currentMicrosteps != microstepsAtCal`, rescale analytically (`countsPerStep *= microstepsAtCal / currentMicrosteps`), mark the calibration `derived/unverified`, and raise `CAL_INVALID` as a *warning* — motion continues, but re-calibration is advised. Do not silently keep a stale factor, and do not hard-block motion.

---

## 5. Calibration routine (design)

Run on demand from Python; results persisted. Requires free travel, away from endstops, with the watchdog active.

**Step 1 — Origin.** Zero encoder count and record current step position as the common origin. (Sufficient for relative error tracking.)

**Step 2 — Sign.** Command a modest move in `+` direction at low speed. Require `|Δcounts| > minCountsForValidity`; `countSign = sgn(Δcounts)`. Repeat in `−`; require consistency. Fail with `CAL_FAILED` if counts don't move (encoder dead / not wired) or signs disagree.

**Step 3 — Scale by regression, not a single move.** Command several lengths (e.g. 500 / 1000 / 2000 / 4000 steps) in both directions. Linear-regress `Δcounts` against `Δsteps`:
- **slope → `countsPerStep`**
- **intercept → offset/slack contribution**
- **R² → quality gate** (reject below threshold)
- **residual scatter → `residualScatter`**, which becomes the basis for verify tolerance and watchdog noise thresholds. This is the key output: thresholds derived from measured noise rather than guessed.

**Step 4 — Backlash.** Approach a point from `+`, reverse, and measure lost motion before counts resume. Repeat, average → `backlashCounts`. (This is your ~30-step slack, quantified.)

**Step 5 — Persist** with microstep setting and quality; emit result to Python.

Safety throughout: bounded travel, low speed, abort on watchdog trip or endstop.

---

## 6. Watchdog (fast blocking detection)

Independent of the PID so it protects **all** modes, including open-loop.

- Runs in the motor loop or a small high-priority task; only reads a counter, so it's cheap.
- Sample encoder every 5–10 ms while FAS reports motion and commanded velocity exceeds a minimum.
- **Stall trip:** `|Δcounts| < noiseThreshold` (from `residualScatter`) for `stallTimeoutMs` (~50–100 ms) while motion is expected → immediate `stopStepper()`, `fault = STALL`, async notify.
- **Lag trip:** `|positionErrorSteps| > lagLimit` while moving → `fault = LOST_STEPS`, stop, notify. Catches slipping (moving but behind) which the stall test misses.
- **Ramp guard:** gate on commanded velocity above a floor and allow a short grace window after start, so acceleration ramps below encoder resolution don't false-trip.
- Trip is latching: motion in that axis is refused until `resetAxis()`.

## 7. Async notification

**On CANopen:** emit an **EMCY** object (`0x80 + node-id`) with a UC2-specific error code per fault. EMCY is the standard, already-supported async channel — no new transport needed. Additionally map `positionErrorSteps` to a TPDO for continuous monitoring.

**Master → host:** `CANopenModule` receives EMCY, converts to a JSON event, and pushes it on the existing async serial channel in the same shape as current motor events.

**Standalone/serial:** the same JSON event directly.

Event payload (steps): `{ "axisEvent": { "axis": n, "fault": "STALL", "posErrSteps": -142, "commandedSteps": ..., "measuredSteps": ... } }`

---

## 8. Module layout (clean slate)

```
main/src/axis/
  AxisTypes.h          modes, faults, AxisFeedback, AxisCalibration
  EncoderBackend.{h,cpp}   A/B PCNT only; explicit unit; correct axis index
  AxisCalibration.{h,cpp}  routine + NVS persistence + microstep rule
  AxisWatchdog.{h,cpp}     fast stall/lag detection
  AxisNotify.{h,cpp}       EMCY + JSON async events
  AxisController.{h,cpp}   IAxis impl: modes, verify/correct, servo, reset
```

`FocusMotor` remains the stepper actuator layer; **FAS backend untouched**. Old `encoder/LinearEncoderController` + `PCNTEncoderController` are retired in WP10 behind `USE_LEGACY_ENCODER`.

---

## 9. Work packages

### WP1 — Module skeleton, step-unit policy, AxisFeedback
Create `axis/` with types and a per-axis `AxisFeedback` maintained every loop. Establish that **every public value is in steps**. Encoderless axes produce identity feedback so downstream code is uniform.

> **Claude Code prompt (WP1):**
> "Create a new module `main/src/axis/` in the uc2-esp32 firmware. Add `AxisTypes.h` defining: `enum AxisMode {OPEN_LOOP, MONITOR, CORRECT, SERVO}`, `enum AxisHealth {OK, DEGRADED, FAULT}`, `enum AxisFault {NONE, STALL, LOST_STEPS, DIVERGENCE, TIMEOUT, CAL_INVALID, CAL_FAILED, ENC_NOISE}`, `struct AxisFeedback {int32_t commandedSteps, measuredSteps, positionErrorSteps; int64_t rawCounts; uint8_t mode, health, fault; bool calibrated, referenced;}` and `struct AxisCalibration {int32_t countsPerStep_q16; int8_t countSign; int32_t backlashCounts; uint16_t microstepsAtCal, residualScatter; uint32_t timestamp; uint8_t quality;}`. Add `AxisController.{h,cpp}` exposing `moveTo(axis,targetSteps,speed,isAbsolute)`, `stop(axis)`, `setMode(axis,mode)`, `getFeedback(axis)`, `resetAxis(axis,policy)`. IMPORTANT: all public values are in STEPS; encoder counts are internal only. For axes without an encoder, set `measuredSteps = commandedSteps`, `positionErrorSteps = 0`, `mode = OPEN_LOOP` so callers need no branching. Do not modify FastAccelStepper or its RMT/PCNT backend. Wire `AxisController::loop()` into the existing motor loop. Keep the legacy `main/src/encoder/` code compiling untouched for now."

### WP2 — Encoder backend (A/B only), correct axis mapping
Rewrite the PCNT reader cleanly for A/B quadrature only. Keep FAS's `PCNT_UNIT_0` reservation as-is but make the encoder's unit explicit and config-driven. Fix the hardcoded `encoders[1]`/X-only assumption and the double direction inversion.

> **Claude Code prompt (WP2):**
> "Create `main/src/axis/EncoderBackend.{h,cpp}` as a clean A/B-quadrature-only encoder reader, replacing `main/src/encoder/PCNTEncoderController.cpp`. Requirements: (1) Do NOT change FastAccelStepper's step-generation backend — keep the existing reservation that leaves PCNT_UNIT_0 to FAS, but make the encoder's PCNT unit index an explicit config field instead of relying on ESP32Encoder's 'first free slot' behaviour. (2) Configure the encoder for the node's LOGICAL axis (`pinConfig.REMOTE_MOTOR_AXIS_ID` on CAN slaves) instead of hardcoding index 1 / X-axis. (3) Apply direction inversion in EXACTLY ONE place (backend, from pin config) — the old code inverted in both `getEncoderCount` and via `LinearEncoderData.encoderDirection`; that duplication must not survive. (4) Expose `int64_t getCount()`, `void zero()`, `bool isPresent()`, and a configurable glitch-filter value. (5) There is no SSI/PWM/index/magnet-status hardware — implement A/B only, no absolute-mode code paths. Add a runtime check logging an error if the configured PCNT unit collides with the FAS unit."

### WP3 — Calibration routine (sign, scale, backlash, microstep tracking)
The load-bearing WP. Measures the four unknowns and derives detection thresholds from real noise.

> **Claude Code prompt (WP3):**
> "Create `main/src/axis/AxisCalibration.{h,cpp}` implementing a calibration routine that determines the relationship between commanded steps and encoder counts. Steps: (1) ORIGIN: zero the encoder and record the current step position as common origin. (2) SIGN: command a short low-speed move in + then − direction, require `abs(deltaCounts) > minCountsForValidity`, set `countSign = sgn(deltaCounts)`, fail with CAL_FAILED if counts don't change or the two directions disagree. (3) SCALE: command several move lengths (configurable, default 500/1000/2000/4000 steps) in both directions and linear-regress deltaCounts vs deltaSteps; slope = countsPerStep (store Q16.16), R² = quality gate (reject below threshold), residual scatter = `residualScatter`. (4) BACKLASH: approach a point from + , reverse, measure lost motion before counts resume; repeat and average into `backlashCounts`. (5) PERSIST to NVS together with the current microstep setting (read from the TMC controller in `main/src/tmc/TMCController.cpp`). Add a check at startup: if current microsteps != microstepsAtCal, rescale countsPerStep analytically, mark calibration as derived/unverified, and raise CAL_INVALID as a WARNING that does not block motion. Expose `residualScatter` so the watchdog and verify tolerances are derived from measured noise instead of hardcoded constants. Constrain the routine to bounded travel at low speed and abort on endstop or watchdog trip. All external values in steps."

### WP4 — Fast blocking/stall watchdog
Independent of the PID; protects every mode.

> **Claude Code prompt (WP4):**
> "Create `main/src/axis/AxisWatchdog.{h,cpp}`: a fast, cheap watchdog detecting a blocked or slipping axis, independent of any PID. Sample the encoder every 5-10 ms whenever FastAccelStepper reports motion AND commanded velocity is above a minimum floor. Two trips: (a) STALL — `abs(deltaCounts) < noiseThreshold` (derived from `AxisCalibration.residualScatter`) sustained for `stallTimeoutMs` (default 80 ms) while motion is expected; (b) LOST_STEPS — `abs(positionErrorSteps) > lagLimit` while moving. On either trip: immediately call `FocusMotor::stopStepper()`, set `AxisFeedback.fault` and `health=FAULT`, and invoke the notification layer. Include a grace window after motion start and a minimum-velocity gate so acceleration ramps below encoder resolution do not false-trip. Make the trip LATCHING: refuse further motion on that axis until `resetAxis()` is called. Run it from the motor loop or a small high-priority task; it must not be blocked by CAN servicing. It must work in OPEN_LOOP/MONITOR modes too, not only when a PID is active."

### WP5 — Async notification (EMCY + JSON)
> **Claude Code prompt (WP5):**
> "Create `main/src/axis/AxisNotify.{h,cpp}` providing asynchronous fault notification. On a CANopen slave, emit a CANopen EMCY object (COB-ID 0x80 + node-id) with a UC2-specific error code for each `AxisFault` (STALL, LOST_STEPS, DIVERGENCE, TIMEOUT, CAL_INVALID). Also map `positionErrorSteps` into a TPDO for continuous monitoring. In `main/src/canopen/CANopenModule.cpp`, handle inbound EMCY on the master and convert it into a JSON async event pushed on the existing async serial channel, in the same shape as existing motor events: `{\"axisEvent\":{\"axis\":n,\"fault\":\"STALL\",\"posErrSteps\":-142,\"commandedSteps\":...,\"measuredSteps\":...}}`. On standalone/serial builds emit the same JSON directly. All values in steps. Notifications must be fire-and-forget from the fault site (no blocking on CAN TX) — queue them and drain in the loop."

### WP6 — MONITOR and CORRECT modes
> **Claude Code prompt (WP6):**
> "In `main/src/axis/AxisController.cpp`, implement MONITOR and CORRECT modes. MONITOR: run the move fully open-loop, continuously update `positionErrorSteps`, and emit an async notification when `abs(error) > divergenceThresholdSteps` (default derived from `residualScatter`) or when the watchdog trips. MONITOR must NEVER command motion of its own. CORRECT: after the open-loop move completes, if `abs(positionErrorSteps) > toleranceSteps`, issue a bounded corrective relative move (clamped to `maxCorrectionSteps`), retry up to `maxCorrectionRetries` (default 2); then adopt the encoder-derived position as truth — CRITICAL: convert encoder position to steps and call the FastAccelStepper `setCurrentPosition()` so the next ABSOLUTE move computes from the corrected origin, not just update a struct field. Backlash handling: on a direction reversal apply `AxisCalibration.backlashCounts` as feed-forward AND widen the verify tolerance for that move, so normal mechanical slack does not trigger spurious corrections. If still out of tolerance after retries, set `health=DEGRADED`, `fault=DIVERGENCE` and notify. MONITOR is the default mode whenever an encoder is present and calibrated. Everything in steps; no blocking busy-wait loops."

### WP7 — SERVO mode (non-blocking)
> **Claude Code prompt (WP7):**
> "Implement SERVO mode in `main/src/axis/AxisController.cpp` as a non-blocking velocity servo, reusing the two-stage PID logic from the legacy `main/src/encoder/SimplePIDController.h` (port it into `axis/`, do not include the old header). Critical difference from the legacy `executePrecisionMotionBlocking`: do NOT run a blocking `while` loop that re-calls `FocusMotor::startStepper()` every millisecond taking the motor mutex. Instead run the PID tick from a dedicated FreeRTOS task pinned to core 0 (or a scheduled periodic callback) and apply the velocity by calling the FastAccelStepper set-speed API directly on an already-running continuous-mode stepper. The CAN bus must stay serviced throughout. Targets and tolerances are specified in steps and converted internally to counts via `AxisCalibration`. Integrate the WP4 watchdog so a stall stops the servo and latches a fault. Implement `resetAxis(policy)` with policies TRUST_ENCODER (adjust commanded steps so error is zero), TRUST_STEPS (zero the encoder to match), FORCE_REHOME — clearing PID state, stall state and fault. Default policy TRUST_ENCODER when calibrated, else FORCE_REHOME."

### WP8 — CANopen OD exposure
> **Claude Code prompt (WP8):**
> "In `main/src/canopen/UC2_OD_Indices.h`, add an AXIS/ENCODER object-dictionary block at base 0x2020: `AXIS_MEASURED_STEPS 0x2020` (ro), `AXIS_POSITION_ERROR_STEPS 0x2021` (ro), `AXIS_MODE 0x2022` (rw, OPEN_LOOP/MONITOR/CORRECT/SERVO), `AXIS_HEALTH 0x2023` (ro), `AXIS_FAULT 0x2024` (ro), `AXIS_RESET 0x2025` (wo, takes reset policy), `AXIS_CALIBRATED 0x2026` (ro), `AXIS_REFERENCED 0x2027` (ro), `AXIS_CALIBRATE 0x2028` (wo, trigger calibration), `AXIS_COUNTS_PER_STEP_Q16 0x2029` (rw), `AXIS_BACKLASH_STEPS 0x202A` (rw), `AXIS_RAW_COUNTS 0x202B` (ro, diagnostics). Wire read/write handlers in `main/src/canopen/CANopenModule.cpp` alongside the existing MOTOR_* handlers, delegating to `AxisController`. All position/error values in STEPS except the explicitly-named raw-counts diagnostic entry. Ensure `MOTOR_ACTUAL_POSITION` (0x2001) keeps its current step semantics for backward compatibility."

### WP9 — uc2-rest / uc2-canopen Python interface
The firmware side is useless without host exposure. UC2-REST already has the right idioms: `self._parent.serial.register_callback(fn, pattern=...)` and the `register_stagescan_callback` / `unregister_*` pattern in `uc2rest/motor.py`.

> **Claude Code prompt (WP9):**
> "In the UC2-REST Python package (`uc2rest/`), add closed-loop / error-tracking support mirroring the existing idioms in `uc2rest/motor.py`. (1) Add methods on the motor class: `setAxisMode(axis, mode)`, `getAxisFeedback(axis)` returning a dict with `commandedSteps, measuredSteps, positionErrorSteps, mode, health, fault, calibrated, referenced`, `resetAxis(axis, policy)`, `calibrateAxis(axis, **params)` returning the calibration result, and `getAxisCalibration(axis)`. All values in STEPS — do NOT convert to µm in this layer; µm conversion stays in the higher-level ImSwitch code. (2) Register an async listener for the new firmware event using the existing mechanism `self._parent.serial.register_callback(self._callback_axis_event, pattern='axisEvent')`, and expose `register_axis_event_callback(callback)` / `unregister_axis_event_callback(callback)` following the exact pattern of the existing `register_stagescan_callback`. The callback receives the fault dict so ImSwitch can react to STALL / LOST_STEPS / DIVERGENCE during long acquisitions. (3) Keep every new method safe when the firmware lacks encoder support: return `mode='OPEN_LOOP'`, zero error, and never raise. The existing `move`/`getPosition` API must remain byte-for-byte unchanged so current ImSwitch code paths are unaffected."

### WP10 — Retire legacy encoder code
> **Claude Code prompt (WP10):**
> "Once `main/src/axis/` is validated on hardware, retire the legacy closed-loop path. Put `main/src/encoder/LinearEncoderController.{h,cpp}`, `PCNTEncoderController.{h,cpp}` and `SimplePIDController.h` behind a `USE_LEGACY_ENCODER` build flag (default OFF). Remove the `encoderBasedMotion` branch and the synthesized `/linearencoder_act` JSON from `FocusMotor::toggleStepper` / `startEncoderBasedMotion` in `main/src/motor/FocusMotor.cpp`, replacing it with a call into `AxisController`. Delete the duplicated conversion constants (`mumPerStep = 12.444f`, `stepsToEncoderUnits = 0.3125f`) — `AxisCalibration` is now the only source. Update `platformio.ini` so `UC2_canopen_slave_motor` builds the new axis module. Verify the firmware still builds for encoderless environments with the axis module compiled in."

---

## 10. Sequencing

| Phase | WPs | Outcome |
|---|---|---|
| **A — Foundation** | WP1, WP2, WP3 | Encoder read cleanly, calibration measures sign/scale/backlash/noise. Thresholds now empirical. |
| **B — Safety & visibility** | WP4, WP5, WP8, WP9 | Blocked axis stops fast; ImSwitch gets async stall/lost-step events. **This is the point at which long runs become trustworthy.** |
| **C — Correction** | WP6 | Lost steps detected *and* corrected; encoder becomes position authority per move. |
| **D — Servo** | WP7 | Full feedback control, only where live tracking is needed. |
| **E — Cleanup** | WP10 | Legacy path retired. |

Phase B is the real deliverable for your tile-scan problem: you may not need `MODE_CORRECT` at all if the notifications show the losses are rare and the stitcher absorbs them — and if they're frequent, the same data tells you it's mechanical rather than firmware.

## 11. Open questions for hardware
- Actual counts-per-2 mm from the AS5311 ABI configuration in use → **measured by WP3**, no longer a guess. Worth logging the first calibration result to confirm it lands near the ~1 µm/count expectation.
- Current microstep setting on the CAN slave nodes (TMC config) → the `microstepsAtCal` baseline.
- Whether any deployed slave node is physically a non-X axis → determines urgency of the WP2 axis-index fix.
- Typical reversal slack in steps (your ~30) → cross-check against the WP3 measured `backlashCounts`.
