// T4.1: pure C (was C++ with default member initializers).
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define MAX_VELOCITY_A     (20000)
#define MAX_ACCELERATION_A (600000)
#define DEFAULT_ACCELERATION (100000)

#pragma pack(push,1)
typedef struct MotorState {
    int32_t currentPosition;
    bool    isRunning;
    uint8_t axis;
} MotorState;
#pragma pack(pop)

// Dedicated QID report for CAN (kept small to fit single CAN frame)
#pragma pack(push,1)
typedef struct QidReport {
    int16_t qid;      // default -1, see motor_data_defaults
    uint8_t state;    // 0=done, 1=error
} QidReport;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct MotorAction
{
    // Runtime motor commands - sent frequently via CAN/I2C
    int32_t targetPosition;
    int32_t speed;
    bool    isforever;
    bool    absolutePosition;
    bool    isStop;
} MotorAction;
#pragma pack(pop)

// Legacy alias for backwards compatibility
typedef MotorAction MotorDataReduced;

#pragma pack(push,1)
typedef struct MotorSettings
{
    // Motor configuration settings - sent once or rarely
    bool    directionPinInverted;
    bool    joystickDirectionInverted;
    bool    isaccelerated;
    bool    isEnable;
    int32_t maxspeed;
    int32_t acceleration;

    // Trigger settings
    bool    isTriggered;
    int32_t offsetTrigger;
    int32_t triggerPeriod;
    int     triggerPin;

    // Pin configuration (can be removed if handled via PinConfig)
    int dirPin;
    int stpPin;

    // Hard limit settings (emergency stop on endstop hit)
    bool hardLimitEnabled;   // Enabled by default (see motor_settings_defaults)
    bool hardLimitPolarity;  // 0 = normally open (NO), 1 = normally closed (NC)
} MotorSettings;
#pragma pack(pop)

#pragma pack(push,1)
typedef struct MotorData
{
    // Runtime action fields
    int32_t speed;
    int32_t targetPosition;
    int32_t currentPosition;
    int     isforever;
    bool    absolutePosition;
    bool    isStop;
    int     qid;

    // Internal state
    bool stopped;
    bool endstop_hit;
    bool isHoming; // homing in progress - ignore soft limits

    // Settings - these should eventually be moved to MotorSettings
    bool  directionPinInverted;
    bool  joystickDirectionInverted;
    // Per-axis joystick speed-scaling multiplier. Runtime default is loaded
    // from pinConfig.JOYSTICK_SPEED_MULTIPLIER[_Z] in FocusMotor fill_data(),
    // but can be overridden and persisted via /motor_act "speedmult" (see
    // MotorJsonParser::parseSetJoystickSpeed). The 1.0f here is only a
    // compile-time placeholder, always overwritten before use.
    float joystickSpeedMultiplier;
    bool  isaccelerated;
    bool  isEnable;
    int32_t maxspeed;
    int32_t acceleration;
    bool  isTriggered;
    int32_t offsetTrigger;
    int32_t triggerPeriod;
    int   triggerPin;
    int   dirPin;
    int   stpPin;

    // Hard limit runtime state (settings are in MotorSettings, sent via CAN to slaves)
    bool   hardLimitEnabled;    // Runtime copy from MotorSettings
    bool   hardLimitPolarity;   // Runtime copy from MotorSettings (0=NO, 1=NC)
    bool   hardLimitTriggered;  // One-shot rising-edge flag for telemetry; lockout uses hardLimitLockoutDir

    // Directional lockout after a hard-limit hit.
    //  0 = no lockout active
    // +1 = endstop hit while moving in positive direction => only negative motion allowed
    // -1 = endstop hit while moving in negative direction => only positive motion allowed
    int8_t hardLimitLockoutDir;

    // Last commanded direction (+1, -1, or 0). Updated by startStepper().
    int8_t lastCommandedDir;
} MotorData;
#pragma pack(pop)

// Common minimal struct
#pragma pack(push,1)
typedef struct MotorDataValueUpdate {
    uint16_t offset;   // e.g. offsetof(MotorData, isforever)
    int32_t  value;    // We'll interpret as bool on the other side
} MotorDataValueUpdate;
#pragma pack(pop)

typedef enum Stepper
{
    A = 0,
    X,
    Y,
    Z,
    B,
    C,
    D,
    E,
    F,
    G
} Stepper;

// C replacement for the C++ default member initializers removed above.
// Call after every zero-init / fresh MotorData so runtime defaults match
// the old C++ semantics (qid=-1, isEnable=true, stopped=true, ...).
static inline void motor_data_defaults(MotorData *d)
{
    d->qid = -1;
    d->stopped = true;
    d->isEnable = true;
    d->joystickSpeedMultiplier = 1.0f;
    d->maxspeed = 200000;
    d->triggerPeriod = -1;
    d->triggerPin = -1;
    d->dirPin = -1;
    d->stpPin = -1;
}

static inline void motor_settings_defaults(MotorSettings *s)
{
    s->isEnable = true;
    s->maxspeed = 200000;
    s->triggerPeriod = -1;
    s->triggerPin = -1;
    s->dirPin = -1;
    s->stpPin = -1;
}
