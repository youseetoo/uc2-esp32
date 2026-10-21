// T4.1: pure C (was the C++ namespace MessageController).
// Serial output goes through the framed C path (serial_out_safe_serialize_json);
// stored home parameters are read straight from NVS (was uc2::Preferences).
#include <stdio.h>
#include <PinConfig.h>

#include "MessageController.h"
#include <uc2_hal.h>
#include "cJsonTool.h"
#include "DeviceRouter.h"          // device_router_handle_home_act (C shim)
#include "uc2_serial_out.h"        // serial_out_safe_serialize_json
#include "nvs.h"
#ifdef MOTOR_CONTROLLER
#include "FocusMotor.h"
#include "MotorTypes.h"
#endif
#ifdef HOME_MOTOR
#include "HomeMotor.h"
#endif

static const char *TAG = "MessageController";

// Read-only NVS int with default (was uc2::Preferences getInt).
static int prefs_get_int(const char *ns, const char *key, int defaultVal)
{
    nvs_handle_t h;
    if (nvs_open(ns, NVS_READONLY, &h) != ESP_OK)
        return defaultVal;
    int32_t v = defaultVal;
    if (nvs_get_i32(h, key, &v) != ESP_OK)
        v = defaultVal;
    nvs_close(h);
    return (int)v;
}

void message_setup(void)
{
    log_d("Setup MessageController");
}

// Helper function to start homing with stored preferences
void message_start_homing_with_stored_params(int axis)
{
#ifdef HOME_MOTOR
    // Read stored homing parameters from preferences
    char key[16];
    snprintf(key, sizeof key, "to_%d", axis);
    int homeTimeout = prefs_get_int("home", key, 20000); // default 20s timeout
    snprintf(key, sizeof key, "hs_%d", axis);
    int homeSpeed = prefs_get_int("home", key, 5000);    // default speed
    snprintf(key, sizeof key, "hms_%d", axis);
    int homeMaxspeed = prefs_get_int("home", key, 5000); // default maxspeed
    snprintf(key, sizeof key, "hd_%d", axis);
    int homeDirection = prefs_get_int("home", key, 1);    // default direction
    snprintf(key, sizeof key, "hep_%d", axis);
    int homeEndStopPolarity = prefs_get_int("home", key, 0); // default polarity

    log_i("Starting homing for axis %d with stored params: timeout=%d, speed=%d, maxspeed=%d, dir=%d, polarity=%d",
          axis, homeTimeout, homeSpeed, homeMaxspeed, homeDirection, homeEndStopPolarity);

    // Route through DeviceRouter so CANopen REMOTE axes are forwarded to the correct slave node
    cJSON *doc = cJSON_CreateObject();
    cJSON *home = cJSON_CreateObject();
    cJSON_AddNumberToObject(home, "stepperid", axis);
    cJSON_AddNumberToObject(home, "speed", homeSpeed);
    cJSON_AddNumberToObject(home, "direction", homeDirection);
    cJSON_AddNumberToObject(home, "timeout", homeTimeout);
    cJSON_AddNumberToObject(home, "endstopRelease", 0);
    cJSON_AddNumberToObject(home, "endstopPolarity", homeEndStopPolarity);
    cJSON_AddItemToObject(doc, "home", home);
    cJSON *resp = device_router_handle_home_act(doc);
    if (resp) cJSON_Delete(resp);
    cJSON_Delete(doc);

    // Send feedback message (key=100+axis indicates homing started)
    message_send_serial(100 + axis, 1);
#else
    log_w("HOME_MOTOR not defined, cannot perform homing for axis %d", axis);
    (void)axis;
#endif
}

// Loop function - currently not used as long-press is handled in main.cpp
void message_loop(void)
{
    // Long-press handling is done in main.cpp checkBtButtonLongPress()
}

