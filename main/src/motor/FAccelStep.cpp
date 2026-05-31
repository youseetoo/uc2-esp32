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

        if (faststeppers[i]->setSpeedInHz(speed) != 0)
            log_w("setSpeedInHz(%d) rejected on motor %d", speed, i);

        // Empirical safety floor: for speeds > 40 kHz, FAS plans degenerate
        // ramps (slow motion + overshoot) when accel < ~170000. Sharp,
        // reproducible boundary; likely a pmfl rounding edge in
        // calculate_ramp_steps. Auto-bump and warn. // FIXME:/TODO: This is weird, was found emperically on the esp32s3 canopen 
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
        constexpr int32_t kHighSpeedHz  = 35000;
        constexpr int32_t kSafeMinAccel = 200000;
        if (speed > kHighSpeedHz && reqAccel > 0 && reqAccel < kSafeMinAccel)
        {
            log_w("Auto-bumping accel %d -> %d on motor %d (speed %d Hz)",
                  (int)reqAccel, (int)kSafeMinAccel, i, speed);
            reqAccel = kSafeMinAccel;
        }
        const int32_t effAccel = (reqAccel > 0) ? reqAccel : MAX_ACCELERATION_A;
        if (faststeppers[i]->setAcceleration(effAccel) != 0)
            faststeppers[i]->setAcceleration(MAX_ACCELERATION_A);

        // prolong the time the enable pin goes to high again
        faststeppers[i]->setDelayToDisable(500);

        // Whenever we enqueue another move check the queue first:
        while (faststeppers[i]->isQueueFull())
        {
            delayMicroseconds(50);
        }

        if (!getData()[i]->isforever)
        {
            const uint8_t rs0 = faststeppers[i]->rampState() & RAMP_STATE_MASK;
            if (rs0 != RAMP_STATE_IDLE || !faststeppers[i]->isQueueEmpty()) // TODO: Maybe we should rather check how much space is in the queue instead of checking if it is full or not? Because if it is full we have to wait anyway, but if it is not empty but has space for the new move, we can already enqueue the new move and save some time?
            {
                log_i("Motor %d: rampState %d, queue not empty before move command - stopping and resetting position to avoid ramp/queue issues",
                      i, rs0);
                faststeppers[i]->forceStopAndNewPosition(
                    faststeppers[i]->getPositionAfterCommandsCompleted());
                uint32_t t0 = millis();
                while (((faststeppers[i]->rampState() & RAMP_STATE_MASK) != RAMP_STATE_IDLE
                        || !faststeppers[i]->isQueueEmpty())
                       && millis() - t0 < 30)
                    delayMicroseconds(100);
                faststeppers[i]->setSpeedInHz(speed);
                faststeppers[i]->setAcceleration(effAccel);
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
#ifdef TMC_CONTROLLER
            // Boost current 1.5x for high-speed moves to avoid stalling.
            Preferences preferences;
            preferences.begin("tmc", false);
            uint16_t rmsCurr = preferences.getInt("current", pinConfig.tmc_rms_current);
            preferences.end();
            if (abs(speed) > 10000) rmsCurr = (uint16_t)(rmsCurr * 1.5f);
            TMCController::setTMCCurrent(rmsCurr);
#endif

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
              getData()[i]->acceleration, isRunning(i));
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
