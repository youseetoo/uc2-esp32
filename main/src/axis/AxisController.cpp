#include "AxisController.h"
#include "EncoderBackend.h"
#include "AxisWatchdog.h"
#include "AxisNotify.h"
#include "AxisPID.h"
#include <PinConfig.h>
#include "esp_log.h"
#include "../motor/MotorTypes.h" // Stepper enum, MotorData (header-only)

#ifdef MOTOR_CONTROLLER
#include "../motor/FocusMotor.h"
#endif
#if defined(MOTOR_CONTROLLER) && defined(USE_FASTACCEL)
#include "../motor/FAccelStep.h" // setLiveSpeed for the SERVO loop
#define AXIS_SERVO_AVAILABLE 1
#endif
#ifdef TMC_CONTROLLER
#include "../tmc/TMCController.h"
#endif

static const char *TAG = "AxisCtrl";

#ifndef MOTOR_AXIS_COUNT
#define MOTOR_AXIS_COUNT 4
#endif

// Default PCNT unit for the encoder (UNIT_0 is reserved for FastAccelStepper).
#ifndef AXIS_ENC_PCNT_UNIT
#define AXIS_ENC_PCNT_UNIT 1
#endif
// Default glitch filter for the encoder input.
#ifndef AXIS_ENC_GLITCH_FILTER
#define AXIS_ENC_GLITCH_FILTER 1
#endif

// FastAccelStepper backs at most 4 physical steppers (A/X/Y/Z). The motor data
// arrays are MOTOR_AXIS_COUNT wide, but indexing the FAS layer beyond this is
// out of bounds — never touch FAS for axes >= this.
static constexpr int AXIS_MAX_STEPPERS = 4;

namespace AxisController
{
    // Move-job stage for MONITOR/CORRECT (SERVO uses the servo task).
    enum MoveStage : uint8_t
    {
        MJ_IDLE = 0,
        MJ_RUNNING,    // open-loop move in flight
        MJ_VERIFY,     // move done; check error
        MJ_CORRECTING  // corrective move in flight
    };

    struct AxisState
    {
        EncoderBackend       encoder;
        AxisCalibration      cal;
        AxisFeedback         fb;
        AxisWatchdog::Config wdCfg;
        AxisWatchdog::State  wdState;
        bool                 hasEnc = false;
        int32_t              originSteps = 0; // step position at the encoder origin
        // originCounts is always 0 (we zero the hardware counter at the origin).
        // Deferred requests set from another task (CANopen), consumed in loop().
        volatile int8_t      reqMode = -1;      // -1 = none
        volatile int8_t      reqReset = -1;     // -1 = none, else AxisResetPolicy
        volatile bool        reqCalibrate = false;

        // ---- move job (MONITOR/CORRECT), driven in loop() ----
        uint8_t  moveStage = MJ_IDLE;
        int32_t  targetSteps = 0;     // absolute target of the current job
        int32_t  moveSpeed = 0;
        uint8_t  correctRetries = 0;
        int8_t   lastMoveDir = 0;     // last commanded direction (backlash)
        bool     reversalMove = false; // widen verify tolerance for this move

        // ---- derived thresholds (from calibration scatter/backlash) ----
        int32_t  verifyTolSteps = 4;
        int32_t  divergenceThresholdSteps = 20;
        int32_t  backlashSteps = 0;

        // ---- SERVO (driven by the servo task) ----
        AxisPID  pid;
        volatile bool servoActive = false;  // servo task should drive this axis
        volatile bool servoArrived = false; // task -> loop: reached target, finalize
    };

    // Correction bounds (design v2, WP6).
    static constexpr int32_t kMaxCorrectionSteps = 5000; // clamp per corrective move
    static constexpr uint8_t kMaxCorrectionRetries = 2;
    static constexpr int32_t kCorrectionSpeed = 2000;    // steps/s for corrections

    static AxisState g_axes[MOTOR_AXIS_COUNT];
    static bool g_setupDone = false;

