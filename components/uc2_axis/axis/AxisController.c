// T4.1: pure C port of the C++ AxisController (design v2, WP1–7).
// The C++ namespace AxisController{...} became the axis_* API in
// AxisController.h; the C++ FocusMotor / FAccelStep / TMCController
// dependencies go through the C shims in uc2_motor (focusmotor_qid_c.h,
// axis_motor_c.h).
static const char *TAG = "AxisCtrl";
#include "AxisController.h"
#include "EncoderBackend.h"
#include "AxisWatchdog.h"
#include "AxisNotify.h"
#include "AxisPID.h"
#include "EncoderMonitor.h"
#include "PinConfigDefault.h" // pinConfig (pure-C board table)
#include "focusmotor_qid_c.h" // C shim: per-axis MotorData field access
#include "axis_motor_c.h"     // C shim: isRunning / setPosition / aborted /
                              // setLiveSpeed / getMicrosteps
#include <string.h>
#include <stdlib.h> // abs
#include "sdkconfig.h"
#include "esp_log.h"
#include "esp_task_wdt.h"
#include "cJSON.h"
#include "uc2_serial_out.h" // serial_out_safe_send_json_string (C API)
#include "uc2_hal.h"         // delay()/millis()/log

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
#define AXIS_MAX_STEPPERS 4

#if defined(CONFIG_UC2_MOTOR_CONTROLLER) && defined(CONFIG_UC2_USE_FASTACCEL)
#define AXIS_SERVO_AVAILABLE 1
#else
#define AXIS_SERVO_AVAILABLE 0
#endif

// Move-job stage for MONITOR/CORRECT (SERVO uses the servo task).
typedef enum MoveStage {
    MJ_IDLE = 0,
    MJ_RUNNING,    // open-loop move in flight
    MJ_VERIFY,     // move done; check error
    MJ_CORRECTING  // corrective move in flight
} MoveStage;

typedef struct AxisState {
    EncoderBackend      encoder;
    AxisCalibration     cal;
    AxisFeedback        fb;
    AxisWatchdogConfig  wdCfg;
    AxisWatchdogState   wdState;
    bool                hasEnc;
    int32_t             originSteps; // step position at the encoder origin
    // originCounts is always 0 (we zero the hardware counter at the origin).
    // Deferred requests set from another task (CANopen), consumed in loop().
    volatile int8_t     reqMode;     // -1 = none
    volatile int8_t     reqReset;    // -1 = none, else AxisResetPolicy
    volatile bool       reqCalibrate;
    volatile bool       reqEncTable;
    volatile int32_t    reqEncTablePoints;
    volatile int32_t    reqEncTableStep;
    volatile int32_t    reqEncTableSpeed;

    // Mode actually driving the CURRENT move. Differs from fb.mode when a
    // per-move {"closedloop":N} override is in play. The watchdog keys off
    // THIS, so an explicitly open-loop move is never stopped by feedback.
    uint8_t  activeMode;

    // ---- move job (MONITOR/CORRECT), driven in loop() ----
    uint8_t  moveStage;
    int32_t  targetSteps;     // absolute target of the current job
    int32_t  moveSpeed;
    uint8_t  correctRetries;
    int8_t   lastMoveDir;     // last commanded direction (backlash)
    bool     reversalMove;    // widen verify tolerance for this move

    // ---- derived thresholds (from calibration scatter/backlash) ----
    int32_t  verifyTolSteps;
    int32_t  divergenceThresholdSteps;
    int32_t  backlashSteps;

    // ---- SERVO (driven by the servo task) ----
    AxisPID  pid;
    volatile bool servoActive;  // servo task should drive this axis
    volatile bool servoArrived; // task -> loop: reached target, finalize
} AxisState;

// Correction bounds (design v2, WP6).
#define kMaxCorrectionSteps   5000  // clamp per corrective move
#define kMaxCorrectionRetries 2
#define kCorrectionSpeed      2000  // steps/s for corrections

static AxisState g_axes[MOTOR_AXIS_COUNT]; // zero-initialized (file scope)
static bool g_setupDone = false;

// ------------------------------------------------------------------ helpers
static uint16_t currentMicrosteps(void)
{
#if CONFIG_UC2_TMC_CONTROLLER
    uint16_t ms = axis_motor_get_microsteps();
    if (ms == 0)
        ms = (uint16_t)pinConfig.tmc_microsteps;
    return ms;
#else
    return (uint16_t)pinConfig.tmc_microsteps;
#endif
}

static int32_t commandedStepsOf(int axis)
{
#if CONFIG_UC2_MOTOR_CONTROLLER
    if (axis < 0 || axis >= AXIS_MAX_STEPPERS)
        return 0; // no FAS stepper backs this index — avoid OOB access
    return focusmotor_get_current_position(axis);
#else
    (void)axis;
    return 0;
#endif
}

static bool axisMoving(int axis)
{
#if CONFIG_UC2_MOTOR_CONTROLLER
    if (axis < 0 || axis >= AXIS_MAX_STEPPERS)
        return false;
    return axis_motor_is_running(axis);
#else
    (void)axis;
    return false;
#endif
}

// |commanded speed| in steps/s for the axis (gates the watchdog).
static int32_t commandedSpeedOf(int axis)
{
#if CONFIG_UC2_MOTOR_CONTROLLER
    if (axis < 0 || axis >= AXIS_MAX_STEPPERS)
        return 0;
    return abs(focusmotor_get_speed(axis));
#else
    (void)axis;
    return 0;
#endif
}

