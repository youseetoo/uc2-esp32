# Fix: extra GPIO step pulses on UC2_4 (FastAccelStepper + TCA9535)

You are fixing a real, reproducible bug in https://github.com/youseetoo/uc2-esp32 on the **native UC2_4** target (the standalone mainboard, no CAN). This file gives you the symptom, the root-causes I found by reading the code and the FastAccelStepper internals, and the changes you should make. **Read the whole file before editing anything.** When in doubt, prefer the smallest change that addresses one root cause, build, ask the user to re-test, then move on.

---

## 1. Symptom (what the user is observing)

Logic-analyzer counts on the `STEPX` GPIO line of UC2_4:

- Absolute move of **200** steps → **205** pulses on the wire (+5).
- Absolute move of **500** → **502** (+2).
- Absolute move of **1000 / 10000** → **+1** pulse.
- Single moves in the same direction always work.
- Extras appear on **every direction reversal in absolute mode**, including:
  - the very first move after boot,
  - the first column of every row in a snake-pattern grid scan.
- Relative moves (`isabs:0`) appear correct.
- The Saleae trace of a 200-step burst shows `fmin ≈ 1.4 kHz`, `fmax ≈ 14.3 kHz`, `fmean ≈ 6.9 kHz` — i.e. there is a clearly visible acceleration ramp **even though the user sent `isaccel:0`**.

Affected target: `UC2_4` (native, no `LINEAR_ENCODER_CONTROLLER`, no `USE_PCNT_COUNTER`, uses `USE_TCA9535`). The encoder is **not** wired and is not the cause; ignore the user's encoder hypothesis.

---

## 2. Root causes (ordered by likely contribution to the symptom)

You will find each one with `grep`/file reads in the paths given. Verify each before changing it.

### 2.1 The TCA9535 wrapper lies about the direction-pin state — defeats FAS's safety net

`main/src/i2c/tca_controller.cpp`:

```cpp
bool setExternalPin(uint8_t pin, uint8_t value) {
#ifdef USE_TCA9535
    pin -= 128;
    TCA.write1(pin, value);
    log_i("Setting TCA9535 pin %i to %i", pin, value);
#endif
    return value;   // <-- always echoes the request
}
```

The library contract for the `_externalCallForPin` callback (see `FastAccelStepper.cpp::handleExternalDirectionPin` and `extras/doc/dir_pin_refactoring.md`) is: **return the actual current state of the pin, not the requested one.** When the pin hasn't yet reached the requested state (because the I²C write to the TCA is slow), the callback must return the *old* state. The library will then enqueue a 2 ms pause command and retry on the next fill cycle — that is the whole point of the external-pin path. The FAS docstring on `setDirectionPin` confirms:

> "For external pins, dir_change_delay_us is ignored, because the mechanism applied for external pins provides already pause in the range of ms or more."

By unconditionally returning `value`, the UC2 wrapper makes FAS believe every TCA toggle landed instantly, so the 2 ms pause is **never inserted**. Step pulses get enqueued immediately after the dir-pin write was *kicked off*, while the TCA's output is still mid-transition (a single TCA register write at 100 kHz I²C is ~250–500 µs, longer with bus contention and the `log_i` UART call inside this same function).

This is the dominant cause for the per-direction-change extras.

### 2.2 `isaccel:0` is silently ignored, and `accel:0` corrupts the acceleration

`main/src/motor/MotorJsonParser.cpp` around line 684:

```cpp
FocusMotor::getData()[s]->acceleration = cJsonTool::getJsonInt(stp, key_acceleration);
FocusMotor::getData()[s]->isaccelerated = cJsonTool::getJsonInt(stp, key_isaccel);
```

`cJsonTool::getJsonInt` returns `0` when the key is absent (see `main/cJsonTool.h`). And `main/src/motor/FAccelStep.cpp::startFastAccelStepper` lines 38–45:

```cpp
if (getData()[i]->acceleration >= 0)               // 0 passes!
    faststeppers[i]->setAcceleration(getData()[i]->acceleration);
else
    faststeppers[i]->setAcceleration(MAX_ACCELERATION_A);
```

Two problems:

1. `isaccelerated` is never read — there is no constant-velocity code path. The motor accelerates regardless.
2. When the JSON omits `accel`, `acceleration` becomes `0`. FAS's `setAcceleration` returns `-1` for any value `<= 0` and silently keeps the previous acceleration (per the FAS API docstring). The "previous acceleration" is whatever the ramp generator was last told — typically `DEFAULT_ACCELERATION = 100000` from `setupFastAccelStepper`, but it is in practice unpredictable.