    // ------------------------------------------------------------------ helpers
    static uint16_t currentMicrosteps()
    {
#ifdef TMC_CONTROLLER
        uint16_t ms = TMCController::getMicrosteps();
        if (ms == 0)
            ms = (uint16_t)pinConfig.tmc_microsteps;
        return ms;
#else
        return (uint16_t)pinConfig.tmc_microsteps;
#endif
    }

    static int32_t commandedStepsOf(int axis)
    {
#ifdef MOTOR_CONTROLLER
        if (axis < 0 || axis >= AXIS_MAX_STEPPERS)
            return 0; // no FAS stepper backs this index — avoid OOB access
        return (int32_t)FocusMotor::getCurrentMotorPosition(axis);
#else
        (void)axis;
        return 0;
#endif
    }

    static bool axisMoving(int axis)
    {
#ifdef MOTOR_CONTROLLER
        if (axis < 0 || axis >= AXIS_MAX_STEPPERS)
            return false;
        return FocusMotor::isRunning(axis);
#else
        (void)axis;
        return false;
#endif
    }

    // |commanded speed| in steps/s for the axis (gates the watchdog).
    static int32_t commandedSpeedOf(int axis)
    {
#ifdef MOTOR_CONTROLLER
        MotorData **d = FocusMotor::getData();
        if (d && d[axis])
            return abs((int32_t)d[axis]->speed);
#endif
        (void)axis;
        return 0;
    }

    // Derive watchdog thresholds from the measured encoder noise. Called after a
    // calibration is loaded/produced so tolerances follow real scatter.
    static void configureWatchdog(int axis)
    {
        AxisState &a = g_axes[axis];
        AxisWatchdog::Config &c = a.wdCfg;
        // "Progress" must clear a few sigma of quantisation noise.
        uint16_t scatter = a.cal.valid ? a.cal.residualScatter : 3;
        if (scatter < 1)
            scatter = 1;
        c.noiseThresholdCounts = (uint16_t)(scatter * 3);
        if (c.noiseThresholdCounts < 2)
            c.noiseThresholdCounts = 2;
        // Lag limit: a generous multiple of scatter expressed in steps, so a
        // genuine lost-step burst trips but calibrated backlash does not.
        if (a.cal.valid)
        {
            int32_t scatterSteps = abs(axisCountsToSteps(a.cal, (int64_t)scatter * 8));
            a.backlashSteps = abs(axisCountsToSteps(a.cal, a.cal.backlashCounts));
            c.lagLimitSteps = scatterSteps + a.backlashSteps + 20;

            // Verify tolerance (CORRECT) and divergence threshold (MONITOR) also
            // follow measured noise: a couple of sigma for verify, more for a
            // divergence alert.
            int32_t sigmaSteps = abs(axisCountsToSteps(a.cal, scatter));
            a.verifyTolSteps = sigmaSteps * 2 + 2;
            a.divergenceThresholdSteps = sigmaSteps * 4 + a.backlashSteps + 4;
        }
        c.enabled = a.hasEnc;
    }

    // Re-zero the encoder and pin the current step position as the origin.
    static void reanchorOrigin(int axis)
    {
        AxisState &a = g_axes[axis];
        if (!a.hasEnc)
            return;
        a.encoder.zero();
        a.originSteps = commandedStepsOf(axis);
    }

    // Is the axis being forced to abort (endstop / e-stop)? Used by calibration.
    static bool axisAborted(int axis)
    {
#ifdef MOTOR_CONTROLLER
        MotorData **d = FocusMotor::getData();
        if (d && d[axis])
            return d[axis]->hardLimitTriggered || d[axis]->endstop_hit;
#endif
        (void)axis;
        return false;
    }