// Derive watchdog thresholds from the measured encoder noise. Called after a
// calibration is loaded/produced so tolerances follow real scatter.
static void configureWatchdog(int axis)
{
    AxisState *a = &g_axes[axis];
    AxisWatchdogConfig *c = &a->wdCfg;
    // "Progress" must clear a few sigma of quantisation noise.
    uint16_t scatter = a->cal.valid ? a->cal.residualScatter : 3;
    if (scatter < 1)
        scatter = 1;
    c->noiseThresholdCounts = (uint16_t)(scatter * 3);
    if (c->noiseThresholdCounts < 2)
        c->noiseThresholdCounts = 2;
    // Lag limit: a generous multiple of scatter expressed in steps, so a
    // genuine lost-step burst trips but calibrated backlash does not.
    if (a->cal.valid)
    {
        int32_t scatterSteps = abs(axisCountsToSteps(&a->cal, (int64_t)scatter * 8));
        a->backlashSteps = abs(axisCountsToSteps(&a->cal, a->cal.backlashCounts));
        c->lagLimitSteps = scatterSteps + a->backlashSteps + 20;

        // Verify tolerance (CORRECT) and divergence threshold (MONITOR) also
        // follow measured noise: a couple of sigma for verify, more for a
        // divergence alert.
        int32_t sigmaSteps = abs(axisCountsToSteps(&a->cal, scatter));
        a->verifyTolSteps = sigmaSteps * 2 + 2;
        a->divergenceThresholdSteps = sigmaSteps * 4 + a->backlashSteps + 4;
    }
    c->enabled = a->hasEnc;
}

// Re-zero the encoder and pin the current step position as the origin.
static void reanchorOrigin(int axis)
{
    AxisState *a = &g_axes[axis];
    if (!a->hasEnc)
        return;
    encoder_backend_zero(&a->encoder);
    a->originSteps = commandedStepsOf(axis);
}

// Is the axis being forced to abort (endstop / e-stop)? Used by calibration.
static bool axisAborted(int axis)
{
#if CONFIG_UC2_MOTOR_CONTROLLER
    if (axis < 0 || axis >= AXIS_MAX_STEPPERS)
        return false;
    return axis_motor_aborted(axis);
#else
    (void)axis;
    return false;
#endif
}

// Blocking relative move used only by the on-demand calibration routine.
// Defined below (move-job section); used by the calibration probe moves.
static void issueMove(int axis, int32_t target, int32_t speed, bool isRelative,
                      int32_t accel);

// Acceleration for calibration probe moves. Explicit so the probes aren't
// stuck with whatever a previous move left behind (FocusMotor::moveMotor
// used to hardcode 1000, making each 500-step probe take ~1.4 s).
#define kCalProbeAccel 40000

// Delay that keeps the task watchdog fed. Calibration blocks the main loop
// task (which owns the TWDT subscription and normally feeds it once per
// iteration) for tens of seconds, so every wait inside the routine MUST
// feed the WDT or the node panics with "task_wdt: - loop (CPU 1)".
// esp_task_wdt_reset() is harmless (ESP_ERR_NOT_FOUND) on unsubscribed tasks.
static void wdtDelay(uint32_t ms)
{
    esp_task_wdt_reset();
    delay(ms);
}

static bool blockingRelMove(int axis, int32_t deltaSteps, int32_t speed)
{
#if CONFIG_UC2_MOTOR_CONTROLLER
    if (axisAborted(axis))
        return false;
    issueMove(axis, deltaSteps, abs(speed), true /*relative*/, kCalProbeAccel);

    const uint32_t startTimeoutMs = 300;
    const uint32_t moveTimeoutMs = 15000;
    uint32_t t0 = millis();
    // Wait for the motor to actually start.
    while (!axis_motor_is_running(axis) && (millis() - t0) < startTimeoutMs)
    {
        if (axisAborted(axis))
            return false;
        wdtDelay(1);
    }
    // Wait for completion.
    while (axis_motor_is_running(axis))
    {
        if (axisAborted(axis))
        {
            focusmotor_stop_stepper(axis);
            return false;
        }
        if ((millis() - t0) > moveTimeoutMs)
        {
            focusmotor_stop_stepper(axis);
            log_e("blockingRelMove ax%d timed out", axis);
            return false;
        }
        wdtDelay(2);
    }
    esp_task_wdt_reset(); // settle period follows in the routine
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
    AxisState *a = &g_axes[axis];
    a->fb.fault = fault;
    a->fb.health = HEALTH_FAULT;
#if CONFIG_UC2_MOTOR_CONTROLLER
    focusmotor_stop_stepper(axis);
#endif
    AxisEvent e;
    e.axis = (uint8_t)axis;
    e.fault = (uint8_t)fault;
    e.posErrSteps = a->fb.positionErrorSteps;
    e.commandedSteps = a->fb.commandedSteps;
    e.measuredSteps = a->fb.measuredSteps;
    axis_notify_report(&e);
    log_w("axis %d FAULT %d (posErr=%d) — motion latched", axis, fault,
          a->fb.positionErrorSteps);
}

// Recompute one axis's feedback from the encoder (or identity if none).
static void updateFeedback(int axis)
{
    /*
    This function is called from the main loop task, so it can safely call
    the FocusMotor shim to get the commanded position. It also reads the
    encoder count and applies the calibration to compute the measured
    position and position error. If the axis has no encoder, it sets the
    measured position equal to the commanded position and the position error
    to zero. It also updates the raw encoder count and the mode in the
    feedback struct.
    */
    AxisState *a = &g_axes[axis];
    int32_t commanded = commandedStepsOf(axis);
    a->fb.commandedSteps = commanded;

    if (!a->hasEnc)
    {
        // Identity feedback so downstream code never branches.
        a->fb.measuredSteps = commanded;
        a->fb.positionErrorSteps = 0;
        a->fb.rawCounts = 0;
        a->fb.mode = MODE_OPEN_LOOP;
        return;
    }

    int64_t raw = encoder_backend_get_count(&a->encoder);
    a->fb.rawCounts = raw;
    if (a->cal.valid)
    {
        int32_t measured = a->originSteps + axisCountsToSteps(&a->cal, raw);
        a->fb.measuredSteps = measured;
        a->fb.positionErrorSteps = measured - commanded; // both in step units
        // Throttled: this used to log EVERY loop (~30 ms), which floods the
        // shared UART and interleaves into (i.e. corrupts) the JSON replies.
        static uint32_t s_lastFbLogMs[MOTOR_AXIS_COUNT];
        static int32_t  s_lastFbErr[MOTOR_AXIS_COUNT];
        uint32_t nowMs = millis();
        if (a->fb.positionErrorSteps != s_lastFbErr[axis] &&
            (nowMs - s_lastFbLogMs[axis]) >= 500)
        {
            s_lastFbLogMs[axis] = nowMs;
            s_lastFbErr[axis] = a->fb.positionErrorSteps;
            log_i("axis %d: commanded=%d measured=%d posErr=%d rawCounts=%lld",
                  axis, commanded, measured, a->fb.positionErrorSteps, raw);
        }
    }
    else
    {
        // No scale yet: cannot express counts as steps. Report identity for
        // position but keep rawCounts visible for diagnostics.
        a->fb.measuredSteps = commanded;
        a->fb.positionErrorSteps = 0;
    }
    a->fb.calibrated = a->cal.valid;
}

