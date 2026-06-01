#include "FAccelStep.h"
#include <PinConfig.h>
#ifdef TMC_CONTROLLER
#include "../tmc/TMCController.h"
#endif

// convert pins to internal pins in setupFastAccelStepper
#ifdef USE_TCA9535
#undef PIN_EXTERNAL_FLAG
#define PIN_EXTERNAL_FLAG 128
#else
#undef PIN_EXTERNAL_FLAG
#define PIN_EXTERNAL_FLAG 0x00
#endif

using namespace FocusMotor;
#ifdef TMC_CONTROLLER
using namespace TMCController;
#endif

namespace FAccelStep
{
    void startFastAccelStepper(int i)
    {
        log_i("Starting FastAccelStepper for motor %i", i);
        // Guard: faststeppers is sized for LOCAL axes only (currently 4).
        // REMOTE axes (index >= array size) have no FastAccelStepper handle.
        if (i < 0 || i >= (int)faststeppers.size())
        {
            log_d("startFastAccelStepper: axis %d has no local stepper (REMOTE or OUT_OF_RANGE)", i);
            return;
        }
        if (faststeppers[i] == nullptr)
        {
            log_e("stepper %i is null", i);
            return;
        }
        int speed = abs(getData()[i]->speed);

        // clamp speed to 80000
        if (speed > 80000) {
            log_w("Clamping speed %d -> 80000 on motor %d", speed, i);
            speed = 80000;
        }
        if (faststeppers[i]->setSpeedInHz(speed) != 0)
            log_w("setSpeedInHz(%d) rejected on motor %d", speed, i);

        // CORRECTED DIAGNOSIS (2026-05): the "slow motion + overshoot" above a
        // speed/accel ratio is NOT a planner/pmfl rounding bug. A faithful host
        // simulation of FAS's real ramp math shows that, planned FROM REST, every
        // (speed, accel) pair below stops EXACTLY on target with zero overshoot.
        // The overshoot only appears when a NEW move is planned while the axis is
        // still moving fast: FAS overshoots by the deceleration distance v^2/(2a)
        // and then REVERSEs to recover (the "first fast, then crawls / overruns
        // the hard limit" symptom). Higher accel just shrinks v^2/(2a), which is
        // why ~12x "kinda" worked. The real fix is the REST GUARANTEE below; the
        // empirical test matrix is kept for reference only.
        /*
        

        works: {"task": "/motor_act", "motor": {"steppers": [{"stepperid": 1, "speed":70000, "position": 50000, "acceleration": 170000, "isabs": 0}]}, "qid": 5}
        does not work:  => slow motion / overshoots 
        works: {"task": "/motor_act", "motor": {"steppers": [{"stepperid": 1, "speed":50000, "position": -50000, "acceleration": 170000, "isabs": 0}]}, "qid": 5}
        does not work: {"task": "/motor_act", "motor": {"steppers": [{"stepperid": 1, "speed":50000, "position": 50000, "acceleration": 160000, "isabs": 0}]}, "qid": 5} => slow motion / overshoots 
        {"task": "/motor_act", "motor": {"steppers": [{"stepperid": 1, "speed":50000, "position": 50000, "acceleration": 20000, "isabs": 0}]}, "qid": 5} => does not work, some accelerates, slower motion than 50000 and overshoots
        {"task": "/motor_act", "motor": {"steppers": [{"stepperid": 1, "speed":40000, "position": 50000, "acceleration": 20000, "isabs": 0}]}, "qid": 5} => works 
        {"task": "/motor_act", "motor": {"steppers": [{"stepperid": 1, "speed":30000, "position": -50000, "acceleration": 20000, "isabs": 0}]}, "qid": 5} => works 
        {"task": "/motor_act", "motor": {"steppers": [{"stepperid": 1, "speed":20000, "position": -50000, "acceleration": 40000, "isabs": 0}]}, "qid": 5} => works 
        */
        int32_t reqAccel = getData()[i]->acceleration;
        /*
        
        TODO: FIXME whatsoever, it has been shown in the current configuration that only an acceleration that is 12x higher than the speed 
        can guarantee that the motor gets going and does not just start with a slow motion and then overshoots. 
        This is a very weird finding and we need to investigate this further, but for now we will just auto-bump 
        the acceleration to a safe value if the speed is above a certain threshold and the acceleration is below the safe minimum. 
        We should also log this auto-bump so we can track how often it happens and maybe find a better solution in the future.
         */
        
        
        // REMOVED: `reqAccel = speed * 12;` (empirical override).
        // Honour the caller-requested acceleration. Host simulation of the real
        // FAS planner proves that from REST every reported (speed, accel) pair
        // stops exactly on target with ZERO overshoot, so there is no need to
        // inflate accel to mask the retarget-while-moving overshoot — the rest
        // guarantee further down removes that race directly.
        if (reqAccel <= 0)  reqAccel = 100000; // sane default when caller passes <= 0
        if (reqAccel < 100) reqAccel = 100;    // FAS sanity floor
        
        /*
        if ((speed > kMidSpeedHz && reqAccel > 0 && reqAccel < kSafeMinAccel))
            reqAccel = 1000000;
        }
        if ((speed > kHighSpeedHz && reqAccel > 0 && reqAccel < kSafeMinAccel))
        {
            log_d("Auto-bumping accel %d -> %d on motor %d (speed %d Hz)",
                  (int)reqAccel, (int)kSafeMinAccel, i, speed);
            reqAccel = kSafeMinAccel;
        }
        const int32_t effAccel = (reqAccel > 0) ? reqAccel : MAX_ACCELERATION_A;
        */
        if (faststeppers[i]->setAcceleration(reqAccel) != 0)
            log_e("setAcceleration(%d) rejected on motor %d", reqAccel, i);
        else 
            log_i("setAcceleration(%d) accepted on motor %d", reqAccel, i);
            //faststeppers[i]->setAcceleration(MAX_ACCELERATION_A);
        

        // prolong the time the enable pin goes to high again
        faststeppers[i]->setDelayToDisable(500);

        // Whenever we enqueue another move check the queue first:
        while (faststeppers[i]->isQueueFull())
        {
            delayMicroseconds(50);
        }

        // --- REST GUARANTEE for bounded moves -----------------------------
        // A bounded (non-forever) move MUST be planned from rest. If the axis is
        // still ramping or the RMT/queue still holds commands, planning a new
        // target now makes FAS overshoot by v^2/(2a) before its REVERSE logic
        // recovers — exactly the reported "first fast, then crawls back / overruns
        // the hard limit" symptom. So: hard-stop, pin to the LIVE position
        // (getCurrentPosition(), consistent with stopFastAccelStepper;
        // getPositionAfterCommandsCompleted() is the queue END, far ahead of the
        // motor while it is moving), then wait until VERIFIABLY idle before we
        // re-arm speed/accel and issue the move.
        if (!getData()[i]->isforever)
        {
            const uint8_t rs0 = faststeppers[i]->rampState() & RAMP_STATE_MASK;
            const bool notIdle   = (rs0 != RAMP_STATE_IDLE);
            const bool queueBusy = !faststeppers[i]->isQueueEmpty();
            if (notIdle || queueBusy)
            {
                const int32_t pinPos = faststeppers[i]->getCurrentPosition();
                log_i("Motor %d not at rest before bounded move (rs=0x%02x queueBusy=%d) - hard-stopping, pin=%ld",
                      i, rs0, (int)queueBusy, (long)pinPos);
                faststeppers[i]->forceStopAndNewPosition(pinPos);

                // forceStop is a HARD stop (clears the queue; the RMT only drains
                // its small hardware buffer), so this converges within a few ms
                // regardless of the configured acceleration. Use a generous
                // ceiling so we never plan a new move on top of a draining queue.
                uint32_t t0 = millis();
                while (((faststeppers[i]->rampState() & RAMP_STATE_MASK) != RAMP_STATE_IDLE
                        || !faststeppers[i]->isQueueEmpty())
                       && (millis() - t0) < 200)
                    delayMicroseconds(100);

                const bool atRest =
                    ((faststeppers[i]->rampState() & RAMP_STATE_MASK) == RAMP_STATE_IDLE)
                    && faststeppers[i]->isQueueEmpty();
                if (!atRest)
                    log_e("Motor %d did NOT reach rest within 200ms (rs=0x%02x) - move may overshoot",
                          i, faststeppers[i]->rampState() & RAMP_STATE_MASK);

                // forceStop may have perturbed the ramp params; re-arm them.
                faststeppers[i]->setSpeedInHz(speed);
                faststeppers[i]->setAcceleration(reqAccel);
            }
        }

        if (getData()[i]->isforever)
        {
            // run forver (e.g. PSx or initaited via Serial)
            for (int iStart = 0; iStart < 10; iStart++)
            { // TODO: this is weird, but sometimes necessary for the very first start it seems - timing issue with the QUEUE? // TODO: / FIXME: !!
                if (getData()[i]->speed > 0)
                {
                    // run clockwise
                    faststeppers[i]->runForward();
                    log_i("runForward, speed: %i, isRunning %i", getData()[i]->speed, isRunning(i));
                }
                else if (getData()[i]->speed < 0)
                {
                    // run counterclockwise
                    faststeppers[i]->runBackward();
                    log_i("runBackward, speed: %i, isRunning %i", getData()[i]->speed, isRunning(i));
                }
                if (isRunning(i))
                {
                    log_i("We need %i starts to get the motor running", iStart);
                    break;
                }
            }
        }
        else
        {
#ifdef TMC_CONTROLLER_NOT
            // Boost current 1.5x for high-speed moves to avoid stalling.
            Preferences preferences;
            preferences.begin("tmc", false);
            uint16_t rmsCurr = preferences.getInt("current", pinConfig.tmc_rms_current);
            preferences.end();
            if (abs(speed) > 10000) rmsCurr = (uint16_t)(rmsCurr * 1.5f);
            TMCController::setTMCCurrent(rmsCurr);
#endif

            // Diagnostic snapshot of the planning state. If a single move from a
            // verified-idle axis STILL overshoots on hardware, the divergence is
            // BELOW the planner (RMT pulse generation) — these numbers will then
            // show rampState != IDLE or curPos far from the queue-end at plan time.
            log_i("plan move motor %d: rs=0x%02x curPos=%ld queueEndPos=%ld speed=%d accel=%d target=%d isabs=%d",
                  i, faststeppers[i]->rampState() & RAMP_STATE_MASK,
                  (long)faststeppers[i]->getCurrentPosition(),
                  (long)faststeppers[i]->getPositionAfterCommandsCompleted(),
                  speed, (int)reqAccel,
                  getData()[i]->targetPosition, (int)getData()[i]->absolutePosition);

            int8_t mres;
            if (getData()[i]->absolutePosition)
                mres = faststeppers[i]->moveTo(getData()[i]->targetPosition, false);
            else
                mres = faststeppers[i]->move(getData()[i]->targetPosition, false);
            if (mres != MOVE_OK)
                log_e("motor %d move/moveTo rejected (rc=%d target=%d isabs=%d) - check speed/accel set",
                      i, (int)mres, getData()[i]->targetPosition, (int)getData()[i]->absolutePosition);

            // spin until queue started
            uint32_t t0 = millis();
            while (!faststeppers[i]->isRunning() && millis() - t0 < 20) {}
            if (!faststeppers[i]->isRunning())
            {
                log_e("motor %d never got going - cancelling", i);
                return;
            }
        }
        getData()[i]->stopped = false;
        log_i("start stepper (act): motor:%d isforever:%d speed:%d target:%d isabs:%d accel:%d isRunning:%d",
              i, getData()[i]->isforever, getData()[i]->speed,
              getData()[i]->targetPosition, getData()[i]->absolutePosition,
              reqAccel, isRunning(i));
    }