This explains why the Saleae trace shows a ramp from 1.4 kHz to 14.3 kHz on a `isaccel:0` move.

### 2.3 The board falls back to MCPWM/PCNT, despite the misleading log line

`main/src/motor/FAccelStep.cpp::setupFastAccelStepper(Stepper, ...)` lines 214–225:

```cpp
#ifdef LINEAR_ENCODER_CONTROLLER
faststeppers[stepper] = engine.stepperConnectToPin(motorstp, DRIVER_RMT);
#endif
if (faststeppers[stepper] == nullptr)
{
    log_e("Failed to create RMT stepper for motor %d, falling back to PCNT", stepper);
    faststeppers[stepper] = engine.stepperConnectToPin(motorstp);
}
else
{
    log_i("Successfully created RMT stepper for motor %d (avoiding PCNT conflicts)", stepper);
}
```

UC2_4 does **not** define `LINEAR_ENCODER_CONTROLLER`. The `#ifdef` block is skipped entirely, the pointer stays `nullptr`, the "fallback" branch fires, and the stepper is allocated on **MCPWM/PCNT**. The `log_e` message about "Failed to create RMT stepper" is also misleading because no attempt was even made.

Why this matters: the MCPWM/PCNT driver does **not** insert FAS's `MIN_CMD_TICKS` direction-change pause that the RMT driver does (see the FAS README "What are the differences between mcpwm/pcnt, rmt, and i2s mux?" section). Switching to RMT here is a second layer of safety against fast direction toggles, on top of the wrapper fix from §2.1.

### 2.4 Boot-time `move(2); move(-2);` leaves residue in the queue

`main/src/motor/FAccelStep.cpp::setupFastAccelStepper(Stepper, ...)` lines 237–238:

```cpp
faststeppers[stepper]->setCurrentPosition(getData()[stepper]->currentPosition);
faststeppers[stepper]->move(2);
faststeppers[stepper]->move(-2);
```

These two calls are non-blocking. They get queued asynchronously, often coalescing with the very first user move, and produce two unaccounted direction reversals during boot. Most likely source of the persistent **+1** observed on long moves where the per-direction-change race contributes a small fixed offset.

### 2.5 `stopFastAccelStepper` does `forceStop(); stopMove();`

`main/src/motor/FAccelStep.cpp::stopFastAccelStepper`:

```cpp
faststeppers[i]->forceStop();
faststeppers[i]->stopMove();
```

Per the FAS source: `forceStop()` sets `q->ignore_commands = true` and stops the ramp generator, but **does not empty the queue** — already-queued commands keep firing for up to ~20 ms. The follow-up `stopMove()` is then a no-op (ramp generator is already stopped). Then the code reads `getCurrentPosition()` (which returns `q->queue_end.pos`, the post-queue position) into `data[i]->currentPosition`.

This is sloppy but is **not** the cause of the leading-edge extras the user is measuring. Still worth fixing for correctness; demote to last priority.

### 2.6 What is *not* the cause (do not chase these)

- **The encoder.** `LINEAR_ENCODER_CONTROLLER` and `USE_PCNT_COUNTER` are both undefined for `UC2_4`; the `ENC_X_*` pins are `disabled`. There is no `ESP32Encoder` instance constructed. No PCNT collision exists.
- **Soft / hard limits.** Both code paths run only when `softLimitEnabled` / `hardLimitEnabled` are set, and neither stops the motor mid-move in the user's test scenario.
- **Homing.** Single-motor `motor_act` calls don't trigger homing.

---

## 3. The fixes, in the order you should apply them

Make each change as a separate commit so the user can bisect which fix removed which symptom. After **each** fix, ask the user to re-run their absolute-move test (`isabs:1`, `isaccel:0`, position 200, then 400, then 0, observed on the Saleae) and report the new pulse counts.

### Fix A (apply first, biggest expected impact): TCA wrapper must report actual pin state

Edit `main/src/i2c/tca_controller.cpp`. Track per-pin "requested" vs "acked" state and a microsecond timestamp of when the request was made. Only return the new value once enough time has elapsed for the I²C write to have actually completed. Until then, return the *previous* state so FAS inserts its 2 ms pause.

Sketch (adapt to existing namespace style; add the necessary `<cstring>` / `Arduino.h` for `micros`/`memset`):