// ------------------------------------------------------------------ mapping
// Decide which logical axis index the wired encoder pins belong to, and
// return the A/B pins + inversion for that axis. Returns false if the axis
// has no encoder. (Standalone Stepper::X/Y/Z == 0/1/2.)
static bool encoderPinsForAxis(int axis, int8_t *aPin, int8_t *bPin, bool *invert)
{
#if CONFIG_UC2_CAN_CONTROLLER_CANOPEN
    // On a CAN slave there is ONE physical motor+encoder, wired to the
    // ENC_X_* pins, representing the node's LOGICAL axis. Map the wired
    // pins to REMOTE_MOTOR_AXIS_ID instead of hardcoding X (WP2 req #2).
    if (axis == (int)pinConfig.REMOTE_MOTOR_AXIS_ID)
    {
        log_i("axis %d: encoder pins A=%d B=%d invert=%d", axis, pinConfig.ENC_X_A, pinConfig.ENC_X_B, !pinConfig.ENC_X_encoderDirection);
        *aPin = pinConfig.ENC_X_A;
        *bPin = pinConfig.ENC_X_B;
        *invert = !pinConfig.ENC_X_encoderDirection;
        return (*aPin >= 0 && *bPin >= 0);
    }
    return false;
#else
    // Standalone: X/Y/Z map to their own ENC_* pins.
    switch (axis)
    {
    case 0: // Stepper::X
        *aPin = pinConfig.ENC_X_A; *bPin = pinConfig.ENC_X_B;
        *invert = !pinConfig.ENC_X_encoderDirection;
        return (*aPin >= 0 && *bPin >= 0);
    case 1: // Stepper::Y
        *aPin = pinConfig.ENC_Y_A; *bPin = pinConfig.ENC_Y_B;
        *invert = !pinConfig.ENC_Y_encoderDirection;
        return (*aPin >= 0 && *bPin >= 0);
    case 2: // Stepper::Z
        *aPin = pinConfig.ENC_Z_A; *bPin = pinConfig.ENC_Z_B;
        *invert = !pinConfig.ENC_Z_encoderDirection;
        return (*aPin >= 0 && *bPin >= 0);
    default:
        return false;
    }
#endif
}

// ---------------------------------------------------- move-job / servo (WP6/7)

// Adopt the encoder-derived position as truth: push it into FAS so the next
// ABSOLUTE move computes from the corrected origin (design §3.2 — must call
// setPosition, not just update a struct field), and re-anchor so the
// reported position error is zero.
static void adoptEncoderPosition(int axis)
{
#if CONFIG_UC2_MOTOR_CONTROLLER
    AxisState *a = &g_axes[axis];
    if (!a->hasEnc || !a->cal.valid)
        return;
    int32_t measured = a->originSteps + axisCountsToSteps(&a->cal, encoder_backend_get_count(&a->encoder));
    axis_motor_set_position(axis, measured);
    a->originSteps = measured;
    encoder_backend_zero(&a->encoder);
#else
    (void)axis;
#endif
}

// Async DIVERGENCE notification (does NOT stop/latch — that's for the
// watchdog faults). Sets health=DEGRADED at the call site.
static void notifyDivergence(int axis)
{
    AxisState *a = &g_axes[axis];
    AxisEvent e;
    e.axis = (uint8_t)axis;
    e.fault = (uint8_t)FAULT_DIVERGENCE;
    e.posErrSteps = a->fb.positionErrorSteps;
    e.commandedSteps = a->fb.commandedSteps;
    e.measuredSteps = a->fb.measuredSteps;
    axis_notify_report(&e);
}

// Issue a stepper move like FocusMotor::moveMotor, but WITHOUT clobbering the
// caller-configured acceleration (moveMotor hardcodes 1000). Preserves the
// qid set by the dispatch layer so completion reporting still works.
static void issueMove(int axis, int32_t target, int32_t speed, bool isRelative,
                      int32_t accel)
{
#if CONFIG_UC2_MOTOR_CONTROLLER
    if (axis < 0 || axis >= AXIS_MAX_STEPPERS)
        return;
    focusmotor_set_target_position(axis, target);
    focusmotor_set_isforever(axis, 0);
    focusmotor_set_absolute(axis, isRelative ? 0 : 1);
    focusmotor_set_is_stop(axis, 0);
    focusmotor_set_stopped(axis, 0);
    focusmotor_set_speed(axis, speed);
    if (accel > 0)
        focusmotor_set_acceleration(axis, accel);
    else if (focusmotor_get_acceleration(axis) <= 0)
        focusmotor_set_acceleration(axis, 40000);
    focusmotor_start_stepper(axis, 0);
#else
    (void)axis; (void)target; (void)speed; (void)isRelative; (void)accel;
#endif
}