    void setupFastAccelStepper()
    {
        if (getData()[Stepper::A] == nullptr)
            log_e("Stepper A getData() NULL");
        if (getData()[Stepper::X] == nullptr)
            log_e("Stepper X getData() NULL");
        if (getData()[Stepper::Y] == nullptr)
            log_e("Stepper Y getData() NULL");
        if (getData()[Stepper::Z] == nullptr)
            log_e("Stepper Z getData() NULL");

        // Initialize engine - RMT drivers will be used instead of PCNT to avoid conflicts
        engine.init();

        log_i("FastAccelStepper using RMT drivers to avoid PCNT conflicts with ESP32Encoder");

#ifdef USE_TCA9535
        log_i("Using TCA9535");
        engine.setExternalCallForPin(_externalCallForPin);
#endif

        // setup the getData()
        for (int i = 0; i < faststeppers.size(); i++)
        {
            faststeppers[i] = nullptr;
        }
        // setup the stepper A
        if (pinConfig.MOTOR_A_STEP >= 0)
        {
            setupFastAccelStepper(Stepper::A, pinConfig.MOTOR_ENABLE | PIN_EXTERNAL_FLAG, pinConfig.MOTOR_A_DIR | PIN_EXTERNAL_FLAG, pinConfig.MOTOR_A_STEP);
        }

        // setup the stepper X
        if (pinConfig.MOTOR_X_STEP >= 0)
        {
            setupFastAccelStepper(Stepper::X, pinConfig.MOTOR_ENABLE | PIN_EXTERNAL_FLAG, pinConfig.MOTOR_X_DIR | PIN_EXTERNAL_FLAG, pinConfig.MOTOR_X_STEP);
        }

        // setup the stepper Y
        if (pinConfig.MOTOR_Y_STEP >= 0)
        {
            setupFastAccelStepper(Stepper::Y, pinConfig.MOTOR_ENABLE | PIN_EXTERNAL_FLAG, pinConfig.MOTOR_Y_DIR | PIN_EXTERNAL_FLAG, pinConfig.MOTOR_Y_STEP);
        }

        // setup the stepper Z
        if (pinConfig.MOTOR_Z_STEP >= 0)
        {
            setupFastAccelStepper(Stepper::Z, pinConfig.MOTOR_ENABLE | PIN_EXTERNAL_FLAG, pinConfig.MOTOR_Z_DIR | PIN_EXTERNAL_FLAG, pinConfig.MOTOR_Z_STEP);
        }
    }

