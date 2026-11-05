#pragma once

// T4.5: C API for the BT controller layer. Replaces the BtController C++ namespace.
// The PSController path remains C++ (external library); this header provides C-callable
// wrappers for the JSON-dispatched methods (/bt_connect, /bt_scan, /bt_disconnect).

#include <stdbool.h>
#include <stdint.h>
#include "cJSON.h"
#include "HidGamePad.h"

#ifdef __cplusplus
extern "C" {
#endif

// BT controller lifecycle
void bt_setup(void);
void bt_loop(void);
void btControllerLoop(void *p);  // FreeRTOS task wrapper (calls bt_loop in a loop)

// JSON-dispatched methods (C API, called from SerialProcessIn.c)
void bt_setMacAndConnect(char *m);
#ifdef PSXCONTROLLER
void bt_connectPsxController(char *mac, int type);
#endif
cJSON *bt_scanForDevices(cJSON *doc);
void bt_disconnect(void);

// Event handler function pointer types
typedef void (*bt_button_changed_cb)(int pressed);
typedef void (*bt_dpad_changed_cb)(DpadDirection pressed);
typedef void (*bt_xyza_changed_cb)(int x, int y, int z, int a);
typedef void (*bt_analog_controller_cb)(int left, int right, bool r1, bool r2, bool l1, bool l2);

// PS4 trackpad event types
typedef enum {
    SWIPE_NONE = 0,
    SWIPE_LEFT = 1,
    SWIPE_RIGHT = 2,
    SWIPE_UP = 3,
    SWIPE_DOWN = 4
} SwipeDirection;

typedef struct {
    bool isActive;
    uint16_t x;
    uint16_t y;
    uint8_t id;
} TouchData;

typedef struct {
    uint8_t reportCounter;
    TouchData touch1;
    TouchData touch2;
} TrackpadData;

typedef void (*bt_trackpad_swipe_cb)(SwipeDirection direction);
typedef void (*bt_trackpad_touch_cb)(TouchData touch1, TouchData touch2);

// Event handler registration
void bt_setCrossChangedEvent(bt_button_changed_cb cb);
void bt_setShareChangedEvent(bt_button_changed_cb cb);
void bt_setOptionsChangedEvent(bt_button_changed_cb cb);
void bt_setCircleChangedEvent(bt_button_changed_cb cb);
void bt_setTriangleChangedEvent(bt_button_changed_cb cb);
void bt_setSquareChangedEvent(bt_button_changed_cb cb);
void bt_setDpadChangedEvent(bt_dpad_changed_cb cb);
void bt_setXYZAChangedEvent(bt_xyza_changed_cb cb);
void bt_setAnalogControllerChangedEvent(bt_analog_controller_cb cb);
void bt_setTrackpadSwipeEvent(bt_trackpad_swipe_cb cb);
void bt_setTrackpadTouchEvent(bt_trackpad_touch_cb cb);

#ifdef __cplusplus
}
#endif
