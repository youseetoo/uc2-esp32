// T4.1: pure C (was the C++ MotorJsonParser namespace).
#pragma once
#include <stdbool.h>
#include "cJSON.h"
#include "JsonKeys.h"

#ifdef __cplusplus
extern "C" {
#endif

int  motorjson_act(cJSON *doc);
cJSON *motorjson_get(cJSON *docin);

// Individual sub-parsers — called by device_router_handle_motor_act so that
// routed /motor_act commands still execute config side-effects (enable,
// setpos, hardlimits, joystickdir, ...) and stage scans. Previously these
// only ran when DeviceRouter returned nullptr (i.e. never for routed paths).
void motorjson_parse_enable_motor(cJSON *doc);
void motorjson_parse_auto_enable_motor(cJSON *doc);
bool motorjson_parse_set_position(cJSON *doc);
void motorjson_parse_motor_pin_direction(cJSON *doc);
void motorjson_parse_set_hard_limits(cJSON *doc);
void motorjson_parse_set_joystick_direction(cJSON *doc);
void motorjson_parse_set_joystick_speed(cJSON *doc);
void motorjson_parse_motor_drive_json(cJSON *doc);
#ifdef STAGE_SCAN
void motorjson_parse_stage_scan(cJSON *doc);
#endif

// Helper function to add JSON parameter if present
void motorjson_add_json_float_if_present(cJSON *source, cJSON *target, const char *key);
void motorjson_add_json_int_if_present(cJSON *source, cJSON *target, const char *key);

#ifdef __cplusplus
}
#endif
