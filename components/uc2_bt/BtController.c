// T4.5: C implementation of the BT controller layer.
// Faithful port of BtController.cpp to C. PSController path is in a
// separate C++ shim (BtController_psx.cpp) to keep this file pure C.

#include <stdbool.h>
#include "BtController.h"
#include "esp_log.h"
#include "PinConfig.h"
#include "ConfigController.h"
#include "cJSON.h"
#include <stdlib.h>
#include <string.h>
#include <math.h>

static const char *TAG = "BtController";

// For host tests, undefine BTHID so we don't need ESP-IDF BT stack stubs.
// The BTHID code paths are tested on real hardware only.
#ifdef UNIT_TEST
#undef BTHID
#endif

#ifdef BTHID
#include "HidController.h"
#endif

// Event handler function pointers
// Event handler function pointers
// Declared in HidController.c
static bt_button_changed_cb cross_changed_event;
static bt_button_changed_cb share_changed_event;
static bt_button_changed_cb options_changed_event;
static bt_button_changed_cb circle_changed_event;
static bt_button_changed_cb triangle_changed_event;
static bt_button_changed_cb square_changed_event;
static bt_dpad_changed_cb dpad_changed_event;
static bt_xyza_changed_cb xyza_changed_event;
static bt_analog_controller_cb analogcontroller_event;
static bt_trackpad_swipe_cb trackpad_swipe_event;
static bt_trackpad_touch_cb trackpad_touch_event;

// Last gamepad data (for change detection)
static GamePadData lastData;

// PS4 trackpad swipe detection
static TrackpadData lastTrackpadData;
static TouchData swipeStartTouch;
static bool swipeInProgress = false;
static const uint16_t SWIPE_THRESHOLD = 50;
static const uint32_t SWIPE_TIMEOUT = 500;
static uint32_t swipeStartTime = 0;

static uint32_t updateRateMS = 100;
static uint32_t lastUpdate = 0;

// PSController path (PSXCONTROLLER) is in BtController_psx.cpp
// which provides these extern "C" functions:
#ifdef PSXCONTROLLER
extern bool psx_is_connected(void);
extern void psx_get_button_state(int *cross, int *circle, int *triangle, int *square,
                                 int *options, int *share, int *up, int *down, int *left, int *right,
                                 int *l1, int *l2, int *r1, int *r2);
extern void psx_get_analog_sticks(int *lx, int *ly, int *rx, int *ry);
extern void psx_set_player(int player);
extern void psx_set_rumble(float intensity, int duration);
extern void psx_disconnect(void);
extern bool psx_init(const char *mac, int type);
#endif

