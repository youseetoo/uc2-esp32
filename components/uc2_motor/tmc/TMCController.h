// T4.1: pure C header (was a C++ namespace). The TMC2209Stepper vendor
// object stays in the TMCController.cpp C++ island; this header only
// declares its extern-C surface.
#pragma once
#include <PinConfig.h>
#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"
// T4.1: uc2_hal.h / uc2_hal/uc2_wire.h deliberately NOT included here — they
// are C++ (class-based) and this header is pulled into C TUs (axis_motor_c.c).
// The only C++ consumer (TMCController.cpp) includes what it needs itself.
#include "JsonKeys.h"
#include "cJsonTool.h"
#include "PinConfig.h"
#include "FocusMotor.h"

#define DRIVER_ADDRESS 0b00
#define R_SENSE 0.2f

// Internal oscillator of the TMC2209 (typ. 12 MHz). Used to convert a
// velocity in steps/s into the TSTEP-domain thresholds (TPWMTHRS/TCOOLTHRS).
#define TMC_FCLK_HZ 12000000UL

struct TMCData
{
    uint16_t msteps;
    uint16_t rms_current;
    uint16_t stall_value;
    uint16_t sgthrs;
    uint8_t semin;
    uint8_t semax;
    uint8_t sedn;
    uint32_t tcoolthrs;
    uint8_t blank_time;
    uint8_t toff;
    // Velocity (in steps/s at the *configured* microstep resolution) above
    // which the driver leaves the quiet StealthChop mode for the high-torque
    // SpreadCycle chopper. 0 = never switch (StealthChop at all velocities,
    // which is the TMC2209 power-on default).
    uint32_t tpwmthrs_sps;
    // 1 = run SpreadCycle at *every* velocity (maximum torque, loudest).
    // Overrides tpwmthrs_sps, which the driver then ignores.
    uint8_t en_spreadcycle;
    // Standstill current as a percentage of the run current. The TMCStepper
    // library defaults to 50 %; raise it towards 100 for maximum holding
    // torque at the cost of heat.
    uint8_t hold_mult_pct;
    // SpreadCycle chopper hysteresis (raw CHOPCONF fields). Only used in
    // SpreadCycle; irrelevant while StealthChop runs. 5 / 0 reproduces the
    // TMC2209 reset default.
    uint8_t hstrt;
    uint8_t hend;
};

typedef struct TMCData TMCData;

// Bump when a pinConfig default changes in a way that must reach boards which
// already have the old value persisted in NVS — tmc_read_params()
// otherwise keeps handing back the stale stored value forever.
#define TMC_SETTINGS_VERSION 2

#ifdef __cplusplus
extern "C" {
#endif

void tmc_setup(void);
void tmc_loop(void);
int tmc_act(cJSON *jsonDocument);
cJSON *tmc_get(cJSON *jsonDocument);
// Apply p to the driver (and persist to NVS when saveToPrefs).
void tmc_apply_params(const TMCData *p, bool saveToPrefs);
// Currently-applied microstep setting (persisted value; falls back to the
// PinConfig default). Used by AxisCalibration to tag calibration records.
uint16_t tmc_get_microsteps(void);
uint16_t tmc_get_current(void);
void tmc_set_current(uint16_t current);
void tmc_calibrate_stallguard(int speed);

// Convert a velocity in steps/s (at the given microstep resolution) into
// the TSTEP-domain register value used by TPWMTHRS / TCOOLTHRS.
// TSTEP counts f_CLK ticks between two 1/256 microsteps, so
//   TSTEP = f_CLK * microsteps / (256 * steps_per_second)
// Returns 0 for sps == 0 ("never switch"), and is clamped to the 20-bit
// register width.
uint32_t tmc_tstep_from_sps(uint32_t stepsPerSecond, uint16_t microsteps);

#ifdef __cplusplus
}
#endif