// Start an open-loop absolute move, optionally with backlash feed-forward on
// a direction reversal (CORRECT mode).
static void startOpenLoopMove(int axis, int32_t absTarget, int32_t speed, bool applyBacklash)
{
#if CONFIG_UC2_MOTOR_CONTROLLER
    AxisState *a = &g_axes[axis];
    int32_t cur = commandedStepsOf(axis);
    int8_t dir = (absTarget > cur) ? 1 : (absTarget < cur ? -1 : 0);
    bool reversal = applyBacklash && a->backlashSteps > 0 && dir != 0 &&
                    a->lastMoveDir != 0 && dir != a->lastMoveDir;
    int32_t olTarget = reversal ? absTarget + dir * a->backlashSteps : absTarget;
    a->reversalMove = reversal;
    if (dir != 0)
        a->lastMoveDir = dir;
    issueMove(axis, olTarget, speed, false /*absolute*/, 0 /*keep accel*/);
#else
    (void)axis; (void)absTarget; (void)speed; (void)applyBacklash;
#endif
}

// MONITOR/CORRECT state machine, ticked once per axis in loop().
static void driveMoveJob(int axis)
{
    AxisState *a = &g_axes[axis];
    if (a->moveStage == MJ_IDLE)
        return;

    switch (a->moveStage)
    {
    case MJ_RUNNING:
        if (axisMoving(axis))
            return; // open-loop move still in flight
        if (a->fb.mode == MODE_MONITOR)
        {
            // MONITOR never corrects — only alerts on divergence.
            if (abs(a->fb.positionErrorSteps) > a->divergenceThresholdSteps)
            {
                a->fb.health = HEALTH_DEGRADED;
                a->fb.fault = FAULT_DIVERGENCE;
                notifyDivergence(axis);
            }
            a->moveStage = MJ_IDLE;
        }
        else // CORRECT
        {
            a->moveStage = MJ_VERIFY;
        }
        break;

    case MJ_VERIFY:
    {
        int32_t err = a->targetSteps - a->fb.measuredSteps;
        int32_t tol = a->verifyTolSteps + (a->reversalMove ? a->backlashSteps : 0);
        if (abs(err) <= tol)
        {
            adoptEncoderPosition(axis); // encoder is truth after each move
            a->moveStage = MJ_IDLE;
            a->reversalMove = false;
        }
        else if (a->correctRetries == 0)
        {
            // Out of tolerance after all retries: degrade + alert, but still
            // adopt the measured truth so the next move starts corrected.
            a->fb.health = HEALTH_DEGRADED;
            a->fb.fault = FAULT_DIVERGENCE;
            notifyDivergence(axis);
            adoptEncoderPosition(axis);
            a->moveStage = MJ_IDLE;
            a->reversalMove = false;
        }
        else
        {
            a->correctRetries--;
            int32_t corr = err;
            if (corr > kMaxCorrectionSteps) corr = kMaxCorrectionSteps;
            if (corr < -kMaxCorrectionSteps) corr = -kMaxCorrectionSteps;
            issueMove(axis, corr, kCorrectionSpeed, true /*relative*/, 0 /*keep accel*/);
            a->moveStage = MJ_CORRECTING;
        }
        break;
    }

    case MJ_CORRECTING:
        if (!axisMoving(axis))
            a->moveStage = MJ_VERIFY; // re-check after the corrective move
        break;

    default:
        break;
    }
}

// Begin a SERVO move. Falls back to CORRECT-style handling if the FAS live
// set-speed path is unavailable (e.g. AccelStep builds).
static void startServo(int axis, int32_t absTarget, int32_t speed)
{
    AxisState *a = &g_axes[axis];
    a->targetSteps = absTarget;
#if AXIS_SERVO_AVAILABLE
    int32_t targetCounts = (int32_t)axisStepsToCounts(&a->cal, absTarget - a->originSteps);
    a->pid.maxVelocity = (speed > 0) ? (float)speed : a->pid.maxVelocity;

    // Arrival tolerance must live ABOVE the measured encoder noise floor —
    // a tolerance tighter than residualScatter can never be satisfied, so
    // the servo would hunt forever and never report "arrived".
    uint16_t scatter = a->cal.valid ? a->cal.residualScatter : 3;
    if (scatter < 1)
        scatter = 1;
    a->pid.positionTolerance = (int32_t)scatter;
    if (a->pid.positionTolerance < 2)
        a->pid.positionTolerance = 2;
    // FAR -> NEAR switch a good margin outside the noise band.
    a->pid.nearThreshold = a->pid.positionTolerance * 8;

    axis_pid_set_target(&a->pid, targetCounts, (int32_t)encoder_backend_get_count(&a->encoder));
    a->moveStage = MJ_IDLE;
    a->servoArrived = false;
#if CONFIG_UC2_MOTOR_CONTROLLER
    // Reflect the servo's commanded speed into MotorData so the watchdog's
    // minimum-speed gate stays armed while the servo drives the axis.
    focusmotor_set_speed(axis, (speed > 0) ? speed : (int32_t)a->pid.maxVelocity);
    focusmotor_set_isforever(axis, 1); // continuous-mode; servo owns the velocity
    focusmotor_set_stopped(axis, 0);
#endif
    a->servoActive = true; // set last: the servo task starts driving now
#else
    a->correctRetries = kMaxCorrectionRetries;
    startOpenLoopMove(axis, absTarget, speed, true);
    a->moveStage = MJ_RUNNING;
#endif
}