    void setupFastAccelStepper(Stepper stepper, int motoren, int motordir, int motorstp)
    {
        // log_i("setupFastAccelStepper %i with motor pins: %i, %i, %i", stepper, motoren, motordir, motorstp);
        // log_i("Heap before setupFastAccelStepper: %d", ESP.getFreeHeap());

        // Driver selection.
        //
        // The MCPWM/PCNT backend in our FastAccelStepper fork does NOT
        // honor the external-direction-pin handshake correctly: even when
        // the wrapper returns the *previous* state (signalling "DIR not
        // settled yet"), the MCPWM/PCNT pipeline still emits a few
        // already-loaded step pulses on direction reversal. Result: the
        // STAGE walks +3 extra steps every time the move reverses
        // direction. The standalone reproducer (which uses DRIVER_RMT)
        // does not show this — the RMT ISR re-applies the repeating
        // pause command without firing additional STEP pulses.
        //
        // Therefore: when ANY motor pin is routed through the TCA9535
        // I/O expander (USE_TCA9535), the external-pin handshake is
        // mandatory and we MUST use the RMT backend, regardless of
        // LED_CONTROLLER.
        //
        // RMT vs NeoPixel coexistence: FAS-RMT calls rmt_isr_register()
        // (a global shared ISR) and Adafruit_NeoPixel calls
        // rmt_driver_install(). They are mutually exclusive on the same
        // RMT controller, but each FAS stepper allocates its own RMT
        // channel (channels 0..3 for steppers, channels 4..7 free for
        // NeoPixel) and rmt_isr_register vs rmt_driver_install can
        // coexist as long as they target different channels — which is
        // already the case here. The historical comment about a panic
        // was for the old single-RMT-ISR path.
#if defined(USE_TCA9535)
        // External direction pins via TCA: must use RMT for correct
        // dir-change handshake. No fallback — MCPWM/PCNT would silently
        // re-introduce the +3-step bug on every direction reversal.
        faststeppers[stepper] = engine.stepperConnectToPin(motorstp, DRIVER_RMT);
        if (faststeppers[stepper] == nullptr)
        {
            log_e("Motor %d: RMT unavailable but USE_TCA9535 requires it. "
                  "Direction-reversal handshake will not work — refusing to "
                  "fall back to MCPWM/PCNT to avoid silent +3-step drift.",
                  stepper);
        }
        else
        {
            log_i("Motor %d: using RMT driver (required for TCA9535 dir handshake)", stepper);
        }
#elif defined(LED_CONTROLLER) && !defined(LINEAR_ENCODER_CONTROLLER)
        // NeoPixel-only board with native dir pins: no external-pin
        // handshake needed, so MCPWM/PCNT is fine and frees RMT for
        // NeoPixel exclusively.
        faststeppers[stepper] = engine.stepperConnectToPin(motorstp);
        if (faststeppers[stepper] == nullptr)
        {
            log_e("Motor %d: failed to create MCPWM/PCNT stepper", stepper);
        }
        else
        {
            log_i("Motor %d: using MCPWM/PCNT driver (LED_CONTROLLER keeps RMT for NeoPixel)", stepper);
        }
#else
        // Native direction pins (no TCA external-pin handshake): use the
        // MCPWM/PCNT backend as the PRIMARY step generator.
        //
        // WHY MCPWM/PCNT and not RMT here: the RMT backend strands the last
        // 1-12 steps of a move at low instantaneous step rates. Its refill
        // runs in continuous mode and is threshold-interrupt driven; once the
        // step period grows past a threshold the refill interrupts stop firing
        // before the queue drains, so the RMT freezes mid-stream with
        // rampState=IDLE but isRunning=1 forever. The move then never reports
        // complete (final position 1-12 steps short), which surfaces as
        // "motor moves and never stops" plus qid-timeouts. Reproduced in a
        // standalone single-axis firmware with NO CANopen/serial concurrency,
        // and the failure is NON-MONOTONIC in speed (so it cannot be tuned
        // away with min speed/accel limits). The MCPWM/PCNT backend - FAS's
        // original, battle-tested generator - passes a full 70/70 speed x
        // accel sweep including extreme low accel that RMT could never finish.
        //
        // Safe here: no external-direction-pin handshake is in play (that is
        // the ONLY scenario where MCPWM/PCNT misbehaves - +3-step drift on
        // reversal, see the USE_TCA9535 branch above).
        //
        // PCNT-unit coordination: the MCPWM backend hard-maps stepper queue 0
        // -> PCNT_UNIT_0. The X linear encoder (ESP32Encoder) is steered onto
        // PCNT_UNIT_1 in PCNTEncoderController::setup() so they never collide
        // on the same PCNT hardware unit.
        faststeppers[stepper] = engine.stepperConnectToPin(motorstp, DRIVER_MCPWM_PCNT);
        if (faststeppers[stepper] == nullptr)
        {
            // MCPWM/PCNT exhausted (all PCNT units taken): fall back to RMT so
            // the axis still moves. RMT may strand trailing steps at low rates,
            // but a degraded axis beats a dead one.
            faststeppers[stepper] = engine.stepperConnectToPin(motorstp, DRIVER_RMT);
            log_w("Motor %d: MCPWM/PCNT unavailable, falling back to RMT "
                  "(trailing-step stall possible at low step rates)", stepper);
        }
        else
        {
            log_i("Motor %d: using MCPWM/PCNT driver (robust trailing-step flush)", stepper);
        }
#endif
        // Null guard: if no backend could be allocated, bail out before the
        // dereferences below. (For USE_TCA9535 we intentionally do NOT fall
        // back to MCPWM/PCNT - see that branch - so a null here means the axis
        // stays disabled rather than silently drifting +3 steps per reversal.)
        if (faststeppers[stepper] == nullptr)
        {
            log_e("Motor %d: no step generator could be allocated - axis disabled", stepper);
            return;
        }


        faststeppers[stepper]->setEnablePin(motoren, pinConfig.MOTOR_ENABLE_INVERTED);
        faststeppers[stepper]->setDirectionPin(motordir, getData()[stepper]->directionPinInverted);

        if (pinConfig.MOTOR_AUTOENABLE)
            faststeppers[stepper]->setAutoEnable(pinConfig.MOTOR_AUTOENABLE);
        else
            faststeppers[stepper]->enableOutputs();
        faststeppers[stepper]->setSpeedInHz(MAX_VELOCITY_A);
        faststeppers[stepper]->setAcceleration(DEFAULT_ACCELERATION);
        faststeppers[stepper]->setCurrentPosition(getData()[stepper]->currentPosition);
        // Note: previously there were two priming move(2)/move(-2) calls here.
        // They were non-blocking, queued asynchronously, and could coalesce
        // with the first user move, producing unaccounted direction reversals
        // and extra step pulses. The FAS engine handles cold starts fine on
        // its own; do not re-add them.
    }