    // Blocking relative move used only by the on-demand calibration routine.
    static bool blockingRelMove(int axis, int32_t deltaSteps, int32_t speed)
    {
#ifdef MOTOR_CONTROLLER
        if (axisAborted(axis))
            return false;
        FocusMotor::moveMotor(deltaSteps, abs(speed), axis, true /*relative*/);

        const uint32_t startTimeoutMs = 300;
        const uint32_t moveTimeoutMs = 15000;
        uint32_t t0 = millis();
        // Wait for the motor to actually start.
        while (!FocusMotor::isRunning(axis) && (millis() - t0) < startTimeoutMs)
        {
            if (axisAborted(axis))
                return false;
            delay(1);
        }
        // Wait for completion.
        while (FocusMotor::isRunning(axis))
        {
            if (axisAborted(axis))
            {
                FocusMotor::stopStepper(axis);
                return false;
            }
            if ((millis() - t0) > moveTimeoutMs)
            {
                FocusMotor::stopStepper(axis);
                ESP_LOGE(TAG, "blockingRelMove ax%d timed out", axis);
                return false;
            }
            delay(2);
        }
        return true;
#else
        (void)axis; (void)deltaSteps; (void)speed;
        return false;
#endif
    }

    // Latch a fault: stop the axis, mark health, and fire an async notification.
    // Motion stays refused until resetAxis() clears it (enforced in moveTo).
    static void raiseFault(int axis, AxisFault fault)
    {
        AxisState &a = g_axes[axis];
        a.fb.fault = fault;
        a.fb.health = HEALTH_FAULT;
#ifdef MOTOR_CONTROLLER
        FocusMotor::stopStepper(axis);
#endif
        AxisNotify::Event e;
        e.axis = (uint8_t)axis;
        e.fault = (uint8_t)fault;
        e.posErrSteps = a.fb.positionErrorSteps;
        e.commandedSteps = a.fb.commandedSteps;
        e.measuredSteps = a.fb.measuredSteps;
        AxisNotify::report(e);
        ESP_LOGW(TAG, "axis %d FAULT %d (posErr=%d) — motion latched", axis, fault,
                 a.fb.positionErrorSteps);
    }

    // Recompute one axis's feedback from the encoder (or identity if none).
    static void updateFeedback(int axis)
    {
        AxisState &a = g_axes[axis];
        int32_t commanded = commandedStepsOf(axis);
        a.fb.commandedSteps = commanded;

        if (!a.hasEnc)
        {
            // Identity feedback so downstream code never branches.
            a.fb.measuredSteps = commanded;
            a.fb.positionErrorSteps = 0;
            a.fb.rawCounts = 0;
            a.fb.mode = MODE_OPEN_LOOP;
            return;
        }

        int64_t raw = a.encoder.getCount();
        a.fb.rawCounts = raw;
        if (a.cal.valid)
        {
            int32_t measured = a.originSteps + axisCountsToSteps(a.cal, raw);
            a.fb.measuredSteps = measured;
            a.fb.positionErrorSteps = measured - commanded;
        }
        else
        {
            // No scale yet: cannot express counts as steps. Report identity for
            // position but keep rawCounts visible for diagnostics.
            a.fb.measuredSteps = commanded;
            a.fb.positionErrorSteps = 0;
        }
        a.fb.calibrated = a.cal.valid;
    }

    // ------------------------------------------------------------------ mapping
    // Decide which logical axis index the wired encoder pins belong to, and
    // return the A/B pins + inversion for that axis. Returns false if the axis
    // has no encoder.
    static bool encoderPinsForAxis(int axis, int8_t &a, int8_t &b, bool &invert)
    {
#ifdef CAN_CONTROLLER_CANOPEN
        // On a CAN slave there is ONE physical motor+encoder, wired to the
        // ENC_X_* pins, representing the node's LOGICAL axis. Map the wired
        // pins to REMOTE_MOTOR_AXIS_ID instead of hardcoding X (WP2 req #2).
        if (axis == (int)pinConfig.REMOTE_MOTOR_AXIS_ID)
        {
            a = pinConfig.ENC_X_A;
            b = pinConfig.ENC_X_B;
            invert = !pinConfig.ENC_X_encoderDirection;
            return (a >= 0 && b >= 0);
        }
        return false;
#else
        // Standalone: X/Y/Z map to their own ENC_* pins.
        switch (axis)
        {
        case Stepper::X:
            a = pinConfig.ENC_X_A; b = pinConfig.ENC_X_B;
            invert = !pinConfig.ENC_X_encoderDirection;
            return (a >= 0 && b >= 0);
        case Stepper::Y:
            a = pinConfig.ENC_Y_A; b = pinConfig.ENC_Y_B;
            invert = !pinConfig.ENC_Y_encoderDirection;
            return (a >= 0 && b >= 0);
        case Stepper::Z:
            a = pinConfig.ENC_Z_A; b = pinConfig.ENC_Z_B;
            invert = !pinConfig.ENC_Z_encoderDirection;
            return (a >= 0 && b >= 0);
        default:
            return false;
        }
#endif
    }

