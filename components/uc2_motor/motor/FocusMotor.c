// T4.1: pure C (was the C++ FocusMotor namespace).
static const char *TAG = "FocusMotor";
#include <string.h>
#include <stdio.h>
#include "uc2_hal/uc2_log.h"  // log_i / log_w / log_e
#include "uc2_serial_out.h"  // serial_out_safe_send_json_string
#include "PinConfigDefault.h"
#include "FocusMotor.h"
#include "cJsonTool.h"
#include "State.h"
#include "QidRegistry.h"
#include "esp_debug_helpers.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"  // SemaphoreHandle_t
#include "nvs.h"
#ifdef USE_TCA9535
#include "tca_controller.h"
#endif
#ifdef USE_FASTACCEL
#include "FAccelStep.h"
#endif
#ifdef USE_ACCELSTEP
#include "AccelStep.h"
#endif
#ifdef STAGE_SCAN
#include "StageScan.h"
#endif

#ifdef DIGITAL_IN_CONTROLLER
#include "DigitalInController.h"
#endif

static MotorData a_dat;
static MotorData x_dat;
static MotorData y_dat;
static MotorData z_dat;
static MotorData b_dat;
static MotorData c_dat;
static MotorData d_dat;
static MotorData e_dat;
static MotorData f_dat;
static MotorData g_dat;

MotorData *data[MOTOR_AXIS_COUNT]; // TODO!!!
bool focusmotor_isActivated[MOTOR_AXIS_COUNT] = {false};
int logcount;
// Grace-period counter: skip this many loop iterations after startStepper()
// before the stop-condition is allowed to fire.  Gives the stepper time to
// start actually running (especially important for CAN-dispatched moves).
static int waitForFirstRun[] = {0, 0, 0, 0};

// Per-axis edge state for the hard-limit endstop. We detect a hit on the
// RISING edge of "pressed" and clear the lockout on the FALLING edge, instead
// of relying on a sticky hardLimitTriggered that only cleared while idle.
// This is what makes a shared endstop GPIO (both physical limits wired in
// parallel) re-arm correctly: each new contact - even at the opposite end,
// even while the motor keeps moving - is a fresh trip.
static bool s_hlPrevPressed[MOTOR_AXIS_COUNT] = {false};
static bool s_hlPrevValid[MOTOR_AXIS_COUNT] = {false};

static SemaphoreHandle_t xMutex = NULL;
static SemaphoreHandle_t xSerialMutex = NULL; // Mutex for serial JSON output

// Guard: set to false during setup() to prevent NVS writes while RMT ISR is active.
// Calling nvs_set_*() with the RMT driver running causes a
// "Cache disabled but cached memory region accessed" panic because the RMT ISR
// is not placed in IRAM and fires while the SPI flash cache is disabled.
static bool _setupComplete = false;

// ── small NVS helpers (was uc2::Preferences over "UC2") ────────────────────
static bool nvs_begin(nvs_handle_t *h)
{
    return nvs_open("UC2", NVS_READWRITE, h) == ESP_OK;
}
static void nvs_end(nvs_handle_t h)
{
    if (h)
        nvs_close(h);
}

static void setup_data(void);
static void fill_data_impl(void);

MotorData **focusmotor_get_data(void)
{
    // `data` is an array (never a null pointer), so the old `data != nullptr`
    // check was always true and the else branch returned a dangling pointer to
    // a local array. Detect the real "uninitialized" state (first slot still
    // null) and populate the array with the static MotorData objects on demand.
    // This also makes getData() safe to call before the full setup() runs
    // (e.g. in the host unit-test suite, where setup() is never invoked).
    if (data[A] == NULL)
        setup_data();
    return data;
}

void focusmotor_set_data(int axis, MotorData *mData)
{
    memcpy(data[axis], mData, sizeof(MotorData));
}

// Compute the requested motion direction for the current command parameters.
// Returns +1, -1, or 0 (unknown / stop / no motion).
static int8_t computeRequestedDir(int axis)
{
    MotorData *md = focusmotor_get_data()[axis];
    if (md->isStop)
        return 0;
    if (md->isforever)
    {
        if (md->speed > 0)
            return 1;
        if (md->speed < 0)
            return -1;
        return 0;
    }
    if (md->absolutePosition)
    {
        int32_t delta = md->targetPosition - md->currentPosition;
        if (delta > 0)
            return 1;
        if (delta < 0)
            return -1;
        return 0;
    }
    // relative move
    if (md->targetPosition > 0)
        return 1;
    if (md->targetPosition < 0)
        return -1;
    return 0;
}

// Returns false if motion in that direction is locked out by a previous
// hard-limit hit. requestedDir is +1, -1, or 0.
static bool directionAllowed(int axis, int8_t requestedDir)
{
    MotorData *md = focusmotor_get_data()[axis];
    if (md->hardLimitLockoutDir == 0)
        return true;
    if (requestedDir == 0)
        return true; // let stops / zero-speed updates through
    return requestedDir != md->hardLimitLockoutDir;
}