    /*
    void stopFastAccelStepper(int i)
    {
        if (faststeppers[i] == nullptr)
            return;
        // Hard stop with consistent post-stop position. Don't combine
        // forceStop() and stopMove(): forceStop() halts the ramp generator
        // but does not empty the queue, and the follow-up stopMove() then
        // becomes a no-op. forceStopAndNewPosition() pins the position to
        // the current value so getCurrentPosition() below is meaningful.
        faststeppers[i]->forceStopAndNewPosition(faststeppers[i]->getCurrentPosition());
        log_i("stop stepper in FAccelStep %i", i);
        getData()[i]->isforever = false;
        getData()[i]->speed = 0;
        getData()[i]->currentPosition = faststeppers[i]->getCurrentPosition();
        getData()[i]->stopped = true;
    }
    */

void stopFastAccelStepper(int i)
    {
        // Guard: faststeppers is sized for LOCAL axes only.
        if (i < 0 || i >= (int)faststeppers.size())
        {
            log_d("stopFastAccelStepper: axis %d has no local stepper (REMOTE or OUT_OF_RANGE)", i);
            return;
        }
        if (faststeppers[i] == nullptr)
            return;
        FastAccelStepper* s = faststeppers[i];

        // Choose the post-stop position carefully:
        //   - If ramp is IDLE, the move completed normally. The queue-end
        //     position is where the motor physically is (RMT may have
        //     emitted pulses the position counter hasn't accounted for yet).
        //   - If ramp is non-IDLE (user-initiated stop mid-motion), use the
        //     live position — the motor was actually moving and queue-end
        //     is far ahead of where we want to halt.
        const uint8_t rs = s->rampState() & RAMP_STATE_MASK;
        const int32_t finalPos =
            (rs == RAMP_STATE_IDLE)
                ? s->getPositionAfterCommandsCompleted()
                : s->getCurrentPosition();

        s->forceStopAndNewPosition(finalPos);
        log_i("stop stepper in FAccelStep %d (rs=0x%02x, pinnedPos=%ld)",
              i, rs, (long)finalPos);
        getData()[i]->isforever = false;
        getData()[i]->speed = 0;
        getData()[i]->currentPosition = finalPos;
        getData()[i]->stopped = true;
    }