    // ---------------------------------------------------- move-job / servo (WP6/7)

    // Adopt the encoder-derived position as truth: push it into FAS so the next
    // ABSOLUTE move computes from the corrected origin (design §3.2 — must call
    // setCurrentPosition, not just update a struct field), and re-anchor so the
    // reported position error is zero.
    static void adoptEncoderPosition(int axis)
    {
#ifdef MOTOR_CONTROLLER
        AxisState &a = g_axes[axis];
        if (!a.hasEnc || !a.cal.valid)
            return;
        int32_t measured = a.originSteps + axisCountsToSteps(a.cal, a.encoder.getCount());
        FocusMotor::setPosition(static_cast<Stepper>(axis), measured);
        a.originSteps = measured;
        a.encoder.zero();
#else
        (void)axis;
#endif
    }

    // Async DIVERGENCE notification (does NOT stop/latch — that's for the
    // watchdog faults). Sets health=DEGRADED at the call site.
    static void notifyDivergence(int axis)
    {
        AxisState &a = g_axes[axis];
        AxisNotify::Event e;
        e.axis = (uint8_t)axis;
        e.fault = (uint8_t)FAULT_DIVERGENCE;
        e.posErrSteps = a.fb.positionErrorSteps;
        e.commandedSteps = a.fb.commandedSteps;
        e.measuredSteps = a.fb.measuredSteps;
        AxisNotify::report(e);
    }

    // Start an open-loop absolute move, optionally with backlash feed-forward on
    // a direction reversal (CORRECT mode).
    static void startOpenLoopMove(int axis, int32_t absTarget, int32_t speed, bool applyBacklash)
    {
#ifdef MOTOR_CONTROLLER
        AxisState &a = g_axes[axis];
        int32_t cur = commandedStepsOf(axis);
        int8_t dir = (absTarget > cur) ? 1 : (absTarget < cur ? -1 : 0);
        bool reversal = applyBacklash && a.backlashSteps > 0 && dir != 0 &&
                        a.lastMoveDir != 0 && dir != a.lastMoveDir;
        int32_t olTarget = reversal ? absTarget + dir * a.backlashSteps : absTarget;
        a.reversalMove = reversal;
        if (dir != 0)
            a.lastMoveDir = dir;
        FocusMotor::moveMotor(olTarget, speed, axis, false /*absolute*/);
#else
        (void)axis; (void)absTarget; (void)speed; (void)applyBacklash;
#endif
    }