void focusmotor_start_stepper(int axis, int reduced)
{
    /*
    reduced organizes the amount of data that is being transmitted (E.g. via CAN Bus)
    reduced: 0 => All data transfered MotorData
    reduced: 1 => MotorDataReduced is transfered
    reduced: 2 => single Value updates
    */

    // Directional hard-limit lockout. Homing legitimately drives into the endstop,
    // so it always bypasses the guard.
    int8_t requestedDir = computeRequestedDir(axis);
    if (!focusmotor_get_data()[axis]->isHoming && !directionAllowed(axis, requestedDir))
    {
        log_w("Axis %d: motion in dir %d blocked by hard-limit lockout (lockoutDir=%d)",
              axis, (int)requestedDir, (int)focusmotor_get_data()[axis]->hardLimitLockoutDir);
        focusmotor_get_data()[axis]->stopped = true;
        focusmotor_send_motor_pos(axis, 0, -3); // error report
        qid_report_action_error(focusmotor_get_data()[axis]->qid);
        return;
    }
    if (requestedDir != 0)
        focusmotor_get_data()[axis]->lastCommandedDir = requestedDir;

    // Use timeout instead of portMAX_DELAY to prevent PS4 controller from blocking indefinitely
    // when serial commands are being processed. 50ms is enough for most operations while
    // allowing gamepad to retry quickly if mutex is busy.
    if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)))
    {
#if defined(USE_FASTACCEL)
        waitForFirstRun[axis] = 10;
        faccel_step_start(axis);
#elif defined(USE_ACCELSTEP)
        waitForFirstRun[axis] = 10;
        accel_step_start(axis);
#endif
        focusmotor_get_data()[axis]->stopped = false;
        xSemaphoreGive(xMutex);
    }
    else
    {
        log_w("startStepper: Could not obtain mutex to start motor on axis %d", axis);
        // Optionally, set an error state or retry logic here
    }
}

void focusmotor_toggle_stepper(Stepper s, bool isStop, int reduced)
{
    /*
    reduced:
        0 => Full MotorDAta
        1 => MotorDataReduced
        2 => Single Value Updates
    */
    if (isStop)
    {
        focusmotor_stop_stepper(s);
    }
    else
    {
        focusmotor_start_stepper(s, reduced);
    }
}

void focusmotor_move_motor(int32_t pos, int32_t speed, int s, bool isRelative)
{
    // move motor
    // blocking = true => wait until motor is done
    // blocking = false => fire and forget
    data[s]->targetPosition = pos;
    data[s]->isforever = false;
    data[s]->absolutePosition = !isRelative;
    data[s]->isStop = false;
    data[s]->stopped = false;
    data[s]->speed = speed;
    data[s]->acceleration = 1000;
    data[s]->isaccelerated = 1;
    log_i("Moving motor %i to position %i with speed %i, isRelative: %i", s, (int)pos, (int)speed, (int)isRelative);
    focusmotor_start_stepper(s, 1); // reduced = 1 means that we only send the motor data reduced to the slave, so that it can handle it faster
}

void focusmotor_set_auto_enable(bool enable)
{
#ifdef USE_FASTACCEL
    faccel_step_set_auto_enable(enable);
#endif
}

void focusmotor_set_enable(bool enable)
{
#ifdef USE_FASTACCEL
    faccel_step_enable(enable);
#elif defined USE_ACCELSTEP
    accel_step_enable(enable);
#endif
}

void focusmotor_update_data(int axis)
{
#if defined(USE_FASTACCEL)
    faccel_step_update_data(axis);
#elif defined(USE_ACCELSTEP)
    accel_step_update_data(axis);
#endif
}

int32_t focusmotor_get_current_motor_position(int axis)
{
#if defined(USE_FASTACCEL)
    return (int32_t)faccel_step_get_current_position((Stepper)axis);
#else
    return data[axis]->currentPosition;
#endif
}

static void setup_data(void)
{
    data[A] = &a_dat;
    data[X] = &x_dat;
    data[Y] = &y_dat;
    data[Z] = &z_dat;
#if MOTOR_AXIS_COUNT > 4 // TODO: We should only setup the data for the motors that are actually present according to the pin configuration, but for now we just assume that if MOTOR_AXIS_COUNT > 4 then we have all 7 axes and set them up accordingly
    data[B] = &b_dat;
    data[C] = &c_dat;
    data[D] = &d_dat;
    data[E] = &e_dat;
    data[F] = &f_dat;
    data[G] = &g_dat;
#endif

    // C replacement for the C++ default member initializers of the static
    // MotorData objects (qid=-1, stopped=true, isEnable=true, ...).
    for (int i = 0; i < MOTOR_AXIS_COUNT; i++)
        motor_data_defaults(data[i]);

    if (data[A] == NULL)
        log_e("Stepper A data NULL");
    if (data[X] == NULL)
        log_e("Stepper X data NULL");
    if (data[Y] == NULL)
        log_e("Stepper Y data NULL");
    if (data[Z] == NULL)
        log_e("Stepper Z data NULL");
#if MOTOR_AXIS_COUNT > 4
    if (data[B] == NULL)
        log_e("Stepper B data NULL");
    if (data[C] == NULL)
        log_e("Stepper C data NULL");
    if (data[D] == NULL)
        log_e("Stepper D data NULL");
    if (data[E] == NULL)
        log_e("Stepper E data NULL");
    if (data[F] == NULL)
        log_e("Stepper F data NULL");
    if (data[G] == NULL)
        log_e("Stepper G data NULL");
#endif
}