    void setExternalCallForPin(
        bool (*func)(uint8_t pin, uint8_t value))
    {
        _externalCallForPin = func;
    }

    void updateData(int i)
        {
        // Guard: faststeppers is sized for LOCAL axes only.
        if (i < 0 || i >= (int)faststeppers.size())
        {
            log_d("updateData: axis %d has no local stepper (REMOTE or OUT_OF_RANGE)", i);
            return;
        }
        if (faststeppers[i] == nullptr)
        {
            log_d("FastAccelStepper for axis %d is null in updateData", i);
            return;
        }
        FastAccelStepper* s = faststeppers[i];
        const uint8_t rs = s->rampState() & RAMP_STATE_MASK;
        // When idle, the queue-end position is the physical truth on RMT.
        // When moving, the live position is what we want for telemetry.
        getData()[i]->currentPosition =
            (rs == RAMP_STATE_IDLE)
                ? s->getPositionAfterCommandsCompleted()
                : s->getCurrentPosition();
    }

    void setAutoEnable(bool enable)
    {
        for (int i = 0; i < faststeppers.size(); i++)
        {
            if (faststeppers[i] != nullptr)
            {
                faststeppers[i]->setAutoEnable(enable);
            }
        }
    }

    void Enable(bool enable)
    {
        for (int i = 0; i < faststeppers.size(); i++)
        {
            if (faststeppers[i] == nullptr)
            {
                continue;
            }
            if (enable)
            {
                faststeppers[i]->enableOutputs();
                faststeppers[i]->setAutoEnable(false);
            }
            else
            {
                faststeppers[i]->disableOutputs();
                faststeppers[i]->setAutoEnable(false);
            }
        }
    }