    // MONITOR/CORRECT state machine, ticked once per axis in loop().
    static void driveMoveJob(int axis)
    {
        AxisState &a = g_axes[axis];
        if (a.moveStage == MJ_IDLE)
            return;

        switch (a.moveStage)
        {
        case MJ_RUNNING:
            if (axisMoving(axis))
                return; // open-loop move still in flight
            if (a.fb.mode == MODE_MONITOR)
            {
                // MONITOR never corrects — only alerts on divergence.
                if (abs(a.fb.positionErrorSteps) > a.divergenceThresholdSteps)
                {
                    a.fb.health = HEALTH_DEGRADED;
                    a.fb.fault = FAULT_DIVERGENCE;
                    notifyDivergence(axis);
                }
                a.moveStage = MJ_IDLE;
            }
            else // CORRECT
            {
                a.moveStage = MJ_VERIFY;
            }
            break;

        case MJ_VERIFY:
        {
            int32_t err = a.targetSteps - a.fb.measuredSteps;
            int32_t tol = a.verifyTolSteps + (a.reversalMove ? a.backlashSteps : 0);
            if (abs(err) <= tol)
            {
                adoptEncoderPosition(axis); // encoder is truth after each move
                a.moveStage = MJ_IDLE;
                a.reversalMove = false;
            }
            else if (a.correctRetries == 0)
            {
                // Out of tolerance after all retries: degrade + alert, but still
                // adopt the measured truth so the next move starts corrected.
                a.fb.health = HEALTH_DEGRADED;
                a.fb.fault = FAULT_DIVERGENCE;
                notifyDivergence(axis);
                adoptEncoderPosition(axis);
                a.moveStage = MJ_IDLE;
                a.reversalMove = false;
            }
            else
            {
                a.correctRetries--;
                int32_t corr = err;
                if (corr > kMaxCorrectionSteps) corr = kMaxCorrectionSteps;
                if (corr < -kMaxCorrectionSteps) corr = -kMaxCorrectionSteps;
#ifdef MOTOR_CONTROLLER
                FocusMotor::moveMotor(corr, kCorrectionSpeed, axis, true /*relative*/);
#endif
                a.moveStage = MJ_CORRECTING;
            }
            break;
        }

        case MJ_CORRECTING:
            if (!axisMoving(axis))
                a.moveStage = MJ_VERIFY; // re-check after the corrective move
            break;

        default:
            break;
        }
    }

    // Begin a SERVO move. Falls back to CORRECT-style handling if the FAS live
    // set-speed path is unavailable (e.g. AccelStep builds).
    static void startServo(int axis, int32_t absTarget, int32_t speed)
    {
        AxisState &a = g_axes[axis];
        a.targetSteps = absTarget;
#ifdef AXIS_SERVO_AVAILABLE
        int32_t targetCounts = (int32_t)axisStepsToCounts(a.cal, absTarget - a.originSteps);
        a.pid.maxVelocity = (speed > 0) ? (float)speed : a.pid.maxVelocity;
        a.pid.setTarget(targetCounts, (int32_t)a.encoder.getCount());
        a.moveStage = MJ_IDLE;
        a.servoArrived = false;
        a.servoActive = true; // set last: the servo task starts driving now
#else
        a.correctRetries = kMaxCorrectionRetries;
        startOpenLoopMove(axis, absTarget, speed, true);
        a.moveStage = MJ_RUNNING;
#endif
    }

#ifdef AXIS_SERVO_AVAILABLE
    // Dedicated 1 kHz PID task (core 0) so CAN servicing on core 1 is never
    // blocked. Applies velocity via the mutex-free FAS set-speed path.
    static void servoTask(void *)
    {
        for (;;)
        {
            for (int axis = 0; axis < AXIS_MAX_STEPPERS; axis++)
            {
                AxisState &a = g_axes[axis];
                if (!a.servoActive)
                    continue;
                if (a.fb.health == HEALTH_FAULT)
                {
                    // Watchdog latched a fault — stop servoing immediately.
                    a.servoActive = false;
                    FAccelStep::setLiveSpeed(axis, 0);
                    continue;
                }
                int32_t cur = (int32_t)a.encoder.getCount();
                float v = a.pid.compute(cur);
                if (a.pid.isComplete())
                {
                    a.servoActive = false;
                    a.servoArrived = true; // loop() finalizes (stop + adopt)
                    FAccelStep::setLiveSpeed(axis, 0);
                    continue;
                }
                FAccelStep::setLiveSpeed(axis, (int32_t)v);
            }
            vTaskDelay(1); // ~1 kHz
        }
    }
#endif

    // Cancel any in-flight move job / servo for the axis.
    static void cancelMotion(int axis)
    {
        AxisState &a = g_axes[axis];
        a.moveStage = MJ_IDLE;
        a.servoActive = false;
        a.servoArrived = false;
        a.pid.stop();
#ifdef AXIS_SERVO_AVAILABLE
        FAccelStep::setLiveSpeed(axis, 0);
#endif
    }