// FreeRTOS task wrapper
void btControllerLoop(void *p)
{
    for (;;)
    {
        bt_loop();
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}

void bt_setCrossChangedEvent(bt_button_changed_cb cb) { cross_changed_event = cb; }
void bt_setShareChangedEvent(bt_button_changed_cb cb) {
    log_i("setShareChangedEvent");
    share_changed_event = cb;
}
void bt_setOptionsChangedEvent(bt_button_changed_cb cb) {
    log_i("setOptionsChangedEvent");
    options_changed_event = cb;
}
void bt_setCircleChangedEvent(bt_button_changed_cb cb) { circle_changed_event = cb; }
void bt_setTriangleChangedEvent(bt_button_changed_cb cb) { triangle_changed_event = cb; }
void bt_setSquareChangedEvent(bt_button_changed_cb cb) { square_changed_event = cb; }
void bt_setDpadChangedEvent(bt_dpad_changed_cb cb) { dpad_changed_event = cb; }
void bt_setXYZAChangedEvent(bt_xyza_changed_cb cb) { xyza_changed_event = cb; }
void bt_setAnalogControllerChangedEvent(bt_analog_controller_cb cb) {
    analogcontroller_event = cb;
}
void bt_setTrackpadSwipeEvent(bt_trackpad_swipe_cb cb) {
    log_i("setTrackpadSwipeEvent");
    trackpad_swipe_event = cb;
}
void bt_setTrackpadTouchEvent(bt_trackpad_touch_cb cb) {
    log_i("setTrackpadTouchEvent");
    trackpad_touch_event = cb;
}

void bt_setup(void)
{
    // Get the bluetooth config
#ifdef PSXCONTROLLER
    char *m = config_psx_mac_get();
    int type = config_psx_type_get();

    if (m != NULL && pinConfig.PSX_MAC != NULL)
    {
        m = (char *)pinConfig.PSX_MAC;
        type = pinConfig.PSX_CONTROLLER_TYPE;
    }

    // If the mac is not empty, try to connect to the psx controller
    if (m != NULL)
    {
        log_i("Connecting to PSX controller");
        psx_init(m, type);
    }
#endif
#ifdef BTHID
    setupHidController();
#endif
    xTaskCreate(&btControllerLoop, "btController_task",
               pinConfig.BT_CONTROLLER_TASK_STACKSIZE,
               NULL, pinConfig.DEFAULT_TASK_PRIORITY, NULL);

    // Initialize update limit
    lastUpdate = millis();
}

static void processTrackpadData(TrackpadData currentData)
{
    // Call touch event handler if registered
    if (trackpad_touch_event != NULL)
    {
        trackpad_touch_event(currentData.touch1, currentData.touch2);
    }

    // Swipe detection logic
    if (trackpad_swipe_event != NULL)
    {
        // Check for new touch (start of potential swipe)
        if (!swipeInProgress && currentData.touch1.isActive && !lastTrackpadData.touch1.isActive)
        {
            swipeStartTouch = currentData.touch1;
            swipeInProgress = true;
            swipeStartTime = millis();
            log_d("Swipe started at (%d, %d)", swipeStartTouch.x, swipeStartTouch.y);
        }
        // Check for end of touch (end of potential swipe)
        else if (swipeInProgress && !currentData.touch1.isActive && lastTrackpadData.touch1.isActive)
        {
            uint32_t swipeDuration = millis() - swipeStartTime;

            if (swipeDuration < SWIPE_TIMEOUT)
            {
                // Calculate swipe distance and direction
                int16_t deltaX = lastTrackpadData.touch1.x - swipeStartTouch.x;
                int16_t deltaY = lastTrackpadData.touch1.y - swipeStartTouch.y;
                uint16_t distance = sqrt(deltaX * deltaX + deltaY * deltaY);

                if (distance > SWIPE_THRESHOLD)
                {
                    SwipeDirection direction = SWIPE_NONE;

                    // Determine primary swipe direction
                    if (abs(deltaX) > abs(deltaY))
                    {
                        // Horizontal swipe
                        direction = (deltaX > 0) ? SWIPE_RIGHT : SWIPE_LEFT;
                    }
                    else
                    {
                        // Vertical swipe
                        direction = (deltaY > 0) ? SWIPE_DOWN : SWIPE_UP;
                    }

                    log_i("Swipe detected: direction=%d, distance=%d, duration=%d ms",
                          direction, distance, swipeDuration);
                    trackpad_swipe_event(direction);
                }
            }

            swipeInProgress = false;
        }
        // Check for swipe timeout
        else if (swipeInProgress && (millis() - swipeStartTime) > SWIPE_TIMEOUT)
        {
            log_d("Swipe timeout");
            swipeInProgress = false;
        }
    }

    // Update last trackpad data
    lastTrackpadData = currentData;
}

#ifdef BTHID
void bt_loop(void)
{
    if (hidIsConnected && (millis() - updateRateMS) > lastUpdate)
    {
        // Check each button for state change and call event handler
        if (lastData.cross != gamePadData.cross)
        {
            lastData.cross = gamePadData.cross;
            if (cross_changed_event != NULL)
                cross_changed_event(gamePadData.cross);
        }
        if (lastData.circle != gamePadData.circle)
        {
            lastData.circle = gamePadData.circle;
            if (circle_changed_event != NULL)
                circle_changed_event(gamePadData.circle);
        }
        if (lastData.triangle != gamePadData.triangle)
        {
            lastData.triangle = gamePadData.triangle;
            if (triangle_changed_event != NULL)
                triangle_changed_event(gamePadData.triangle);
        }
        if (lastData.square != gamePadData.square)
        {
            lastData.square = gamePadData.square;
            if (square_changed_event != NULL)
                square_changed_event(gamePadData.square);
        }
        if (lastData.options != gamePadData.options)
        {
            lastData.options = gamePadData.options;
            if (options_changed_event != NULL)
                options_changed_event(gamePadData.options);
        }
        if (lastData.share != gamePadData.share)
        {
            lastData.share = gamePadData.share;
            if (share_changed_event != NULL)
                share_changed_event(gamePadData.share);
        }
        if (lastData.dpaddirection != gamePadData.dpaddirection)
        {
            lastData.dpaddirection = gamePadData.dpaddirection;
            if (dpad_changed_event != NULL)
                dpad_changed_event(lastData.dpaddirection);
        }

        if (xyza_changed_event != NULL)
            xyza_changed_event(gamePadData.RightX, gamePadData.RightY,
                              gamePadData.LeftY, gamePadData.LeftX);
        lastData.LeftX = gamePadData.LeftY;
        lastData.RightX = gamePadData.RightX;
        lastData.RightY = gamePadData.RightY;
        lastData.LeftY = gamePadData.LeftX;

        int left = gamePadData.dpaddirection == DIRECTION_LEFT;
        int right = gamePadData.dpaddirection == DIRECTION_RIGHT;
        bool r1 = gamePadData.r1;
        bool r2 = gamePadData.r2;
        bool l1 = gamePadData.l1;
        bool l2 = gamePadData.l2;
        if (analogcontroller_event != NULL)
            analogcontroller_event(left, right, r1, r2, l1, l2);

        lastUpdate = millis();
    }
    if (millis() - lastUpdate > updateRateMS && !hidIsConnected)
    {
        // Ensure all values are zero and we stop motors
        xyza_changed_event(0, 0, 0, 0);
    }
}
#endif

#ifdef PSXCONTROLLER
void bt_loop(void)
{
    if (psx_is_connected())
    {
        // Check each button for state change and call event handler
        int cross, circle, triangle, square, options, share;
        int up, down, left, right, l1, l2, r1, r2;
        psx_get_button_state(&cross, &circle, &triangle, &square, &options, &share,
                             &up, &down, &left, &right, &l1, &l2, &r1, &r2);

        if (lastData.cross != cross)
        {
            lastData.cross = cross;
            if (cross_changed_event != NULL)
                cross_changed_event(cross);
        }
        if (lastData.circle != circle)
        {
            lastData.circle = circle;
            if (circle_changed_event != NULL)
                circle_changed_event(circle);
        }
        if (lastData.triangle != triangle)
        {
            lastData.triangle = triangle;
            if (triangle_changed_event != NULL)
                triangle_changed_event(triangle);
        }
        if (lastData.square != square)
        {
            lastData.square = square;
            if (square_changed_event != NULL)
                square_changed_event(square);
        }
        if (lastData.options != options)
        {
            lastData.options = options;
            if (options_changed_event != NULL)
                options_changed_event(options);
        }
        if (lastData.share != share)
        {
            lastData.share = share;
            if (share_changed_event != NULL)
                share_changed_event(share);
        }

        DpadDirection dir = DIRECTION_NONE;
        if (up)
            dir = DIRECTION_UP;
        if (down)
            dir = DIRECTION_DOWN;
        if (left)
            dir = DIRECTION_LEFT;
        if (right)
            dir = DIRECTION_RIGHT;
        if (dir != lastData.dpaddirection)
        {
            lastData.dpaddirection = dir;
            if (dpad_changed_event != NULL)
                dpad_changed_event(dir);
        }

        int lx, ly, rx, ry;
        psx_get_analog_sticks(&lx, &ly, &rx, &ry);
        if (xyza_changed_event != NULL)
            xyza_changed_event(rx, ry, ly, lx);
        lastData.LeftX = ly;
        lastData.RightX = rx;
        lastData.RightY = ry;
        lastData.LeftY = lx;

        if (analogcontroller_event != NULL)
            analogcontroller_event(left, right, r1, r2, l1, l2);
    }
}
#endif

#if !defined(BTHID) && !defined(PSXCONTROLLER)
// No BT controller backend on this build (host tests, BT-less apps):
// btControllerLoop still calls bt_loop, so provide a no-op fallback.
void bt_loop(void)
{
}
#endif

cJSON *bt_scanForDevices(cJSON *doc)
{
    log_i("scanForDevices - this does not work on all ESP32 even if same fabricate and same version");
    log_i("scanForDevices - Also ensure that we have enough Heap memory. 39300 seems to be the minimum below which it doesn't work anymore");

#ifdef BTHID
    // Create a task for BT scanning to prevent blocking the serial task
    size_t free_heap = esp_get_free_heap_size();
    log_i("Free heap before BT scan: %d bytes", free_heap);

    log_i("Starting BT scan task");
    xTaskCreate(hid_demo_task, "hid_demo_task", 8192, (void *)1, 6, NULL);

    cJSON *root = cJSON_CreateObject();
    cJSON_AddStringToObject(root, "status", "scan_started");
    cJSON_AddStringToObject(root, "message", "Bluetooth device scan started in background");
    return root;
#else
    log_i("BT scanning not supported on this device");
#endif
    return NULL;
}

void bt_disconnect(void)
{
#ifdef BTHID
    // HID controllers don't have a simple disconnect function
    hidIsConnected = false;
    log_i("HID controller disconnect requested");
#endif
#ifdef PSXCONTROLLER
    psx_disconnect();
#endif
}

void bt_setMacAndConnect(char *m)
{
    // No-op for now
}