    void setPosition(Stepper s, int val)
    {
        if (faststeppers[s] == nullptr)
        {
            log_e("FastAccelStepper for axis %d is null in setPosition", s);
            return;
        }
        faststeppers[s]->setCurrentPosition(val);
    }

bool isRunning(int i)
    {
        if (faststeppers[i] == nullptr)
        {
            return false;
        }
        FastAccelStepper* s = faststeppers[i];

        // "Running" is true if EITHER the ramp generator is still
        // producing commands (rampState != IDLE) OR the FAS command
        // queue still holds entries that the RMT/MCPWM pipeline has
        // not consumed yet.
        //
        // The previous implementation only tested rampState. On ESP32
        // RMT this returns IDLE up to ~20 ms before the motor actually
        // stops (the FAS forward-planning horizon). During that window
        // FocusMotor::loop saw "isRunning=false", a follow-up
        // /motor_act got dispatched, and the new move() call appended
        // its target onto the still-draining queue — producing the
        // observed "stuck at lower speed / never stops / overruns
        // hard limit" symptoms. Including the queue check closes that
        // race.
        const uint8_t rs = s->rampState() & RAMP_STATE_MASK;
        if (rs != RAMP_STATE_IDLE) return true;
        if (!s->isQueueEmpty()) return true;
        return false;
    }