    // --------------------------------------------------------------------- API
    void setup()
    {
        if (g_setupDone)
            return;

        for (int axis = 0; axis < MOTOR_AXIS_COUNT; axis++)
        {
            AxisState &a = g_axes[axis];
            a.fb = AxisFeedback();

            int8_t pinA = -1, pinB = -1;
            bool invert = false;
            // Only axes backed by a real FAS stepper can be feedback-controlled.
            if (axis < AXIS_MAX_STEPPERS && encoderPinsForAxis(axis, pinA, pinB, invert))
            {
                a.hasEnc = a.encoder.begin(pinA, pinB, invert,
                                           AXIS_ENC_PCNT_UNIT, AXIS_ENC_GLITCH_FILTER);
            }

            if (a.hasEnc)
            {
                // Load calibration, then apply the microstep-change rule.
                if (AxisCalibrationStore::load(axis, a.cal))
                {
                    uint16_t ms = currentMicrosteps();
                    if (AxisCalibrationStore::applyMicrostepRule(a.cal, ms))
                    {
                        // Rescaled from a different microstep setting: keep
                        // motion, but flag CAL_INVALID as a WARNING.
                        a.fb.fault = FAULT_CAL_INVALID;
                        a.fb.health = HEALTH_OK;
                        AxisCalibrationStore::save(axis, a.cal);
                    }
                }
                a.fb.calibrated = a.cal.valid;
                configureWatchdog(axis);
                reanchorOrigin(axis);
                AxisWatchdog::reset(a.wdState);
                // MONITOR is the safe default once an encoder is present and
                // calibrated; otherwise stay OPEN_LOOP until calibrated.
                a.fb.mode = a.cal.valid ? MODE_MONITOR : MODE_OPEN_LOOP;
                ESP_LOGI(TAG, "axis %d: encoder present (cal=%d, mode=%d)",
                         axis, a.cal.valid, a.fb.mode);
            }
            else
            {
                a.fb.mode = MODE_OPEN_LOOP;
            }
        }

#ifdef AXIS_SERVO_AVAILABLE
        // One PID task services all SERVO-mode axes; pinned to core 0 so the
        // 1 kHz loop never contends with CAN servicing on core 1.
        static TaskHandle_t s_servoTask = nullptr;
        if (s_servoTask == nullptr)
        {
            xTaskCreatePinnedToCore(servoTask, "axisServo", 4096, nullptr,
                                    configMAX_PRIORITIES - 3, &s_servoTask, 0);
        }
#endif
        g_setupDone = true;
    }

    void loop()
    {
        if (!g_setupDone)
            return;
        // Only FAS-backed axes have live positions; higher indices keep their
        // default identity feedback.
        int n = (MOTOR_AXIS_COUNT < AXIS_MAX_STEPPERS) ? MOTOR_AXIS_COUNT : AXIS_MAX_STEPPERS;
        uint32_t now = millis();
        for (int axis = 0; axis < n; axis++)
        {
            // Consume deferred requests (set from the CANopen task) on THIS task.
            AxisState &st = g_axes[axis];
            if (st.reqMode >= 0)
            {
                uint8_t m = (uint8_t)st.reqMode;
                st.reqMode = -1;
                setMode(axis, (AxisMode)m);
            }
            if (st.reqReset >= 0)
            {
                uint8_t p = (uint8_t)st.reqReset;
                st.reqReset = -1;
                resetAxis(axis, (AxisResetPolicy)p);
            }
            if (st.reqCalibrate)
            {
                st.reqCalibrate = false;
                calibrateAxis(axis); // blocking — fine on the main loop task
            }

            updateFeedback(axis);

            // Fast blocking/stall watchdog — protects EVERY mode, incl. open
            // loop. Skip if no encoder or already latched.
            AxisState &a = g_axes[axis];
            if (!a.hasEnc || a.fb.health == HEALTH_FAULT)
                continue;
            AxisWatchdog::Trip trip = AxisWatchdog::update(
                a.wdCfg, a.wdState, axisMoving(axis), commandedSpeedOf(axis),
                a.fb.rawCounts, a.fb.positionErrorSteps, now);
            if (trip == AxisWatchdog::TRIP_STALL)
            {
                cancelMotion(axis);
                raiseFault(axis, FAULT_STALL);
                continue;
            }
            else if (trip == AxisWatchdog::TRIP_LOST_STEPS)
            {
                cancelMotion(axis);
                raiseFault(axis, FAULT_LOST_STEPS);
                continue;
            }

            // Finalize a SERVO move the task flagged as arrived (stop + adopt on
            // the main task, off the servo hot loop).
            if (a.servoArrived)
            {
                a.servoArrived = false;
#ifdef MOTOR_CONTROLLER
                FocusMotor::stopStepper(axis);
#endif
                adoptEncoderPosition(axis);
            }

            // Drive the MONITOR/CORRECT move-job state machine.
            driveMoveJob(axis);
        }

        // Drain async fault notifications (EMCY on slave / JSON standalone).
        AxisNotify::loop();
    }