#if AXIS_SERVO_AVAILABLE
// Dedicated 1 kHz PID task (core 0) so CAN servicing on core 1 is never
// blocked. Applies velocity via the mutex-free FAS set-speed path.
static void servoTask(void *arg)
{
    (void)arg;
    log_i("Axis servo task started on core %d", xPortGetCoreID());
    for (;;)
    {
        for (int axis = 0; axis < AXIS_MAX_STEPPERS; axis++)
        {
            AxisState *a = &g_axes[axis];
            if (!a->servoActive)
                continue;
            if (a->fb.health == HEALTH_FAULT)
            {
                // Watchdog latched a fault — stop servoing immediately.
                a->servoActive = false;
                axis_motor_set_live_speed(axis, 0);
                continue;
            }
            int32_t cur = (int32_t)encoder_backend_get_count(&a->encoder);
            float v = axis_pid_compute(&a->pid, cur, 0.0f);
            if (axis_pid_is_complete(&a->pid))
            {
                a->servoActive = false;
                a->servoArrived = true; // loop() finalizes (stop + adopt)
                axis_motor_set_live_speed(axis, 0);
                continue;
            }
            // CRITICAL: the PID error lives in COUNT space, the velocity
            // command in STEP space. When countSign is -1 (+steps produce
            // -counts) the raw command drives the axis AWAY from the target
            // — positive feedback, i.e. a full-speed runaway that only the
            // watchdog stops. Map count-space -> step-space with countSign.
            int32_t cmd = (int32_t)v * (int32_t)a->cal.countSign;
            axis_motor_set_live_speed(axis, cmd);
        }
        vTaskDelay(1); // ~1 kHz
    }
}
#endif

// Cancel any in-flight move job / servo for the axis.
static void cancelMotion(int axis)
{
    AxisState *a = &g_axes[axis];
    a->moveStage = MJ_IDLE;
    a->servoActive = false;
    a->servoArrived = false;
    axis_pid_stop(&a->pid);
#if AXIS_SERVO_AVAILABLE
    axis_motor_set_live_speed(axis, 0);
#endif
}

// --------------------------------------------------------------------- API
void axis_controller_setup(void)
{
    if (g_setupDone)
        return;

    for (int axis = 0; axis < MOTOR_AXIS_COUNT; axis++)
    {
        AxisState *a = &g_axes[axis];
        memset(&a->fb, 0, sizeof(a->fb)); // AxisFeedback()
        encoder_backend_init(&a->encoder); // present=false
        axis_pid_init(&a->pid);            // legacy default gains/limits
        a->reqMode = -1;
        a->reqReset = -1;
        a->activeMode = MODE_OPEN_LOOP;

        int8_t pinA = -1, pinB = -1;
        bool invert = false;
        // Only axes backed by a real FAS stepper can be feedback-controlled.
        if (axis < AXIS_MAX_STEPPERS && encoderPinsForAxis(axis, &pinA, &pinB, &invert))
        {
            log_i("axis %d: encoder pins A=%d B=%d invert=%d", axis, pinA, pinB, invert);
            a->hasEnc = encoder_backend_begin(&a->encoder, pinA, pinB, invert,
                                              AXIS_ENC_PCNT_UNIT, AXIS_ENC_GLITCH_FILTER);
        }

        if (a->hasEnc)
        {
            log_w("axis %d: encoder present, loading calibration", axis);
            // Load calibration, then apply the microstep-change rule.
            if (axis_cal_store_load(axis, &a->cal))
            {
                log_i("axis %d: loaded calibration (valid=%d, scatter=%d, backlash=%d)",
                      axis, a->cal.valid, a->cal.residualScatter, a->cal.backlashCounts);
                uint16_t ms = currentMicrosteps();
                if (axis_cal_store_apply_microstep_rule(&a->cal, ms))
                {
                    // Rescaled from a different microstep setting: keep
                    // motion, but flag CAL_INVALID as a WARNING.
                    a->fb.fault = FAULT_CAL_INVALID;
                    a->fb.health = HEALTH_OK;
                    axis_cal_store_save(axis, &a->cal);
                    log_i("axis %d: rescaled calibration to %d microsteps (scatter=%d, backlash=%d)",
                          axis, ms, a->cal.residualScatter, a->cal.backlashCounts);
                }
            }
            a->fb.calibrated = a->cal.valid;
            configureWatchdog(axis);
            reanchorOrigin(axis);
            axis_wd_reset(&a->wdState);
            // MONITOR is the safe default once an encoder is present and
            // calibrated; otherwise stay OPEN_LOOP until calibrated.
            a->fb.mode = a->cal.valid ? MODE_MONITOR : MODE_OPEN_LOOP;
            log_i("axis %d: encoder present (cal=%d, mode=%d)",
                  axis, a->cal.valid, a->fb.mode);
        }
        else
        {
            a->fb.mode = MODE_OPEN_LOOP;
        }
    }

#if AXIS_SERVO_AVAILABLE
    // One PID task services all SERVO-mode axes; pinned to core 0 so the
    // 1 kHz loop never contends with CAN servicing on core 1.
    static TaskHandle_t s_servoTask = NULL;
    if (s_servoTask == NULL)
    {
        xTaskCreatePinnedToCore((TaskFunction_t)servoTask, "axisServo", 4096, NULL,
                                configMAX_PRIORITIES - 3, &s_servoTask, 0);
    }
#endif
    g_setupDone = true;
}