    int rampState(int i)
    {
        if (faststeppers[i] == nullptr)
        {
            log_d("FastAccelStepper for axis %d is null in rampState", i);
            return -1;
        }
        return faststeppers[i]->rampState();
    }

    int32_t getCurrentSpeedInMilliHz(int i, bool withSign)
    {
        if (faststeppers[i] == nullptr)
        {
            log_d("FastAccelStepper for axis %d is null in getCurrentSpeedInMilliHz", i);
            return 0;
        }
        int32_t speed = faststeppers[i]->getCurrentSpeedInMilliHz();
        if (!withSign)
        {
            speed = abs(speed);
        }
        return speed;
    }

    int32_t getCurrentPosition(int i)
    {
        if (faststeppers[i] == nullptr)
        {
            log_d("FastAccelStepper for axis %d is null in getCurrentPosition", i);
            return 0;
        }
        return faststeppers[i]->getCurrentPosition();
    }

    int32_t getPositionAfterCommandsCompleted(int i)
    {
        if (faststeppers[i] == nullptr)
        {
            log_d("FastAccelStepper for axis %d is null in getPositionAfterCommandsCompleted", i);
            return 0;
        }
        return faststeppers[i]->getPositionAfterCommandsCompleted();
    }

    bool isQueueFull(int i)
    {
        if (faststeppers[i] == nullptr)
        {
            log_d("FastAccelStepper for axis %d is null in isQueueFull", i);
            return false;
        }
        return faststeppers[i]->isQueueFull();
    }

    void move(Stepper s, int steps, bool blocking)
    {
        if (faststeppers[s] == nullptr)
        {
            log_e("FastAccelStepper for axis %d is null in move", s);
            return;
        }
        // move the motor by the given steps
        faststeppers[s]->move(steps, blocking);
    }

    long getCurrentPosition(Stepper s)
    {
        if (faststeppers[s] == nullptr)
        {
            log_e("FastAccelStepper for axis %d is null", s);
            return 0;
        }
        return faststeppers[s]->getCurrentPosition();
    }
}