    void moveTo(int axis, int32_t targetSteps, int32_t speed, bool isAbsolute)
    {
        if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
            return;
        // Refuse motion while a fault is latched (except CAL_INVALID, a warning).
        if (g_axes[axis].fb.health == HEALTH_FAULT)
        {
            ESP_LOGW(TAG, "moveTo ax%d refused: fault %d latched (resetAxis first)",
                     axis, g_axes[axis].fb.fault);
            return;
        }
#ifdef MOTOR_CONTROLLER
        AxisState &a = g_axes[axis];
        int32_t absTarget = isAbsolute ? targetSteps : commandedStepsOf(axis) + targetSteps;

        // Feedback modes require a valid calibration; otherwise run pure open loop.
        uint8_t mode = a.fb.mode;
        if (!a.hasEnc || !a.cal.valid)
            mode = MODE_OPEN_LOOP;

        // A fresh move supersedes anything in flight.
        cancelMotion(axis);

        switch (mode)
        {
        case MODE_MONITOR:
            a.targetSteps = absTarget;
            startOpenLoopMove(axis, absTarget, speed, false /*no backlash FF*/);
            a.moveStage = MJ_RUNNING;
            break;
        case MODE_CORRECT:
            a.targetSteps = absTarget;
            a.correctRetries = kMaxCorrectionRetries;
            startOpenLoopMove(axis, absTarget, speed, true /*backlash FF*/);
            a.moveStage = MJ_RUNNING;
            break;
        case MODE_SERVO:
            startServo(axis, absTarget, speed);
            break;
        case MODE_OPEN_LOOP:
        default:
            FocusMotor::moveMotor(targetSteps, speed, axis, !isAbsolute);
            a.moveStage = MJ_IDLE;
            break;
        }
#else
        (void)targetSteps; (void)speed; (void)isAbsolute;
#endif
    }

    void stop(int axis)
    {
        if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
            return;
        if (axis < AXIS_MAX_STEPPERS)
            cancelMotion(axis);
#ifdef MOTOR_CONTROLLER
        FocusMotor::stopStepper(axis);
#endif
    }

    void setMode(int axis, AxisMode mode)
    {
        if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
            return;
        // Feedback modes require an encoder; fall back to OPEN_LOOP otherwise.
        if (mode != MODE_OPEN_LOOP && !g_axes[axis].hasEnc)
        {
            ESP_LOGW(TAG, "axis %d has no encoder; forcing OPEN_LOOP", axis);
            mode = MODE_OPEN_LOOP;
        }
        g_axes[axis].fb.mode = mode;
    }

    AxisFeedback getFeedback(int axis)
    {
        if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
            return AxisFeedback();
        return g_axes[axis].fb;
    }

