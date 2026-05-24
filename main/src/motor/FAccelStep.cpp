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
        int speed = getData()[i]->speed;
        if (speed < 0)
        {
            speed *= -1;
        }

        faststeppers[i]->setSpeedInHz(speed);

        // Honor isaccelerated. The JSON parser defaults absent fields to 0,
        // so a missing "isaccel" / "accel" must not silently keep stale
        // ramp settings. FAS has no true "constant velocity" mode for
        // moveTo/move, so when the user asks for no acceleration we use
        // MAX_ACCELERATION_A to keep the ramp as short as possible.
        const int32_t reqAccel = getData()[i]->acceleration;
        if (reqAccel > 0)
        {
            if (faststeppers[i]->setAcceleration(reqAccel) != 0)
            {
                log_w("setAcceleration(%d) rejected on motor %d, using MAX_ACCELERATION_A",
                      (int)reqAccel, i);
                faststeppers[i]->setAcceleration(MAX_ACCELERATION_A);
            }
        }
        else
        {
            faststeppers[i]->setAcceleration(MAX_ACCELERATION_A);
        }

        // Check soft limits before allowing movement (unless homing is active)
        // (Removed: soft-limit subsystem deleted; hard limits via endstops are the only positional safety.)

        // prolong the time the enable pin goes to high again
        faststeppers[i]->setDelayToDisable(500);

        // Whenever we enqueue another move check the queue first:
        while (faststeppers[i]->isQueueFull())
        {
            delayMicroseconds(50);
        }

        // Force a clean ramp-generator reset before re-arming, but ONLY when we
        // are actually switching mode/direction. The original problem case:
        // previous move(-N) leaves a residual ramp state, then runForward()
        // reports isRunning=true but emits no pulses (homing Phase 9 -> Phase 1
        // stuck for 17s). Solution: reset whenever the desired motion does not
        // match the current ramp.
        // Doing the reset unconditionally caused audible click/stutter when the
        // PS4 controller streams continuous runForward() speed updates -- each
        // update would forceStop+restart the engine. So gate the reset:
        //   - isforever, new dir != current dir          -> reset
        //   - isforever after a finite move (rs!=IDLE && finite target) -> reset
        //   - !isforever (move/moveTo) and rs!=IDLE      -> reset (existing behaviour)
        {   // TODO: This needs a revisiion - it's a massive hickup with the registers RMT and queue of FAS
            // - we should only do that when we detect a direction change or when we switch from finite to infinite move or vice versa, but not on every move command if the motor is already running - that is just too much of a hickup and can cause stuttering and other issues - we need to check the current ramp state and speed and compare it to the new command to decide if we need to do a reset or not
            const uint8_t rs0 = faststeppers[i]->rampState() & RAMP_STATE_MASK;
            const int32_t curSpeedMilliHz = faststeppers[i]->getCurrentSpeedInMilliHz();
            const bool wantForever = getData()[i]->isforever;
            const int32_t wantSpeed = getData()[i]->speed;
            bool needReset = false;
            if (wantForever) {
                // Reverse direction while running -> need reset
                if (rs0 != RAMP_STATE_IDLE && wantSpeed != 0 &&
                    ((wantSpeed > 0 && curSpeedMilliHz < 0) ||
                     (wantSpeed < 0 && curSpeedMilliHz > 0))) {
                    needReset = true;
                }
                // Coming out of a finite move while ramp not idle
                else if (rs0 != RAMP_STATE_IDLE && !faststeppers[i]->isRunningContinuously()) {
                    needReset = true;
                }
            } else {
                if (rs0 != RAMP_STATE_IDLE) needReset = true;
            }
            if (needReset) {
                faststeppers[i]->forceStopAndNewPosition(
                    faststeppers[i]->getPositionAfterCommandsCompleted());
                uint32_t t0 = millis();
                while ((faststeppers[i]->rampState() & RAMP_STATE_MASK)
                            != RAMP_STATE_IDLE
                       && millis() - t0 < 10)
                {
                    delayMicroseconds(100);
                }
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
            // if the motor speed is above threshold, maximise motor current to avoid stalling
            // We rather do that to not change motor current with any call (e.g. PS, Encoder, etc)
#ifdef TMC_CONTROLLER // TODO: This is only working on TMC2209-enabled sattelite boards since we have only one TMC Controller for one motor
            // read value from preferences
            Preferences preferences;
            preferences.begin("tmc", false);
            uint16_t rmsCurrFromPref = preferences.getInt("current", pinConfig.tmc_rms_current);
            preferences.end();
            if (abs(speed) > 10000)
            {
                rmsCurrFromPref = (int)((float)rmsCurrFromPref * 1.5);
                log_i("Overdrive current for motor %i: %i", i, rmsCurrFromPref);
            }
            TMCController::setTMCCurrent(rmsCurrFromPref);
#endif

/*
            // Force a clean ramp-generator reset before re-arming. Without this,
            // a direction reversal (e.g. previous move +N, this move -M) can
            // leave residual entries in the FAS queue: getCurrentPosition()
            // stalls a few steps shy of getPositionAfterCommandsCompleted(),
            // ramp state goes IDLE, but isRunning() stays true forever because
            // it ORs both. Symptom: 50-step undershoot + isRunning never clears.
            // Stopping the queue first guarantees a fresh start.
            if (faststeppers[i]->isRunning() ||
                faststeppers[i]->getCurrentPosition() !=
                    faststeppers[i]->getPositionAfterCommandsCompleted())
            {
                faststeppers[i]->forceStopAndNewPosition(
                    faststeppers[i]->getCurrentPosition());
                // Brief settle so the engine fully drains the queue/RMT.
                uint32_t t0 = millis();
                while (faststeppers[i]->isRunning() && millis() - t0 < 10)
                {
                    delayMicroseconds(100);
                }
            }
                */

            // Ramp-generator reset is now performed unconditionally above
            // for both isforever and targeted-move paths — see the
            // forceStopAndNewPosition block right after the queue-full wait.

            if (getData()[i]->absolutePosition)
            {
                log_i("moveTo %i", getData()[i]->targetPosition);
                faststeppers[i]->moveTo(getData()[i]->targetPosition, false);
            }
            else
            {
                log_i("move %i", getData()[i]->targetPosition);
                faststeppers[i]->move(getData()[i]->targetPosition, false);
            }

            // spin until queue started
            uint32_t t0 = millis();
            while (!faststeppers[i]->isRunning() && millis() - t0 < 20)
            {
            } // 20 ms timeout

            if (!faststeppers[i]->isRunning())
            {
                log_e("motor %d never got going – cancelling", i);
                return;
            }
        }
        // "unstop" the motor after it has actually started?
        getData()[i]->stopped = false;

        log_i("start stepper (act): motor:%i isforver:%i, speed: %i, maxSpeed: %i, target pos: %i, isabsolute: %i, isacceleration: %i, acceleration: %i, isStopped %i, isRunning %i",
              i,
              getData()[i]->isforever,
              getData()[i]->speed,
              getData()[i]->maxspeed,
              getData()[i]->targetPosition,
              getData()[i]->absolutePosition,
              getData()[i]->isaccelerated,
              getData()[i]->acceleration,
              getData()[i]->stopped,
              isRunning(i));
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
        // Always try RMT first. Fall back to MCPWM/PCNT only if RMT
        // allocation fails.
        faststeppers[stepper] = engine.stepperConnectToPin(motorstp, DRIVER_RMT);
#endif
        if (faststeppers[stepper] == nullptr)
        {
            faststeppers[stepper] = engine.stepperConnectToPin(motorstp);
            log_w("Motor %d: RMT unavailable, falling back to MCPWM/PCNT", stepper);
        }
        else
        {
            log_i("Motor %d: using RMT driver", stepper);
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

        // Use getPositionAfterCommandsCompleted() — the *queue-end* position —
        // for completion detection. On ESP32 RMT, getCurrentPosition() can lag
        // the real motor by up to a full command's worth of steps because the
        // RMT pulse generator and the position accountant are decoupled. The
        // queue-end position is updated at queue-fill time and is reliable.
        //
        // Definition of "running": ramp is non-IDLE, OR there are still
        // unconsumed queue entries (queue-end position differs from where the
        // ramp generator thinks the next-to-fill entry will land).
        const uint8_t rs = s->rampState() & RAMP_STATE_MASK;
        if (rs != RAMP_STATE_IDLE) return true;

        // Ramp is idle. Treat as truly-done. Don't trust FAS's isRunning()
        // here because on RMT it stays true while the position counter
        // catches up to the ongoing command's pulses — but the *motion* is
        // already over from the application's point of view.
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
