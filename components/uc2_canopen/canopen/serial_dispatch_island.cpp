// serial_dispatch_island.cpp — C++ island for the still-C++ bridges (T4.4 4g).
//
// serial_dispatch.c (C since T4.4 4g) cannot call the PtzKeyboard /
// JoystickRouter C++ namespaces directly. These extern "C" shims are the
// bridge: the C handlers call them, they forward to the C++ bridges.
// Compiles to an empty object on apps without any of the bridges (same
// pattern as canopen_tpdo_galvo.cpp).
//
// T4.4 4i: PtzKeyboard is now C; the C handlers call ptz_keyboard_act/get
// directly. Only the JoystickRouter shims remain. Drop the whole file once
// the joystick bridge is C-ified.
#include <cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef JOYSTICK_USBHOST_PROVIDER
#include "JoystickRouter.h"
// T4.4 4f: C shim for the still-C++ joystick bridge — drop once joystick is C.
cJSON *device_router_joystick_act(cJSON *doc) { return JoystickRouter::act(doc); }
cJSON *device_router_joystick_get(cJSON *doc) { return JoystickRouter::get(doc); }
#endif

#ifdef __cplusplus
}  // extern "C"
#endif
