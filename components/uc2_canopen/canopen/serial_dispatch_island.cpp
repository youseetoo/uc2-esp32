// serial_dispatch_island.cpp — C++ island for the still-C++ bridges (T4.4 4g).
//
// serial_dispatch.c (C since T4.4 4g) cannot call the GpioCanSlave /
// PtzKeyboard / JoystickRouter C++ namespaces directly. These extern "C"
// shims are the bridge: the C handlers call them, they forward to the C++
// bridges. Compiles to an empty object on apps without any of the three
// bridges (same pattern as canopen_tpdo_galvo.cpp).
//
// Drop the whole file once GpioCanSlave (4h) and the ptz trio (4i) are
// C-ified and the C handlers call the bridges' C APIs directly.
#include <cJSON.h>

#ifdef __cplusplus
extern "C" {
#endif

#ifdef GPIO_CAN_SLAVE_CONTROLLER
#include "GpioCanSlave.h"
// LOCAL /gpio_act + /gpio_get dispatch (serial_dispatch.c gpio handlers).
cJSON *serial_dispatch_gpio_local_act(cJSON *doc) { return GpioCanSlave::act(doc); }
cJSON *serial_dispatch_gpio_local_get(cJSON *doc) { return GpioCanSlave::get(doc); }
#endif

#ifdef PTZ_KEYBOARD_CONTROLLER
#include "PtzKeyboard.h"
// T4.4 4f: C shims for the still-C++ PTZ bridge — device_router.c (C) cannot
// call the PtzKeyboard:: namespace directly. Drop once the ptz trio is
// C-ified (step 4i).
cJSON *device_router_ptz_act(cJSON *doc) { return PtzKeyboard::act(doc); }
cJSON *device_router_ptz_get(cJSON *doc) { return PtzKeyboard::get(doc); }
#endif

#ifdef JOYSTICK_USBHOST_PROVIDER
#include "JoystickRouter.h"
// T4.4 4f: C shims for the still-C++ joystick bridge — drop in 4i.
cJSON *device_router_joystick_act(cJSON *doc) { return JoystickRouter::act(doc); }
cJSON *device_router_joystick_get(cJSON *doc) { return JoystickRouter::get(doc); }
#endif

#ifdef __cplusplus
}  // extern "C"
#endif
