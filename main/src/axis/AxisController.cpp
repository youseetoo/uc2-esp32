#include "AxisController.h"
#include "EncoderBackend.h"
#include <PinConfig.h>
#include "esp_log.h"
#include "../motor/MotorTypes.h" // Stepper enum, MotorData (header-only)

#ifdef MOTOR_CONTROLLER
#include "../motor/FocusMotor.h"
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
    struct AxisState
    {
        EncoderBackend  encoder;
        AxisCalibration cal;
        AxisFeedback    fb;
        bool            hasEnc = false;
        int32_t         originSteps = 0;  // step position at the encoder origin
        // originCounts is always 0 (we zero the hardware counter at the origin).
    };

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
                reanchorOrigin(axis);
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
        g_setupDone = true;
    }

    void loop()
    {
        if (!g_setupDone)
            return;
        // Only FAS-backed axes have live positions; higher indices keep their
        // default identity feedback.
        int n = (MOTOR_AXIS_COUNT < AXIS_MAX_STEPPERS) ? MOTOR_AXIS_COUNT : AXIS_MAX_STEPPERS;
        for (int axis = 0; axis < n; axis++)
            updateFeedback(axis);
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
        // WP1–WP3: always open-loop. MONITOR watches via loop(); CORRECT/SERVO
        // land in WP6/WP7.
        FocusMotor::moveMotor(targetSteps, speed, axis, !isAbsolute);
#else
        (void)targetSteps; (void)speed; (void)isAbsolute;
#endif
    }

    void stop(int axis)
    {
        if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
            return;
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

        // Clear latched fault/health. Preserve a CAL_INVALID warning unless a
        // fresh calibration replaces it.
        a.fb.fault = a.cal.derived ? FAULT_CAL_INVALID : FAULT_NONE;
        a.fb.health = HEALTH_OK;

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
            reanchorOrigin(axis); // fresh origin after all the probe moves
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
}