```cpp
#ifdef USE_TCA9535
namespace {
    // Index 0..15 maps to TCA pin number (already-stripped of the 128 flag).
    // 0xFF = unknown.
    uint8_t  s_requested[16];
    uint8_t  s_acked[16];
    uint32_t s_request_us[16];
    bool     s_inited = false;

    // Generous: a single-byte TCA write at 100 kHz I²C is ~250-500 us.
    // Bump to ~1000 us if you also enable the verbose log_i below.
    constexpr uint32_t TCA_SETTLE_US = 800;

    inline void ensure_init() {
        if (s_inited) return;
        memset(s_requested, 0xFF, sizeof(s_requested));
        memset(s_acked,     0xFF, sizeof(s_acked));
        s_inited = true;
    }
}
#endif

bool setExternalPin(uint8_t pin, uint8_t value) {
#ifdef USE_TCA9535
    ensure_init();
    const uint8_t p = pin - 128;          // matches the existing convention
    if (p >= 16) return value;            // out-of-range: degrade gracefully

    if (s_requested[p] != value) {
        // New request: kick off the I2C write, but do NOT yet claim success.
        TCA.write1(p, value);
        s_requested[p]  = value;
        s_request_us[p] = micros();
        // Return the LAST acknowledged state so FAS sees "not yet" and
        // inserts its 2ms pause; on the next fill cycle we'll be polled again.
        return s_acked[p];
    }

    // Same value re-requested (FAS is polling us): only ack once the write
    // has had time to land on the TCA.
    if ((uint32_t)(micros() - s_request_us[p]) >= TCA_SETTLE_US) {
        s_acked[p] = value;
    }
    return s_acked[p];
#else
    return value;
#endif
}
```

Also: **delete or guard** the `log_i("Setting TCA9535 pin %i to %i", pin, value);` line. With log level `info` enabled, that's a UART transaction (often >1 ms) executing inside FAS's engine task on every direction toggle, which by itself can stretch the race window.

After this fix, re-run the test. If the +5 disappears (and you see a brief 2 ms quiet period on STEPX after every direction reversal), this was the dominant cause.

### Fix B: honor `isaccel` and reject `accel <= 0`

Edit `main/src/motor/FAccelStep.cpp::startFastAccelStepper`. Replace the existing acceleration block with something like:

```cpp
const bool wantAccel   = getData()[i]->isaccelerated != 0;
const int32_t reqAccel = getData()[i]->acceleration;

if (wantAccel && reqAccel > 0) {
    if (faststeppers[i]->setAcceleration(reqAccel) != 0) {
        log_w("setAcceleration(%d) rejected on motor %d, using MAX_ACCELERATION_A",
              reqAccel, i);
        faststeppers[i]->setAcceleration(MAX_ACCELERATION_A);
    }
} else {
    // Either the user asked for no acceleration (constant velocity) or
    // gave us a non-positive value. FAS has no true "constant velocity"
    // mode for moveTo/move, so the safest fallback is the maximum allowed
    // acceleration so the ramp is as short as possible.
    faststeppers[i]->setAcceleration(MAX_ACCELERATION_A);
}
```

Note the change of test from `>= 0` to `> 0`, the explicit check of the return code, and the added `isaccelerated` gate. Do not silently keep stale acceleration values.

If you want a true "no ramp" mode (for very small distances at low speed where the user really wants instant top-speed), you would need to call `setSpeedInHz(speed); applySpeedAcceleration();` and then `runForward()/runBackward()` for a known number of ticks — that's a much bigger refactor and not required to fix this bug. Leave it.

### Fix C: prefer the RMT driver on UC2_4

Edit `main/src/motor/FAccelStep.cpp::setupFastAccelStepper(Stepper, ...)`:

```cpp
// Always try RMT first. UC2_4 has no encoder competing for PCNT, and
// RMT inserts a MIN_CMD_TICKS pause before direction toggles which the
// MCPWM/PCNT driver does not.
faststeppers[stepper] = engine.stepperConnectToPin(motorstp, DRIVER_RMT);
if (faststeppers[stepper] == nullptr) {
    // Last-resort fallback to MCPWM/PCNT.
    faststeppers[stepper] = engine.stepperConnectToPin(motorstp);
    log_w("Motor %d: RMT unavailable, falling back to MCPWM/PCNT", stepper);
} else {
    log_i("Motor %d: using RMT driver", stepper);
}
```

This removes the `#ifdef LINEAR_ENCODER_CONTROLLER` guard around the RMT call and corrects the misleading log messages. Verify with `grep -rn "DRIVER_RMT\|stepperConnectToPin" main/src/motor/` that you've caught every call site.

### Fix D: drop the boot-time priming moves (or make them blocking)

Edit `main/src/motor/FAccelStep.cpp::setupFastAccelStepper(Stepper, ...)`. Either:

```cpp
// Removed: faststeppers[stepper]->move(2);
// Removed: faststeppers[stepper]->move(-2);
```

or, if there is some hardware reason they were added that I don't see (ask the user before deleting, e.g. some stepper drivers want a wakeup pulse), make them blocking so the queue is drained before the engine accepts user moves:

```cpp
faststeppers[stepper]->move(2,  /*blocking=*/true);
faststeppers[stepper]->move(-2, /*blocking=*/true);
```

Default to **removing** them; the FAS engine handles cold starts fine on its own.

### Fix E: clean up `stopFastAccelStepper`

Edit `main/src/motor/FAccelStep.cpp::stopFastAccelStepper`. Replace:

```cpp
faststeppers[i]->forceStop();
faststeppers[i]->stopMove();
```

with one of:

- For a **graceful** stop (preferred for the loop()'s end-of-move path):
  ```cpp
  faststeppers[i]->stopMove();
  ```
- For a **hard** stop with a consistent position (preferred when the caller actually wants an abort):
  ```cpp
  faststeppers[i]->forceStopAndNewPosition(faststeppers[i]->getCurrentPosition());
  ```

Don't combine `forceStop()` and `stopMove()`. Pick one. This is correctness cleanup and is unlikely to change the user's measured pulse counts.

---

## 4. How to verify each fix

After **each** fix above, ask the user to do this exact test and report results:

1. Power-cycle the ESP32.
2. Send via serial:
   ```json
   {"task":"/motor_act","motor":{"steppers":[{"stepperid":1,"position":200,"speed":15000,"isabs":1,"isaccel":0}]}}
   {"task":"/motor_act","motor":{"steppers":[{"stepperid":1,"position":0,  "speed":15000,"isabs":1,"isaccel":0}]}}
   {"task":"/motor_act","motor":{"steppers":[{"stepperid":1,"position":200,"speed":15000,"isabs":1,"isaccel":0}]}}
   ```
3. On Saleae, count `Nrising` on STEPX for each of the three moves. The expected values after all fixes are applied are **200, 200, 200**.

You can also ask the user to add a 4th channel to the Saleae on the I²C `SCL` line. After Fix A, every direction-reversed move should show:

- TCA-write SCL bursts complete,
- a quiet ≥2 ms period on STEPX,
- then exactly N step pulses.

Before Fix A, the SCL bursts and the first STEPX pulses overlap.

---

## 5. Things to NOT do

- Do not touch the encoder code paths. The user's hypothesis that the encoder PCNT is interfering is wrong on UC2_4.
- Do not change the I²C bus speed from 100 kHz to 400 kHz unilaterally. Other I²C devices on this bus (look at `I2C_ADD_LEX_*` in `main/config/UC2_4/PinConfig.h`) may not tolerate it. If you want to suggest 400 kHz as an extra mitigation, propose it as a comment in the PR, do not commit it.
- Do not rewrite `MotorJsonParser.cpp` to make absent JSON fields preserve the previous value instead of defaulting to 0. That sounds tempting but it's a behavior change that affects every other caller (CAN, gamepad, etc.) and will create new bugs. Fix the consumer (Fix B), not the parser.
- Do not change `setDelayToDisable(500)` — that's milliseconds and is fine.
- Do not change the FastAccelStepper library itself. All fixes belong in the UC2 application code.

---

## 6. Reference: the FastAccelStepper internals you may want to read

If you need to verify any claim above, the relevant files in https://github.com/gin66/FastAccelStepper are:

- `src/FastAccelStepper.cpp` lines 33–63 (`handleExternalDirectionPin`) — the contract.
- `src/FastAccelStepper.cpp` lines 67–200 (`addQueueEntry`) — where the 2 ms pause is inserted.
- `src/FastAccelStepper.h` lines 100–116 — `setDirectionPin` docstring.
- `extras/doc/dir_pin_refactoring.md` — full design rationale for the external-pin handshake.
- `src/fas_arch/result_codes.h` — the `AQE_DIR_PIN_2MS_PAUSE_ADDED` code and `aqeRetry()` semantics.
- README "What are the differences between mcpwm/pcnt, rmt, and i2s mux?" — RMT's automatic dir-change pause.

The relevant UC2 files are:

- `main/src/motor/FAccelStep.cpp` — fixes B, C, D, E.
- `main/src/i2c/tca_controller.cpp` — fix A.
- `main/src/motor/MotorJsonParser.cpp` — read-only context for fix B.
- `main/cJsonTool.h` — read-only context for fix B.
- `main/config/UC2_4/PinConfig.h` — confirm `USE_TCA9535`, no encoder, the dir-pin assignments.