static void fill_data_impl(void)
{
    // NVS key buffers (C++ used ("prefix" + String(axis)).c_str())
    char key[24];
    int32_t v;
    bool b;
    float f;
    int8_t c;
    nvs_handle_t h = 0;
    if (!nvs_begin(&h))
        return;

    // setup motor pins
    if (pinConfig.MOTOR_A_STEP >= 0)
    {
        snprintf(key, sizeof key, "motor%d", A);
        data[A]->dirPin = pinConfig.MOTOR_A_DIR;
        data[A]->stpPin = pinConfig.MOTOR_A_STEP;
        v = 0;
        if (nvs_get_i32(h, key, &v) == ESP_OK)
            data[A]->currentPosition = v;
        data[A]->directionPinInverted = nvs_get_i32(h, "motainvert", &v) == ESP_OK ? v : 0;
        b = false;
        snprintf(key, sizeof key, "joyDir%d", A);
        if (nvs_get_u8(h, key, (uint8_t *)&b) != ESP_OK)
            b = false;
        data[A]->joystickDirectionInverted = b;
        f = (float)pinConfig.JOYSTICK_SPEED_MULTIPLIER;
        snprintf(key, sizeof key, "joySpd%d", A);
        if (nvs_get_float(h, key, &f) != ESP_OK)
            f = (float)pinConfig.JOYSTICK_SPEED_MULTIPLIER;
        data[A]->joystickSpeedMultiplier = f;
        b = pinConfig.hardLimitEnabledA;
        snprintf(key, sizeof key, "hlEn%d", A);
        if (nvs_get_u8(h, key, (uint8_t *)&b) != ESP_OK)
            b = pinConfig.hardLimitEnabledA;
        data[A]->hardLimitEnabled = b; // Disabled by default
        b = pinConfig.hardLimitPolarityA;
        snprintf(key, sizeof key, "hlPol%d", A);
        if (nvs_get_u8(h, key, (uint8_t *)&b) != ESP_OK)
            b = pinConfig.hardLimitPolarityA;
        data[A]->hardLimitPolarity = b; // NO by default
        focusmotor_isActivated[A] = true;
        log_i("Motor A position: %i", data[A]->currentPosition);
    }
    if (pinConfig.MOTOR_X_STEP >= 0)
    {
        data[X]->dirPin = pinConfig.MOTOR_X_DIR;
        data[X]->stpPin = pinConfig.MOTOR_X_STEP;
        snprintf(key, sizeof key, "motor%d", X);
        v = 0;
        if (nvs_get_i32(h, key, &v) == ESP_OK)
            data[X]->currentPosition = v;
        data[X]->directionPinInverted = nvs_get_i32(h, "motxinv", &v) == ESP_OK ? v : 0;
        b = false;
        snprintf(key, sizeof key, "joyDir%d", X);
        if (nvs_get_u8(h, key, (uint8_t *)&b) != ESP_OK)
            b = false;
        data[X]->joystickDirectionInverted = b;
        f = (float)pinConfig.JOYSTICK_SPEED_MULTIPLIER;
        snprintf(key, sizeof key, "joySpd%d", X);
        if (nvs_get_float(h, key, &f) != ESP_OK)
            f = (float)pinConfig.JOYSTICK_SPEED_MULTIPLIER;
        data[X]->joystickSpeedMultiplier = f;
        b = pinConfig.hardLimitEnabledX;
        snprintf(key, sizeof key, "hlEn%d", X);
        if (nvs_get_u8(h, key, (uint8_t *)&b) != ESP_OK)
            b = pinConfig.hardLimitEnabledX;
        data[X]->hardLimitEnabled = b;
        b = pinConfig.hardLimitPolarityX;
        snprintf(key, sizeof key, "hlPol%d", X);
        if (nvs_get_u8(h, key, (uint8_t *)&b) != ESP_OK)
            b = pinConfig.hardLimitPolarityX;
        data[X]->hardLimitPolarity = b;
        focusmotor_isActivated[X] = true;
        log_i("Motor X position: %i", data[X]->currentPosition);
    }
    if (pinConfig.MOTOR_Y_STEP >= 0)
    {
        data[Y]->dirPin = pinConfig.MOTOR_Y_DIR;
        data[Y]->stpPin = pinConfig.MOTOR_Y_STEP;
        snprintf(key, sizeof key, "motor%d", Y);
        v = 0;
        if (nvs_get_i32(h, key, &v) == ESP_OK)
            data[Y]->currentPosition = v;
        data[Y]->directionPinInverted = nvs_get_i32(h, "motyinv", &v) == ESP_OK ? v : 0;
        b = false;
        snprintf(key, sizeof key, "joyDir%d", Y);
        if (nvs_get_u8(h, key, (uint8_t *)&b) != ESP_OK)
            b = false;
        data[Y]->joystickDirectionInverted = b;
        f = (float)pinConfig.JOYSTICK_SPEED_MULTIPLIER;
        snprintf(key, sizeof key, "joySpd%d", Y);
        if (nvs_get_float(h, key, &f) != ESP_OK)
            f = (float)pinConfig.JOYSTICK_SPEED_MULTIPLIER;
        data[Y]->joystickSpeedMultiplier = f;
        b = pinConfig.hardLimitEnabledY;
        snprintf(key, sizeof key, "hlEn%d", Y);
        if (nvs_get_u8(h, key, (uint8_t *)&b) != ESP_OK)
            b = pinConfig.hardLimitEnabledY;
        data[Y]->hardLimitEnabled = b;
        b = pinConfig.hardLimitPolarityY;
        snprintf(key, sizeof key, "hlPol%d", Y);
        if (nvs_get_u8(h, key, (uint8_t *)&b) != ESP_OK)
            b = pinConfig.hardLimitPolarityY;
        data[Y]->hardLimitPolarity = b;
        focusmotor_isActivated[Y] = true;
        log_i("Motor Y position: %i", data[Y]->currentPosition);
    }
    if (pinConfig.MOTOR_Z_STEP >= 0)
    {
        data[Z]->dirPin = pinConfig.MOTOR_Z_DIR;
        data[Z]->stpPin = pinConfig.MOTOR_Z_STEP;
        snprintf(key, sizeof key, "motor%d", Z);
        v = 0;
        if (nvs_get_i32(h, key, &v) == ESP_OK)
            data[Z]->currentPosition = v;
        data[Z]->directionPinInverted = nvs_get_i32(h, "motzinv", &v) == ESP_OK ? v : 0;
        b = false;
        snprintf(key, sizeof key, "joyDir%d", Z);
        if (nvs_get_u8(h, key, (uint8_t *)&b) != ESP_OK)
            b = false;
        data[Z]->joystickDirectionInverted = b;
        f = (float)pinConfig.JOYSTICK_SPEED_MULTIPLIER_Z;
        snprintf(key, sizeof key, "joySpd%d", Z);
        if (nvs_get_float(h, key, &f) != ESP_OK)
            f = (float)pinConfig.JOYSTICK_SPEED_MULTIPLIER_Z;
        data[Z]->joystickSpeedMultiplier = f;
        b = pinConfig.hardLimitEnabledZ;
        snprintf(key, sizeof key, "hlEn%d", Z);
        if (nvs_get_u8(h, key, (uint8_t *)&b) != ESP_OK)
            b = pinConfig.hardLimitEnabledZ;
        data[Z]->hardLimitEnabled = b;
        b = pinConfig.hardLimitPolarityZ;
        snprintf(key, sizeof key, "hlPol%d", Z);
        if (nvs_get_u8(h, key, (uint8_t *)&b) != ESP_OK)
            b = pinConfig.hardLimitPolarityZ;
        data[Z]->hardLimitPolarity = b;
        focusmotor_isActivated[Z] = true;
        log_i("Motor Z position: %i", data[Z]->currentPosition);
    }
    if (pinConfig.MOTOR_B_STEP >= 0)
    {
        data[B]->dirPin = pinConfig.MOTOR_B_DIR;
        data[B]->stpPin = pinConfig.MOTOR_B_STEP;
        snprintf(key, sizeof key, "motor%d", B);
        v = 0;
        if (nvs_get_i32(h, key, &v) == ESP_OK)
            data[B]->currentPosition = v;
        log_i("Motor B position: %i", data[B]->currentPosition);
    }
    if (pinConfig.MOTOR_C_STEP >= 0)
    {
        data[C]->dirPin = pinConfig.MOTOR_C_DIR;
        data[C]->stpPin = pinConfig.MOTOR_C_STEP;
        snprintf(key, sizeof key, "motor%d", C);
        v = 0;
        if (nvs_get_i32(h, key, &v) == ESP_OK)
            data[C]->currentPosition = v;
        log_i("Motor C position: %i", data[C]->currentPosition);
    }
    if (pinConfig.MOTOR_D_STEP >= 0)
    {
        data[D]->dirPin = pinConfig.MOTOR_D_DIR;
        data[D]->stpPin = pinConfig.MOTOR_D_STEP;
        snprintf(key, sizeof key, "motor%d", D);
        v = 0;
        if (nvs_get_i32(h, key, &v) == ESP_OK)
            data[D]->currentPosition = v;
        log_i("Motor D position: %i", data[D]->currentPosition);
    }
    if (pinConfig.MOTOR_E_STEP >= 0)
    {
        data[E]->dirPin = pinConfig.MOTOR_E_DIR;
        data[E]->stpPin = pinConfig.MOTOR_E_STEP;
        snprintf(key, sizeof key, "motor%d", E);
        v = 0;
        if (nvs_get_i32(h, key, &v) == ESP_OK)
            data[E]->currentPosition = v;
        log_i("Motor E position: %i", data[E]->currentPosition);
    }
    if (pinConfig.MOTOR_F_STEP >= 0)
    {
        data[F]->dirPin = pinConfig.MOTOR_F_DIR;
        data[F]->stpPin = pinConfig.MOTOR_F_STEP;
        snprintf(key, sizeof key, "motor%d", F);
        v = 0;
        if (nvs_get_i32(h, key, &v) == ESP_OK)
            data[F]->currentPosition = v;
        log_i("Motor F position: %i", data[F]->currentPosition);
    }
    if (pinConfig.MOTOR_G_STEP >= 0)
    {
        data[G]->dirPin = pinConfig.MOTOR_G_DIR;
        data[G]->stpPin = pinConfig.MOTOR_G_STEP;
        snprintf(key, sizeof key, "motor%d", G);
        v = 0;
        if (nvs_get_i32(h, key, &v) == ESP_OK)
            data[G]->currentPosition = v;
        log_i("Motor G position: %i", data[G]->currentPosition);
    }

    // Restore the persisted directional hard-limit lockout so a stage left
    // trapped against an endstop across a reboot/power-cycle still knows
    // which way is safe to move. If the endstop is no longer pressed, the
    // runtime hard-limit check (evaluateHardLimitForAxis) clears it again on
    // the next loop.
    for (int ax = 0; ax < MOTOR_AXIS_COUNT; ax++)
    {
        if (focusmotor_isActivated[ax])
        {
            c = 0;
            snprintf(key, sizeof key, "hlLock%d", ax);
            if (nvs_get_i8(h, key, &c) != ESP_OK)
                c = 0;
            data[ax]->hardLimitLockoutDir = c;
        }
    }
    nvs_end(h);

    /*
    // setup trigger pins
    if (pinConfig.DIGITAL_OUT_1 > 0)
        data[X]->triggerPin = 1; // pixel^
    if (pinConfig.DIGITAL_OUT_2 > 0)
        data[Y]->triggerPin = 2; // line^
    if (pinConfig.DIGITAL_OUT_3 > 0)
        data[Z]->triggerPin = 3; // frame^
        */
}

