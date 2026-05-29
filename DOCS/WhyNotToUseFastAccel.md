I'll do a deep investigation into the FastAccelStepper stop conditions, especially for speeds above 20000 steps/s. Let me explore the codebase first.

Now let me check the FastAccelStepper library limits and the FocusMotor code to understand stop detection.

Now let me look at the RMT driver specifically since the firmware now uses RMT, and check how isRunning behaves at high speeds.

Now let me check the FocusMotor stop logic - especially how the motor knows when to stop:

Now let me look at the homing logic:

Now let me find where the homing speed cap and the actual "stop fail at 25000 steps/s on a finite move" happens. Let me look at FocusMotor's startStepper and stopStepper:

Now let me check the StepperISR/RMT decoupling between rampState becoming IDLE and physical step pulses still being emitted:

Now I need to understand WHEN exactly the ramp goes to IDLE. Let me check:

Now let me find where rampState is actually written into the queue commands and check the time of transition vs. queue emptying:

Now let me look at the CANOpen slave motor specifically:

Now let me look at CANopenModule for how the slave handles motor commands and reports done:

Now let me look at how the slave reports motor completion to master via TPDO:

Let me check the user's existing motor settings and acceleration:

Now let me check the master side TPDO consumption for completion detection:

Now let me check if there's a hardcoded speed cap in startStepper or other limiters, and look at the actual move command path for finite moves with default acceleration handling:

Let me check the moveMotor path and the start logic at very high speed to identify any speed cap interactions:

Let me also confirm by checking that the `_forward_planning_in_ticks` is unchanged from FAS default:

I have enough findings now. Let me consolidate the report.

## Deep Investigation Report — Motor not stopping at >20000 steps/s

Below is what I found tracing the actual stop/finish path from FAS internals all the way up to the CAN slave. I have not changed any code.

### 1. The 20000 boundary is not arbitrary — there's a hidden 20 ms "forward-planning" window in FAS

**FAS internals (constants):** [FastAccelStepper.cpp:524](.pio/libdeps/UC2_canopen_slave_galvo_debug/FastAccelStepper/src/FastAccelStepper.cpp:524)
```
_forward_planning_in_ticks = TICKS_PER_S / 50;   //  = 320 000 ticks @16 MHz = 20 ms
```
`fill_queue()` keeps the hardware queue filled **20 ms into the future** ([FastAccelStepper.cpp:371-454](.pio/libdeps/UC2_canopen_slave_galvo_debug/FastAccelStepper/src/FastAccelStepper.cpp:371)). This is intentional FAS behavior, not a bug.

The 20 ms window translates into very different *step counts* depending on speed:

| Step rate     | Steps queued ahead of real motor |
|---------------|----------------------------------|
| 5 000 steps/s | 100 steps                        |
| 10 000        | 200                              |
| 18 000        | 360                              |
| **25 000**    | **500**                          |

Above ~20 kHz the "queue carry-over" becomes large enough (hundreds of steps, mm-scale on a leadscrew) to be visible as "the motor doesn't stop".

The constant `MAX_VELOCITY_A = 20000` at [MotorTypes.h:4](main/src/motor/MotorTypes.h:4) is the matching software design limit. The homing FIXME at [HomeMotor.cpp:128-132](main/src/home/HomeMotor.cpp:128) caps homing speed at 18 000 for the same reason.

### 2. Root cause: `FAccelStep::isRunning()` is too optimistic for ESP32 RMT

**[FAccelStep.cpp:500-525](main/src/motor/FAccelStep.cpp:500)**
```cpp
bool isRunning(int i) {
    ...
    const uint8_t rs = s->rampState() & RAMP_STATE_MASK;
    if (rs != RAMP_STATE_IDLE) return true;
    // Ramp is idle. Treat as truly-done. Don't trust FAS's isRunning()...
    return false;
}
```

`rampState()` flips to `RAMP_STATE_IDLE` the instant the **ramp generator enqueues the last command** ([RampConstAcceleration.cpp:133-141 + 228-237](.pio/libdeps/UC2_canopen_slave_galvo_debug/FastAccelStepper/src/RampConstAcceleration.cpp:133)). On RMT this can be **up to 20 ms before the physical motor reaches the target** — the queue/RMT-DMA still has to drain (see also `StepperISR.h:45-50` which explicitly warns about this for ESP32).

The comment in the wrapper claims "Definition of running: ramp non-IDLE **OR there are still unconsumed queue entries**" — but the code only checks `rampState`. The "OR unconsumed queue" half was dropped.

Stock FAS gets this right: [FastAccelStepper.cpp:897-900](.pio/libdeps/UC2_canopen_slave_galvo_debug/FastAccelStepper/src/FastAccelStepper.cpp:897)
```cpp
bool FastAccelStepper::isRunning() {
  StepperQueue* q = &fas_queue[_queue_num];
  return q->isRunning() || _rg.isRampGeneratorActive() || !isQueueEmpty();
}
```

At low speeds the difference is invisible (a few residual steps). At ≥20 kHz the wrapper "lies" for 20 ms while the motor still emits hundreds of steps.

### 3. Why this manifests as "motor won't stop" — the merge race

When the wrapper returns false too early, **two consumers** race:

A. **`FocusMotor::loop`** ([FocusMotor.cpp:711](main/src/motor/FocusMotor.cpp:711))
   ```cpp
   if (isActivated[i] && !isRunning(i) && !data[i]->stopped && !data[i]->isforever && !data[i]->isHoming)
       stopStepper(i);  // -> forceStop_rmt() -> rmt_disable() -> motor stops HARD
   ```