void axis_controller_loop(void)
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
        AxisState *st = &g_axes[axis];
        if (st->reqMode >= 0)
        {
            uint8_t m = (uint8_t)st->reqMode;
            st->reqMode = -1;
            log_i("axis %d: deferred setMode(%d)", axis, m);
            axis_set_mode(axis, m);
        }
        if (st->reqReset >= 0)
        {
            uint8_t p = (uint8_t)st->reqReset;
            st->reqReset = -1;
            log_i("axis %d: deferred resetAxis(%d)", axis, p);
            axis_reset(axis, p);
        }
        if (st->reqEncTable)
        {
            st->reqEncTable = false;
            int pts = (int)st->reqEncTablePoints;
            int32_t sz = st->reqEncTableStep;
            int32_t sp = st->reqEncTableSpeed;
            log_i("axis %d: deferred encoderTable()", axis);
            axis_encoder_table(axis, pts, sz, sp); // blocking, WDT-fed
        }
        if (st->reqCalibrate)
        {
            st->reqCalibrate = false;
            log_i("axis %d: deferred calibrateAxis()", axis);
            axis_calibrate_defaults(axis); // blocking — fine on the main loop task
        }
        // Update the feedback struct from the encoder (or identity if none).
        updateFeedback(axis);

        // Periodic encoder liveness monitor (reuses this iteration's count
        // sample — no extra PCNT access). No-op unless enabled via
        // {"encmonitor":<ms>} / encoder_monitor_set_period.
        AxisState *a = &g_axes[axis];
        if (a->hasEnc)
            encoder_monitor_tick(axis, a->fb.rawCounts, now);

        // Fast blocking/stall watchdog. Skipped when the axis (or this
        // move, via {"closedloop":0}) is OPEN_LOOP: open loop means the
        // host owns responsibility for the move, so firmware must never
        // stop or latch on encoder feedback there. Also skipped without an
        // encoder or while a fault is already latched.
        if (!a->hasEnc || a->fb.health == HEALTH_FAULT ||
            a->activeMode == MODE_OPEN_LOOP)
            continue;
        AxisWatchdogTrip trip = axis_wd_update(
            &a->wdCfg, &a->wdState, axisMoving(axis), commandedSpeedOf(axis),
            a->fb.rawCounts, a->fb.positionErrorSteps, now);
        if (trip == AW_TRIP_STALL)
        {
            cancelMotion(axis);
            raiseFault(axis, FAULT_STALL);
            continue;
        }
        else if (trip == AW_TRIP_LOST_STEPS)
        {
            cancelMotion(axis);
            raiseFault(axis, FAULT_LOST_STEPS);
            continue;
        }

        // Finalize a SERVO move the task flagged as arrived (stop + adopt on
        // the main task, off the servo hot loop).
        if (a->servoArrived)
        {
            a->servoArrived = false;
#if CONFIG_UC2_MOTOR_CONTROLLER
            focusmotor_stop_stepper(axis);
#endif
            adoptEncoderPosition(axis);
        }

        // Drive the MONITOR/CORRECT move-job state machine.
        driveMoveJob(axis);
    }

    // Drain async fault notifications (EMCY on slave / JSON standalone).
    axis_notify_loop();
}

void axis_move_to(int axis, int32_t targetSteps, int32_t speed, bool isAbsolute)
{
    // 0xFF = "use the axis's configured mode".
    axis_move_to_with_mode(axis, targetSteps, speed, isAbsolute, 0xFF);
}

void axis_move_to_with_mode(int axis, int32_t targetSteps, int32_t speed, bool isAbsolute,
                            uint8_t modeOverride)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
        return;
    // Refuse motion while a fault is latched (except CAL_INVALID, a warning).
    if (g_axes[axis].fb.health == HEALTH_FAULT)
    {
        log_w("moveTo ax%d refused: fault %d latched (resetAxis first)",
              axis, g_axes[axis].fb.fault);
        return;
    }
#if CONFIG_UC2_MOTOR_CONTROLLER
    AxisState *a = &g_axes[axis];
    int32_t absTarget = isAbsolute ? targetSteps : commandedStepsOf(axis) + targetSteps;

    // Per-move override wins over the axis's configured mode; it does NOT
    // change a->fb.mode, so the next plain move reverts to the axis default.
    uint8_t mode = (modeOverride <= MODE_SERVO) ? modeOverride : a->fb.mode;
    // Feedback modes require a valid calibration; otherwise run pure open loop.
    if (!a->hasEnc || !a->cal.valid)
        mode = MODE_OPEN_LOOP;
    a->activeMode = mode; // gates the watchdog for the duration of this move

    // A fresh move supersedes anything in flight.
    cancelMotion(axis);

    switch (mode)
    {
    case MODE_MONITOR:
        a->targetSteps = absTarget;
        startOpenLoopMove(axis, absTarget, speed, false /*no backlash FF*/);
        a->moveStage = MJ_RUNNING;
        break;
    case MODE_CORRECT:
        a->targetSteps = absTarget;
        a->correctRetries = kMaxCorrectionRetries;
        startOpenLoopMove(axis, absTarget, speed, true /*backlash FF*/);
        a->moveStage = MJ_RUNNING;
        break;
    case MODE_SERVO:
        startServo(axis, absTarget, speed);
        break;
    case MODE_OPEN_LOOP:
    default:
        // Keep the caller-configured acceleration (moveMotor would clobber
        // it with its hardcoded 1000).
        issueMove(axis, targetSteps, speed, !isAbsolute, 0 /*keep accel*/);
        a->moveStage = MJ_IDLE;
        break;
    }
#else
    (void)targetSteps; (void)speed; (void)isAbsolute; (void)modeOverride;
#endif
}

void axis_stop(int axis)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
        return;
    if (axis < AXIS_MAX_STEPPERS)
        cancelMotion(axis);
#if CONFIG_UC2_MOTOR_CONTROLLER
    focusmotor_stop_stepper(axis);
#endif
}

void axis_set_mode(int axis, uint8_t mode)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
        return;
    // Feedback modes require an encoder; fall back to OPEN_LOOP otherwise.
    if (mode != MODE_OPEN_LOOP && !g_axes[axis].hasEnc)
    {
        log_w("axis %d has no encoder; forcing OPEN_LOOP", axis);
        mode = MODE_OPEN_LOOP;
    }
    g_axes[axis].fb.mode = mode;
    // Keep the watchdog gate in step with the configured mode for moves
    // that never pass through moveTo (plain open-loop dispatch).
    g_axes[axis].activeMode = mode;
}

AxisFeedback axis_get_feedback(int axis)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
    {
        AxisFeedback fb;
        memset(&fb, 0, sizeof(fb));
        return fb;
    }
    return g_axes[axis].fb;
}

