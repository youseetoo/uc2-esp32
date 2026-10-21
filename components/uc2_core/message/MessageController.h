// T4.1: pure C (was the C++ namespace MessageController).
#pragma once
#include <PinConfig.h>
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

int message_act(cJSON *ob);
cJSON *message_get(cJSON *ob);
void message_send_serial(int key, int value);
void message_setup(void);
void message_loop(void); // Called periodically to check for long-press timeouts

// Start homing with parameters stored in preferences
void message_start_homing_with_stored_params(int axis);

// Button event handlers - short press for normal action, long press (5s) for homing
void message_triangle_changed_event(int pressed);  // Axis A (short: release hard limit, long: home A)
void message_cross_changed_event(int pressed);     // Axis X (short: normal, long: home X)
void message_circle_changed_event(int pressed);    // Axis Y (short: normal, long: home Y)
void message_square_changed_event(int pressed);    // Axis Z (short: normal, long: home Z)

#ifdef __cplusplus
}
#endif