void focusmotor_fill_data(void)
{
    fill_data_impl();
}

#ifdef USE_TCA9535
static void testTca(void)
{
    for (int iMotor = 0; iMotor < MOTOR_AXIS_COUNT; iMotor++)
    {
        // need to activate the motor's dir pin eventually
        // This also updates the dial's positions
        // only test those motors that are activated
        // Only test axes that are both activated AND have a local step pin.
        // Axes routed via CANopen (stpPin == -1) have no local FastAccelStepper
        // handle; calling startStepper() on them would access faststeppers[]
        // out-of-bounds and crash.
        if (focusmotor_isActivated[iMotor] && focusmotor_get_data()[iMotor]->stpPin >= 0)
        {
            Stepper s = (Stepper)iMotor;
            data[s]->absolutePosition = false;
            data[s]->targetPosition = -1;
            focusmotor_start_stepper(iMotor, 0);
            delay(10);
            focusmotor_stop_stepper(iMotor);
            data[s]->targetPosition = 1;
            focusmotor_start_stepper(iMotor, 0);
            delay(10);
            focusmotor_stop_stepper(iMotor);
        }
    }
}
#endif

static void sendMotorPosition(void)
{
    // send motor positions
    for (int iMotor = 0; iMotor < MOTOR_AXIS_COUNT; iMotor++)
    {
        if (focusmotor_isActivated[iMotor])
        {
            focusmotor_send_motor_pos(iMotor, 0, -1);
        }
    }
}