    void resetAxis(int axis, AxisResetPolicy policy)
    {
        if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
            return;
        AxisState &a = g_axes[axis];

        // Abandon any in-flight move/servo and clear PID/stall state.
        if (axis < AXIS_MAX_STEPPERS)
            cancelMotion(axis);

        // Clear latched fault/health. Preserve a CAL_INVALID warning unless a
        // fresh calibration replaces it.
        a.fb.fault = a.cal.derived ? FAULT_CAL_INVALID : FAULT_NONE;
        a.fb.health = HEALTH_OK;
        AxisWatchdog::reset(a.wdState);

        if (!a.hasEnc)
            return;

        switch (policy)
        {
        case RESET_TRUST_ENCODER:
            // Adopt the encoder-derived position as truth: push it into FAS so
            // the next absolute move computes from the corrected origin.
            if (a.cal.valid)
            {
#ifdef MOTOR_CONTROLLER
                int32_t measured = a.originSteps + axisCountsToSteps(a.cal, a.encoder.getCount());
                FocusMotor::setPosition(static_cast<Stepper>(axis), measured);
                a.originSteps = measured;
                a.encoder.zero();
#endif
            }
            else
            {
                reanchorOrigin(axis);
            }
            break;

        case RESET_TRUST_STEPS:
            // Zero the encoder to match the step counter.
            reanchorOrigin(axis);
            break;

        case RESET_FORCE_REHOME:
            reanchorOrigin(axis);
            a.fb.referenced = false;
            break;
        }
        updateFeedback(axis);
    }

    // ------------------------------------------------------------- calibration
    bool calibrateAxis(int axis, const AxisCalibrationRoutine::Params &params)
    {
        if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
            return false;
        AxisState &a = g_axes[axis];
        if (!a.hasEnc)
        {
            ESP_LOGW(TAG, "calibrateAxis %d: no encoder", axis);
            a.fb.fault = FAULT_CAL_FAILED;
            return false;
        }

        // Origin: zero the encoder and record the common step origin.
        reanchorOrigin(axis);

        AxisCalibrationRoutine::Hooks hooks;
        hooks.getStepPos = [axis]() { return commandedStepsOf(axis); };
        hooks.moveRelBlocking = [axis](int32_t d, int32_t s) { return blockingRelMove(axis, d, s); };
        hooks.getRawCount = [axis]() { return g_axes[axis].encoder.getCount(); };
        hooks.aborted = [axis]() { return axisAborted(axis); };

        AxisCalibration result;
        AxisFault err = FAULT_NONE;
        bool ok = AxisCalibrationRoutine::run(hooks, params, result, err);
        if (ok)
        {
            a.cal = result;
            AxisCalibrationStore::save(axis, a.cal);
            a.fb.calibrated = true;
            a.fb.fault = FAULT_NONE;
            a.fb.health = HEALTH_OK;
            configureWatchdog(axis); // thresholds now follow measured scatter
            reanchorOrigin(axis); // fresh origin after all the probe moves
            AxisWatchdog::reset(a.wdState);
            if (a.fb.mode == MODE_OPEN_LOOP)
                a.fb.mode = MODE_MONITOR;
            ESP_LOGI(TAG, "calibrateAxis %d OK (q=%u scatter=%u)", axis,
                     a.cal.quality, a.cal.residualScatter);
        }
        else
        {
            a.fb.fault = err;
            ESP_LOGE(TAG, "calibrateAxis %d FAILED (fault=%d)", axis, err);
        }
        return ok;
    }

    bool calibrateAxis(int axis)
    {
        AxisCalibrationRoutine::Params p;
        p.currentMicrosteps = currentMicrosteps();
        return calibrateAxis(axis, p);
    }

    AxisCalibration getCalibration(int axis)
    {
        if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
            return AxisCalibration();
        return g_axes[axis].cal;
    }

    bool hasEncoder(int axis)
    {
        if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
            return false;
        return g_axes[axis].hasEnc;
    }

    void requestMode(int axis, uint8_t mode)
    {
        if (axis < 0 || axis >= MOTOR_AXIS_COUNT || mode > MODE_SERVO)
            return;
        g_axes[axis].reqMode = (int8_t)mode;
    }

    void requestReset(int axis, uint8_t policy)
    {
        if (axis < 0 || axis >= MOTOR_AXIS_COUNT || policy > RESET_FORCE_REHOME)
            return;
        g_axes[axis].reqReset = (int8_t)policy;
    }

    void requestCalibration(int axis)
    {
        if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
            return;
        g_axes[axis].reqCalibrate = true;
    }
}