void axis_reset(int axis, uint8_t policy)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
        return;
    AxisState *a = &g_axes[axis];

    // Abandon any in-flight move/servo and clear PID/stall state.
    if (axis < AXIS_MAX_STEPPERS)
        cancelMotion(axis);

    // Clear latched fault/health. Preserve a CAL_INVALID warning unless a
    // fresh calibration replaces it.
    a->fb.fault = a->cal.derived ? FAULT_CAL_INVALID : FAULT_NONE;
    a->fb.health = HEALTH_OK;
    axis_wd_reset(&a->wdState);

    if (!a->hasEnc)
        return;

    switch (policy)
    {
    case RESET_TRUST_ENCODER:
        // Adopt the encoder-derived position as truth: push it into FAS so
        // the next absolute move computes from the corrected origin.
        if (a->cal.valid)
        {
#if CONFIG_UC2_MOTOR_CONTROLLER
            int32_t measured = a->originSteps + axisCountsToSteps(&a->cal, encoder_backend_get_count(&a->encoder));
            axis_motor_set_position(axis, measured);
            a->originSteps = measured;
            encoder_backend_zero(&a->encoder);
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
        a->fb.referenced = false;
        break;

    default:
        break;
    }
    updateFeedback(axis);
}

// Push the calibration outcome to the host as an async JSON event so the
// measured numbers are available without a follow-up SDO/OD read:
//  {"axisCalibration":{"axis":1,"ok":1,"countsPerStep":0.15688,"countSign":-1,
//    "stepsPerCount":6.37,"backlashCounts":3,"backlashSteps":19,
//    "residualScatter":6,"quality":100,"microsteps":16,"fault":"NONE"}}
static void emitCalibrationJson(int axis, bool ok, uint8_t fault)
{
    const AxisCalibration *c = &g_axes[axis].cal;
    cJSON *root = cJSON_CreateObject();
    cJSON *o = cJSON_AddObjectToObject(root, "axisCalibration");
    cJSON_AddNumberToObject(o, "axis", axis);
    cJSON_AddNumberToObject(o, "ok", ok ? 1 : 0);
    if (ok)
    {
        double cps = (double)c->countsPerStep_q16 / 65536.0;
        cJSON_AddNumberToObject(o, "countsPerStep", cps);
        cJSON_AddNumberToObject(o, "stepsPerCount", (cps > 0.0) ? (1.0 / cps) : 0.0);
        cJSON_AddNumberToObject(o, "countsPerStepQ16", c->countsPerStep_q16);
        cJSON_AddNumberToObject(o, "countSign", c->countSign);
        cJSON_AddNumberToObject(o, "backlashCounts", c->backlashCounts);
        cJSON_AddNumberToObject(o, "backlashSteps", abs(axisCountsToSteps(c, c->backlashCounts)));
        cJSON_AddNumberToObject(o, "residualScatter", c->residualScatter);
        cJSON_AddNumberToObject(o, "quality", c->quality);
        cJSON_AddNumberToObject(o, "microsteps", c->microstepsAtCal);

        // Raw regression inputs, so a bad fit can be diagnosed off-device:
        // "points" is [[steps,counts],...] in execution order.
        const AxisCalibrationLastRun *lr = axis_cal_last_run();
        cJSON_AddNumberToObject(o, "r2", lr->r2);
        cJSON_AddNumberToObject(o, "slope", lr->slope);
        cJSON_AddNumberToObject(o, "intercept", lr->intercept);
        // Per-leg fits: forward and reverse should be parallel; their
        // intercept difference is the backlash (hysteresis loop width).
        cJSON_AddNumberToObject(o, "slopeForward", lr->slopeForward);
        cJSON_AddNumberToObject(o, "slopeReverse", lr->slopeReverse);
        cJSON_AddNumberToObject(o, "interceptForward", lr->interceptForward);
        cJSON_AddNumberToObject(o, "interceptReverse", lr->interceptReverse);
        cJSON_AddNumberToObject(o, "r2Forward", lr->r2Forward);
        cJSON_AddNumberToObject(o, "r2Reverse", lr->r2Reverse);
        cJSON_AddNumberToObject(o, "nForward", lr->nForward);
        cJSON *pts = cJSON_AddArrayToObject(o, "points");
        for (uint8_t i = 0; i < lr->nPoints; i++)
        {
            cJSON *pair = cJSON_CreateArray();
            cJSON_AddItemToArray(pair, cJSON_CreateNumber(lr->stepsAtPoint[i]));
            cJSON_AddItemToArray(pair, cJSON_CreateNumber(lr->countsAtPoint[i]));
            cJSON_AddItemToArray(pts, pair);
        }
    }
    cJSON_AddStringToObject(o, "fault", axis_notify_fault_name(fault));
    char *json = cJSON_PrintUnformatted(root);
    if (json)
    {
        serial_out_safe_send_json_string(json);
        free(json);
    }
    cJSON_Delete(root);
}

// ------------------------------------------------------------- calibration
// C hook impls for the calibration routine (ctx = axis index, packed).
static int32_t calHookStepPos(void *ctx)
{
    return commandedStepsOf((int)(intptr_t)ctx);
}

static bool calHookMoveRelBlocking(void *ctx, int32_t deltaSteps, int32_t speed)
{
    return blockingRelMove((int)(intptr_t)ctx, deltaSteps, speed);
}

static int64_t calHookGetRawCount(void *ctx)
{
    return encoder_backend_get_count(&g_axes[(int)(intptr_t)ctx].encoder);
}

static bool calHookAborted(void *ctx)
{
    return axisAborted((int)(intptr_t)ctx);
}