void focusmotor_setup(void)
{
    // Initialize mutexes FIRST, before any other operations
    if (xMutex == NULL)
        xMutex = xSemaphoreCreateMutex();
    if (xSerialMutex == NULL)
        xSerialMutex = xSemaphoreCreateMutex();

    // Common setup for all
    setup_data();
    fill_data_impl();

#ifdef USE_FASTACCEL
#ifdef USE_TCA9535
    log_i("Setting external pin for FastAccelStepper");
    faccel_step_set_external_call_for_tca();
#endif
    faccel_step_setup();
#ifdef USE_TCA9535
    testTca();
#else
    sendMotorPosition();
#endif
#endif

#ifdef USE_ACCELSTEP
#ifdef USE_TCA9535
    accel_step_set_external_call_for_tca();
#endif
    accel_step_setup();
#ifdef USE_TCA9535
    testTca();
#else
    sendMotorPosition();
#endif
#endif
    // NVS writes are now safe: all ISR-driven peripherals are initialised and
    // any testTca() moves have completed.
    _setupComplete = true;
}

void focusmotor_set_hard_limit(int axis, bool enabled, bool polarity)
{
    // Set hard limit configuration for a motor axis
    // enabled: if true, motor will emergency stop when endstop is hit during normal operation
    // polarity: 0 = normally open (NO), 1 = normally closed (NC)
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
        return;

    focusmotor_get_data()[axis]->hardLimitEnabled = enabled;
    focusmotor_get_data()[axis]->hardLimitPolarity = polarity;
    if (!enabled)
    {
        // Immediately release any active lockout so motion is possible again
        focusmotor_set_hard_limit_lockout_dir(axis, 0);
        focusmotor_get_data()[axis]->hardLimitTriggered = false;
    }
    log_i("Set hard limit on axis %d: enabled=%d, polarity=%d", axis, (int)enabled, (int)polarity);

    // Save to preferences
    char key[16];
    nvs_handle_t h = 0;
    if (nvs_begin(&h))
    {
        snprintf(key, sizeof key, "hlEn%d", axis);
        nvs_set_u8(h, key, enabled ? 1 : 0);
        snprintf(key, sizeof key, "hlPol%d", axis);
        nvs_set_u8(h, key, polarity ? 1 : 0);
        nvs_commit(h);
        nvs_end(h);
    }
}