void message_triangle_changed_event(int pressed)
{
    // Triangle button: Short press = Emergency release of hard-limit blocked motors
    // Long press handling is done in main.cpp

    if (pressed)
    {
        // Perform emergency release of hard-limit blocked motors
#ifdef MOTOR_CONTROLLER
        log_i("Triangle pressed - Releasing all hard-limit blocked motors");
        int releasedCount = 0;

        // Check all motor axes and release any that are blocked
        for (int axis = 0; axis < MOTOR_AXIS_COUNT; axis++)
        {
            if (focusmotor_get_data()[axis]->hardLimitTriggered)
            {
                log_i("Releasing hard-limit on axis %d via homing reset", axis);

#ifdef HOME_MOTOR
                // Use homing event with timeout=0 to clear hard limit
                home_start(axis, 1, 0, 0, 1, 0, 0, -99);
                home_get_data()[axis]->homeIsActive = false;
                focusmotor_get_data()[axis]->isHoming = false;
#endif

                // Clear local state
                focusmotor_clear_hard_limit_triggered(axis);
                focusmotor_set_position((Stepper)axis, 0);
                focusmotor_send_motor_pos(axis, 0, -1);
                releasedCount++;
            }
        }

        if (releasedCount > 0)
        {
            message_send_serial(99, releasedCount);
            log_i("Released %d blocked motor(s)", releasedCount);
        }
#endif

        // Send message for backwards compatibility
        message_send_serial(1, 1);
    }
}

void message_cross_changed_event(int pressed)
{
    // Cross button: Short press action
    // Long press handling is done in main.cpp
    if (pressed)
    {
        log_i("Cross short press");
        message_send_serial(2, 1);
    }
}

void message_circle_changed_event(int pressed)
{
    // Circle button: Short press action
    // Long press handling is done in main.cpp
    if (pressed)
    {
        log_i("Circle short press");
        message_send_serial(3, 1);
    }
}

void message_square_changed_event(int pressed)
{
    // Square button: Short press action
    // Long press handling is done in main.cpp
    if (pressed)
    {
        log_i("Square short press");
        message_send_serial(4, 1);
    }
}

// Custom function accessible by the API
int message_act(cJSON *ob)
{   // {"message":{"key":"message","value":2}} or
    // {"task":"/message_act", "message":{"key":"message","value":2}}
    /*
    data lookup:
    {
        key 	|	 meaning 		|	value
        --------------------------------
        1		|	snap image		|	0
        2		|	exposure time	|	0....1000000
        3		|	gain			|	0...100

    */
    log_i("act");
    cJSON *message = cJSON_GetObjectItem(ob, "message");
    int qid = 0;
    if (message != NULL) {
        qid = cjson_tool_get_int(message, "qid", 0);
        int key = cjson_tool_get_int(message, "key", 0);
        int value = cjson_tool_get_int(message, "value", 0);
        // now we print this data over serial
        message_send_serial(key, value);
    }
    return qid;
}

// Custom function accessible by the API
// returns json {"Message":{..}}  as qid
cJSON *message_get(cJSON *ob)
{
    (void)ob;
    cJSON *j = cJSON_CreateObject();
    cJSON *ld = cJSON_CreateObject();
    cJSON_AddItemToObject(j, "message", ld);
    cjson_tool_set_int(ld, "data", 1);
    return j;
}

// home done returns
// {"message":{...}} thats the qid
void message_send_serial(int key, int value)
{
    // send home done to client
    log_i("key: %i, value: %i", key, value);
    cJSON *json = cJSON_CreateObject();
    cJSON *message = cJSON_CreateObject();
    cJSON_AddItemToObject(json, "message", message);
    cJSON *jsonkey = cJSON_CreateNumber(key);
    cJSON *jsonvalue = cJSON_CreateNumber(value);
    cJSON_AddItemToObject(message, "key", jsonkey);
    cJSON_AddItemToObject(message, "data", jsonvalue);
    // Was: Serial.println("++"); cJSON_PrintUnformatted; Serial.println(ret);
    // free; Serial.println("--") — now the framed C output path.
    serial_out_safe_serialize_json(json);
    cJSON_Delete(json);
}