B. **`CANopenModule::loop` dispatch** ([CANopenModule.cpp:1884-1939](main/src/canopen/CANopenModule.cpp:1884)) which fires whenever a new RPDO arrives:
   ```cpp
   FocusMotor::startStepper(localAxis, 0);   // -> FAccelStep::startFastAccelStepper
   ```

In `startFastAccelStepper` the `needReset` block ([FAccelStep.cpp:90-122](main/src/motor/FAccelStep.cpp:90)) checks `rs0 == RAMP_STATE_IDLE` → since the ramp is already IDLE it concludes **"no reset needed"** and goes straight to `faststeppers[i]->move(N)`. That call **appends** N more steps to the leftover queue. The motor never stops between the two moves.

Timing window per move at 25 kHz:

```
ramp IDLE  → TPDO sent → master "done" → master sends next move →
SDO arrives → RPDO dispatch → startStepper()
            ▲                                                ▲
            └──────── ~5-15 ms ────────────────────────────────┘
queue still draining for 20 ms after ramp IDLE
```

If startStepper wins the race against `FocusMotor::loop`'s stopStepper, the queues are concatenated and the user sees a motor that **never came to rest** between moves.

This is also exactly why the symptom is more pronounced on the CAN slave — the master is faster at submitting follow-up moves (sub-ms round-trip on CAN bus) than the human is at sending another `move` from a UI.

### 4. Why the homing fault has the same root cause

- Phase 2 polls every 10 ms ([HomeMotor.cpp:458](main/src/home/HomeMotor.cpp:458)). At 25 kHz that's 250 steps per poll — the motor overshoots the endstop by hundreds of steps before the task notices.
- Phase 4 / 10 / 12 wait on `!isRunning(axis)` ([HomeMotor.cpp:279](main/src/home/HomeMotor.cpp:279), [441](main/src/home/HomeMotor.cpp:441), [371](main/src/home/HomeMotor.cpp:371)) — the same false-positive 20 ms early.
- Phase 3 (direction-reversal retract) starts a new move while the previous queue is still draining. With the external-pin handshake (TCA9535) the `toggle_dir` entry can be processed late, so the first ~hundreds of steps of the retract emit in the **wrong direction**. That matches the existing FIXME at [HomeMotor.cpp:292](main/src/home/HomeMotor.cpp:292).

### 5. TPDO running-flag is also poisoned at high speed

[CANopenModule.cpp:1766-1797](main/src/canopen/CANopenModule.cpp:1766) computes the slave's `statusWord` bit-0 from `FocusMotor::isRunning(localAxis)` — i.e. the same wrapper. So the master is told the slave is done ~20 ms early, which is what makes it eager to fire the next move into the open race window.

### 6. Position is also lied to on premature stop

When `FocusMotor::loop` calls `stopStepper` early, `FAccelStep::stopFastAccelStepper` ([FAccelStep.cpp:414-422](main/src/motor/FAccelStep.cpp:414)) sees `rs == IDLE` and pins the position to `getPositionAfterCommandsCompleted()` — i.e. the queue-end position, which is **20 ms ahead of where the rotor actually stopped** once `rmt_disable()` slams the channel off. Subsequent relative moves then compound the error.

### 7. What is NOT the cause (I ruled these out)

- `MIN_CMD_TICKS = 3200` (200 µs) is not violated at 25 kHz — `ticks*steps` still ≥ MIN_CMD_TICKS via multi-step packing ([StepperISR.cpp:42-48](.pio/libdeps/UC2_canopen_slave_galvo_debug/FastAccelStepper/src/StepperISR.cpp:42)).
- RMT memory underrun: `mem_block_symbols = 2*PART_SIZE` (~46 symbols) and the encoder callback fires per chunk; at 25 kHz callback period is ~880 µs, well within `manageSteppers`' 1 ms tick.
- `rmt_disable()` itself does stop pulses synchronously — `forceStop_rmt` ([StepperISR_idf5_esp32_rmt.cpp:207-215](.pio/libdeps/UC2_canopen_slave_galvo_debug/FastAccelStepper/src/StepperISR_idf5_esp32_rmt.cpp:207)) is correct.

### Suggested fix directions (no code change yet — review first)

1. **Replace the wrapper's stop test with the actual completion condition** — either `FastAccelStepper::isRunning()` (stock) or `(rampState()==IDLE) && isQueueEmpty()`. That single change closes the 20 ms race window.
2. **Make `stopStepper` idempotent vs. an already-pending command in dispatch** — drop new commands that arrive while the queue is non-empty *and* mark the axis with a brief "settling" window (e.g. don't accept a new move within `_forward_planning_in_ticks` of the last command going into the queue).
3. **Plumb a smaller `_forward_planning_in_ticks`** (`setForwardPlanningTimeInMs`) when high-speed continuous moves are expected. Trading a few ms of planning for tighter stop accuracy. (Trade-off: more risk of underrun at very high accel.)
4. **Homing**: gate phase transitions on `!FAS::isQueueEmpty() && rampState==IDLE` instead of the wrapper, and bump `vTaskDelay(100)` to cover the planning window after `stopStepper` so the residual queue is verifiably drained before issuing the reverse retract.
5. Long-term: raise `MAX_VELOCITY_A`/`homeSpeed` cap only after items 1-4 are in.

Want me to draft a patch for item 1 (the smallest, most-targeted change)? I'd recommend trying that alone first — it should be enough to eliminate the "motor won't stop between moves" symptom, and we can measure before deciding whether 2-4 are also needed.