void focusmotor_clear_hard_limit_triggered(int axis)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
        return;
    focusmotor_get_data()[axis]->hardLimitTriggered = false;
    focusmotor_set_hard_limit_lockout_dir(axis, 0); // also clears the persisted lockout
    log_i("Cleared hard limit triggered flag and lockout for axis %d", axis);
}

// Set the directional hard-limit lockout for an axis AND persist it to NVS,
// so a stage left trapped against an endstop keeps the correct escape
// direction across a reboot/power-cycle. NVS write is skipped when unchanged.
void focusmotor_set_hard_limit_lockout_dir(int axis, int8_t dir)
{
    if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
        return;
    MotorData *md = focusmotor_get_data()[axis];
    if (md->hardLimitLockoutDir == dir)
        return; // no change -> no NVS write
    md->hardLimitLockoutDir = dir;
    char key[16];
    nvs_handle_t h = 0;
    if (nvs_begin(&h))
    {
        snprintf(key, sizeof key, "hlLock%d", axis);
        nvs_set_i8(h, key, dir);
        nvs_commit(h);
        nvs_end(h);
    }
    log_i("Axis %d hard-limit lockout dir -> %d (persisted)", axis, (int)dir);
}

// Per-axis hard-limit handling: EDGE-based trip + lockout-clear.
//
// Both physical endstops of a linear axis are wired in parallel onto a single
// GPIO ("pressed" at either end, released in between). The previous LEVEL-based
// logic gated the trip on a sticky hardLimitTriggered that only cleared while
// the motor was idle. When escaping one endstop you drive toward the other, so
// the release happened WHILE MOVING and the flag never cleared - which masked
// the hit at the far end and left the directional lockout pointing at the wrong
// wall. Result: the far endstop was ignored and the axis got "trapped" with the
// escape direction blocked. Detecting edges fixes this:
//   rising edge (released -> pressed) while moving -> trip on the CURRENT dir
//   falling edge (pressed -> released)             -> clear, even while moving
static void evaluateHardLimitForAxis(int axis, int digitalInputIdx)
{
    MotorData *md = focusmotor_get_data()[axis];
    if (!focusmotor_isActivated[axis] || !md->hardLimitEnabled)
        return;

#ifdef DIGITAL_IN_CONTROLLER
    bool endstopState = digitalin_get_val(digitalInputIdx); // C shim over the C++ module (uc2_io)
#else
    // If no digital input controller, assume endstop is not triggered (safe default)
    bool endstopState = false;
#endif
    // SINGLE SOURCE OF TRUTH for the endstop's active level: hardLimitPolarity.
    // polarity is the digital level read when the endstop IS triggered
    // (polarity=1 -> triggered when HIGH; polarity=0 -> triggered when LOW).
    // The homing state machine uses this exact same field (synced in startHome).
    bool pressed = (endstopState == md->hardLimitPolarity);

    // Keep the edge baseline current even while homing, so normal hard-limit
    // handling never synthesizes a stale edge when it resumes after a run.
    bool prevPressed = s_hlPrevPressed[axis];
    bool prevValid = s_hlPrevValid[axis];
    s_hlPrevPressed[axis] = pressed;
    s_hlPrevValid[axis] = true;

    // Homing deliberately drives into endstops; the homing task owns the axis.
    if (md->isHoming)
        return;
    if (!prevValid)
        return; // first sample: baseline only, never act on a synthetic edge

    // --- Trip on the RISING edge of a contact while the motor is moving ---
    if (pressed && !prevPressed && focusmotor_is_running(axis))
    {
        // Direction of the motion that just hit the endstop = the wall we are
        // now against. Using the CURRENT commanded direction (not a stale one)
        // keeps the lockout pointing at the correct end of a shared GPIO.
        int8_t dir = md->lastCommandedDir;
        if (dir == 0)
        {
            if (md->speed > 0)
                dir = 1;
            else if (md->speed < 0)
                dir = -1;
            else
            {
                int32_t delta = md->targetPosition - md->currentPosition;
                if (delta > 0)
                    dir = 1;
                else if (delta < 0)
                    dir = -1;
            }
        }
        log_e("HARD LIMIT TRIGGERED on axis %d (endstop=%d, polarity=%d, dir=%d)",
              axis, (int)endstopState, (int)md->hardLimitPolarity, (int)dir);
        focusmotor_set_hard_limit_lockout_dir(axis, dir); // persist so we can escape after a reboot
        md->hardLimitTriggered = true;
        focusmotor_stop_stepper(axis);
        focusmotor_send_motor_pos(axis, 0, -3); // one-shot error report
        return;
    }

    // --- Clear the lockout when the endstop is released ---
    // (a) FALLING edge: the axis just drove off the endstop -> free both
    //     directions immediately, even while still moving. This un-traps a
    //     shared-GPIO axis the instant it leaves the switch.
    // (b) safety net: released AND idle with a stale/persisted lockout still
    //     set (e.g. restored from NVS after a reboot when not on the switch).
    if (!pressed)
    {
        bool fallingEdge = prevPressed;
        bool idleStale = !focusmotor_is_running(axis);
        if ((fallingEdge || idleStale) &&
            (md->hardLimitLockoutDir != 0 || md->hardLimitTriggered))
        {
            log_i("Hard-limit lockout cleared on axis %d (released, edge=%d idle=%d)",
                  axis, (int)fallingEdge, (int)idleStale);
            focusmotor_set_hard_limit_lockout_dir(axis, 0);
            md->hardLimitTriggered = false;
        }
    }
}

