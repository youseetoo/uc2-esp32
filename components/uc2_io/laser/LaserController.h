// Laser PWM controller (T4.1: pure C, was C++ namespace LaserController).
// API: laser_* functions; state arrays exposed for read/update by other
// components (was namespace statics + static int& back-refs).
#pragma once

#include <stdbool.h>
#include <stdint.h>

#include "cJSON.h"
// T1.3: raw-IDF consumers get the uc2 façade via PinConfig.h (uc2_hal.h).
#include "PinConfig.h"

#ifdef __cplusplus
extern "C" {
#endif

// LASER IDs 0-4 local; 5-9 reserved for hybrid CAN mode.
#define LASER_MAX_LASERS (5 + 5)

// Laser state snapshot (was struct LaserData with C++ default inits).
struct LaserData
{
    int LASERid;
    int LASERval;
    int LASERdespeckle;
    int LASERdespecklePeriod;
};
typedef struct LaserData LaserData;

// Dpad directions for laser_dpad_changed_event — values mirror
// Dpad::Direction (HidGamePad.h): up=0, right=2, down=4, left=6, none=15.
#define LASER_DPAD_UP   0
#define LASER_DPAD_RIGHT 2
#define LASER_DPAD_DOWN 4
#define LASER_DPAD_LEFT 6
#define LASER_DPAD_NONE 15

// Arrays indexed by LASERid (was namespace statics).
extern int laser_val_arr[LASER_MAX_LASERS];
extern int laser_despeckle_arr[LASER_MAX_LASERS];
extern int laser_despeckle_period_arr[LASER_MAX_LASERS];
// PWM channel mapping (was PWM_CHANNEL_LASER[] + PWM_CHANNEL_LASER_n back-refs).
extern int laser_pwm_channel[LASER_MAX_LASERS];

// Helper functions to get pin and channel for a given LASERid.
int laser_get_pin(int laser_id);
int laser_get_pwm_channel(int laser_id);

// Main API functions.
int laser_get_val(int laser_id);
bool laser_set_val(int laser_id, int laser_val); // current despeckle, qid 0
bool laser_set_val_full(int laser_id, int laser_val, int laser_despeckle,
                        int laser_despeckle_period, int qid);

void laser_despeckle(int laser_despeckle, int laser_id, int laser_period);
int laser_act(cJSON *ob);
void laser_configure_pwm(int servo_pin, int resolution, int led_channel, int frequency);
void laser_move_servo(int led_channel, int angle, int frequency, int resolution);

cJSON *laser_get(cJSON *ob);
void laser_init(void);
void laser_loop(void);
void laser_set_pwm(int pwm_value, int pwm_channel);
void laser_setup_channel(int laser_pin, int pwm_chan, int pwm_freq, int pwm_res);
void laser_apply_value(const LaserData *laser_data);

LaserData laser_get_data(void);

void laser_dpad_changed_event(int pressed);
void laser_process_hold_actions(void);
void laser_handle_short_click(int direction);
void laser_execute_hold_action(int direction);
void laser_cross_changed_event(int pressed); // Cross button: Laser 4 toggle

// Send laser value update after setting new value (similar to sendMotorPos).
void laser_send_value(int laser_id, int qid);

#ifdef __cplusplus
}
#endif