bool axis_calibrate(int axis, const AxisCalibrationParams *params)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
        return false;
    AxisState *a = &g_axes[axis];
    if (!a->hasEnc)
    {
        log_w("calibrateAxis %d: no encoder", axis);
        a->fb.fault = FAULT_CAL_FAILED;
        return false;
    }

    // Origin: zero the encoder and record the common step origin.
    reanchorOrigin(axis);

    AxisCalibrationParams p;
    if (params)
    {
        p = *params;
    }
    else
    {
        axis_cal_params_default(&p, currentMicrosteps());
    }

    AxisCalibrationHooks hooks;
    hooks.ctx = (void *)(intptr_t)axis;
    hooks.get_step_pos = calHookStepPos;
    hooks.move_rel_blocking = calHookMoveRelBlocking;
    hooks.get_raw_count = calHookGetRawCount;
    hooks.aborted = calHookAborted;

    AxisCalibration result;
    memset(&result, 0, sizeof(result));
    AxisFault err = FAULT_NONE;
    log_i("Calibrating axis %d — hooks wired: getStepPos=%d moveRelBlocking=%d "
          "getRawCount=%d aborted=%d",
          axis, (int)(hooks.get_step_pos != NULL), (int)(hooks.move_rel_blocking != NULL),
          (int)(hooks.get_raw_count != NULL), (int)(hooks.aborted != NULL));
    bool ok = axis_cal_run(&hooks, &p, &result, &err);
    if (ok)
    {
        a->cal = result;
        axis_cal_store_save(axis, &a->cal);
        a->fb.calibrated = true;
        a->fb.fault = FAULT_NONE;
        a->fb.health = HEALTH_OK;
        configureWatchdog(axis); // thresholds now follow measured scatter
        reanchorOrigin(axis); // fresh origin after all the probe moves
        axis_wd_reset(&a->wdState);
        if (a->fb.mode == MODE_OPEN_LOOP)
            a->fb.mode = MODE_MONITOR;
        log_i("calibrateAxis %d OK (q=%u scatter=%u)", axis,
              a->cal.quality, a->cal.residualScatter);
    }
    else
    {
        a->fb.fault = err;
        log_e("calibrateAxis %d FAILED (fault=%d)", axis, err);
    }
    emitCalibrationJson(axis, ok, (uint8_t)(ok ? FAULT_NONE : err));
    return ok;
}

bool axis_calibrate_defaults(int axis)
{
    AxisCalibrationParams p;
    axis_cal_params_default(&p, currentMicrosteps());
    return axis_calibrate(axis, &p);
}

// ------------------------------------------------- encoder linearity sweep
bool axis_encoder_table(int axis, int points, int32_t stepSize, int32_t speed)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
        return false;
    AxisState *a = &g_axes[axis];
    if (!a->hasEnc)
    {
        log_w("encoderTable %d: no encoder", axis);
        return false;
    }
    if (points < 1)  points = 10;
    if (points > 40) points = 40; // bound travel + JSON size
    if (stepSize == 0) stepSize = 500;
    if (speed <= 0)    speed = 2000;

    // Sweep out and back from the current position so the two legs expose
    // hysteresis/backlash directly (forward and reverse at the same spots).
    const int total = 2 * points + 1;
    static int32_t stepsArr[2 * 40 + 1];
    static int32_t cntsArr[2 * 40 + 1];
    int n = 0;

    encoder_backend_zero(&a->encoder);
    int32_t cumSteps = 0;
    stepsArr[n] = 0;
    cntsArr[n] = (int32_t)encoder_backend_get_count(&a->encoder);
    n++;

    log_i("encoderTable ax%d: %d points x %d steps @ %d steps/s (out and back)",
          axis, points, (int)stepSize, (int)speed);

    bool ok = true;
    for (int leg = 0; leg < 2 && ok; leg++)
    {
        int32_t delta = (leg == 0) ? stepSize : -stepSize;
        for (int i = 0; i < points && n < total; i++)
        {
            if (!blockingRelMove(axis, delta, speed))
            {
                log_e("encoderTable ax%d aborted at point %d", axis, n);
                ok = false;
                break;
            }
            delay(40); // settle before sampling
            esp_task_wdt_reset();
            cumSteps += delta;
            stepsArr[n] = cumSteps;
            cntsArr[n] = (int32_t)encoder_backend_get_count(&a->encoder);
            n++;
        }
    }

    // Emit the whole table as one event: plot counts vs steps to judge
    // linearity, and compare the two legs to see hysteresis.
    cJSON *root = cJSON_CreateObject();
    cJSON *o = cJSON_AddObjectToObject(root, "encoderTable");
    cJSON_AddNumberToObject(o, "axis", axis);
    cJSON_AddNumberToObject(o, "stepSize", stepSize);
    cJSON_AddNumberToObject(o, "points", points);
    cJSON_AddNumberToObject(o, "speed", speed);
    cJSON_AddNumberToObject(o, "ok", ok ? 1 : 0);
    cJSON *arr = cJSON_AddArrayToObject(o, "data");
    for (int i = 0; i < n; i++)
    {
        cJSON *pair = cJSON_CreateArray();
        cJSON_AddItemToArray(pair, cJSON_CreateNumber(stepsArr[i]));
        cJSON_AddItemToArray(pair, cJSON_CreateNumber(cntsArr[i]));
        cJSON_AddItemToArray(arr, pair);
    }
    char *json = cJSON_PrintUnformatted(root);
    if (json)
    {
        serial_out_safe_send_json_string(json);
        free(json);
    }
    cJSON_Delete(root);

    reanchorOrigin(axis); // sweep moved us around; re-anchor cleanly
    axis_wd_reset(&a->wdState);
    return ok;
}

void axis_request_encoder_table(int axis, int points, int32_t stepSize, int32_t speed)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT || !g_setupDone)
        return;
    AxisState *a = &g_axes[axis];
    a->reqEncTablePoints = points;
    a->reqEncTableStep = stepSize;
    a->reqEncTableSpeed = speed;
    a->reqEncTable = true; // set last: commit flag
}

AxisCalibration axis_get_calibration(int axis)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
    {
        AxisCalibration cal;
        memset(&cal, 0, sizeof(cal));
        return cal;
    }
    return g_axes[axis].cal;
}

bool axis_has_encoder(int axis)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
        return false;
    return g_axes[axis].hasEnc;
}

void axis_request_mode(int axis, uint8_t mode)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT || mode > MODE_SERVO || !g_setupDone)
        return;
    g_axes[axis].reqMode = (int8_t)mode;
}

void axis_request_reset(int axis, uint8_t policy)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT || policy > RESET_FORCE_REHOME || !g_setupDone)
        return;
    g_axes[axis].reqReset = (int8_t)policy;
}

void axis_request_calibration(int axis)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT || !g_setupDone)
        return;
    g_axes[axis].reqCalibrate = true;
}