void focusmotor_check_hard_limits(void)
{ // NODE_ROLE = 2 => can slave
#if defined(CAN_CONTROLLER_CANOPEN) && (NODE_ROLE == 2) && defined(MOTOR_CONTROLLER) && defined(DIGITAL_IN_CONTROLLER)
    Stepper mStepper = (Stepper)pinConfig.REMOTE_MOTOR_AXIS_ID;
    evaluateHardLimitForAxis(mStepper, 1);
#elif defined(MOTOR_CONTROLLER) && defined(DIGITAL_IN_CONTROLLER) && !defined(CAN_CONTROLLER_CANOPEN)
    // standalone case //TODO: add Stepper 0?
    evaluateHardLimitForAxis(X, 1);
    evaluateHardLimitForAxis(Y, 2);
    evaluateHardLimitForAxis(Z, 3);
#endif
}

void focusmotor_loop(void)
{
    // Check for QID timeouts
    qid_tick_timeout();

#if !defined(CAN_CONTROLLER_CANOPEN) || (NODE_ROLE == 2)
    focusmotor_check_hard_limits();
#endif

    for (int i = 0; i < MOTOR_AXIS_COUNT; i++)
    {
#if defined(USE_FASTACCEL) || defined(USE_ACCELSTEP)
        // Skip axes that were never activated (no step pin configured)
        if (!focusmotor_isActivated[i])
            continue;

        /*
#ifdef USE_FASTACCEL
log_i("FAS ax%d: ramp=0x%02x speedMHz=%ld curPos=%ld endPos=%ld qFull=%d",
  i, FAccelStep::rampState(i),
  (long)FAccelStep::getCurrentSpeedInMilliHz(i, true),
  (long)FAccelStep::getCurrentPosition(i),
  (long)FAccelStep::getPositionAfterCommandsCompleted(i),
  FAccelStep::isQueueFull(i));
#endif
*/

        // checks if a stepper is still running
        // seems like the i2c needs a moment to start the motor (i.e. act is async and loop is continously running, maybe faster than the motor can start)
        if (waitForFirstRun[i])
        {
            waitForFirstRun[i]--;
            continue;
        }
        if (0)
        {
            log_i("Checking motor %d: isRunning=%d, stopped=%d, isforever=%d, isHoming=%d",
                  i, focusmotor_is_running(i), data[i]->stopped, data[i]->isforever, data[i]->isHoming);
        }
        if (focusmotor_isActivated[i] && !focusmotor_is_running(i) && !data[i]->stopped && !data[i]->isforever && !data[i]->isHoming)
        {
            // If the motor is not running, we stop it, report the position and save the position
            // This is the ordinary case if the motor is not connected via I2C/CAN
            // Skip during homing - the homing task manages motor lifecycle
            log_d("Stop Motor (2) %i in loop, mIsRunning %i, data[i]->stopped %i", i, (int)focusmotor_is_running(i), (int)!data[i]->stopped);
            focusmotor_stop_stepper(i);
        }
#endif
    }
}

bool focusmotor_is_running(int i)
{
    bool mIsRunning = false;
    // Fallback: local stepper only (CAN routing handled by DeviceRouter)
#if defined(USE_FASTACCEL)
    mIsRunning = faccel_step_is_running(i);
#elif defined(USE_ACCELSTEP)
    mIsRunning = accel_step_is_running(i);
#endif
    return mIsRunning;
}

// returns json {"steppers":[...]} as qid
void focusmotor_send_motor_pos(int i, int arraypos, int qid)
{
    // Always update and persist position first, independent of serial mutex
    focusmotor_update_data(i);
    // Skip NVS write during setup: nvs_set_*() disables the SPI flash
    // cache, and if an RMT ISR fires at that moment the CPU panics with
    // "Cache disabled but cached memory region accessed".
    if (_setupComplete)
    {
        char key[16];
        nvs_handle_t h = 0;
        if (nvs_begin(&h))
        {
            snprintf(key, sizeof key, "motor%d", i);
            nvs_set_i32(h, key, data[i]->currentPosition);
            nvs_commit(h);
            nvs_end(h);
        }
    }

    // Safety check: ensure mutex is initialized
    if (xSerialMutex == NULL)
    {
        log_e("Serial mutex not initialized - cannot send motor position");
        return;
    }

    // Protect entire function to prevent concurrent access to cJSON objects and serial output
    // This prevents heap corruption when multiple threads try to send motor positions simultaneously
    if (!xSemaphoreTake(xSerialMutex, pdMS_TO_TICKS(1000)))
    {
        log_w("Failed to acquire serial mutex for sendMotorPos");
        return;
    }

    // update current position of the motor depending on the interface (already done above, refresh for JSON)
    focusmotor_update_data(i);

    cJSON *root = cJSON_CreateObject();
    if (root == NULL)
    {
        xSemaphoreGive(xSerialMutex);
        return; // Handle allocation failure
    }

    cJSON *stprs = cJSON_CreateArray();
    if (stprs == NULL)
    {
        cJSON_Delete(root);
        xSemaphoreGive(xSerialMutex);
        return; // Handle allocation failure
    }
    cJSON_AddItemToObject(root, key_steppers, stprs);
    if (qid == -1)
        qid = data[i]->qid;
    cJSON_AddNumberToObject(root, "qid", qid);

    cJSON *item = cJSON_CreateObject();
    if (item == NULL)
    {
        cJSON_Delete(root);
        xSemaphoreGive(xSerialMutex);
        return; // Handle allocation failure
    }
    cJSON_AddItemToArray(stprs, item);
    cJSON_AddNumberToObject(item, key_stepperid, i);
    cJSON_AddNumberToObject(item, key_position, data[i]->currentPosition);
    cJSON_AddNumberToObject(item, "isDone", data[i]->stopped);

    arraypos++;

    // CRITICAL: Serialize to string BEFORE releasing mutex to prevent doc from being freed
    // This ensures atomic operation: create JSON -> serialize -> delete
    char *jsonString = cJSON_PrintUnformatted(root);

    // Check if serialization was successful before proceeding
    if (jsonString == NULL)
    {
        log_e("Failed to serialize motor position JSON for motor %d", i);
        cJSON_Delete(root);
        xSemaphoreGive(xSerialMutex);
        return;
    }

    // Delete cJSON object immediately after serialization
    cJSON_Delete(root);
    root = NULL;

    // Release mutex AFTER JSON operations are complete
    xSemaphoreGive(xSerialMutex);

    // Now send the pre-serialized string through the safe output queue
    serial_out_safe_send_json_string(jsonString);

    // Free the serialized string - cJSON_PrintUnformatted allocates with malloc
    free(jsonString);
    jsonString = NULL;
}

void focusmotor_stop_stepper(int i)
{
    // Stop local stepper (CAN routing handled by DeviceRouter)
#if defined(USE_FASTACCEL)
    faccel_step_stop(i);
#elif defined(USE_ACCELSTEP)
    accel_step_stop(i);
#endif
    log_i("stopStepper Focus Motor %i, stopped: %i", i, (int)data[i]->stopped);
    // only send motor data if it was running before
    focusmotor_send_motor_pos(i, 0, -1);
    qid_report_action_done(data[i]->qid);
}

uint32_t focusmotor_get_position(Stepper s)
{
    log_i("Getting motor %i position", (int)s);
    focusmotor_update_data(s);
    return (uint32_t)focusmotor_get_data()[s]->currentPosition;
}

void focusmotor_set_position(Stepper s, int pos)
{
#ifdef USE_FASTACCEL
    faccel_step_set_position(s, pos);
#elif defined USE_ACCELSTEP
    accel_step_set_position(s, pos);
#endif
    focusmotor_get_data()[s]->currentPosition = pos;
}

void focusmotor_move(Stepper s, int steps, bool blocking)
{
#ifdef USE_FASTACCEL
    faccel_step_move(s, steps, blocking);
#endif
}
