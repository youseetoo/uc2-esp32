// PtzRouter.h — PTZ-keyboard → CANopen dispatcher.
//
// Consumes PtzKeyboard's Motion snapshot and Event queue and drives the bus
// exactly like JoystickRouter does for the DS4 bridge:
//
//   joystick pan  (speed 0..63) → X axis forever-move, proportional speed
//   joystick tilt (speed 0..63) → Y axis forever-move
//   TELE / WIDE   (zoom rocker) → Z axis forever-move, fixed speed
//   NEAR / FAR    (focus keys)  → A axis forever-move, fixed speed
//   stop frame                  → stop every running axis
//
// Discrete keys (preset call/set/clear, AUX 1..6 on/off, iris OPEN/CLOSE)
// are NOT handled locally — they are pushed upstream to the CAN master via
// a TPDO2 event frame which the master turns into a {"ptz":{...}} serial JSON
// line for UC2-REST / ImSwitch.
//
// A motion watchdog (ptz_keyboard_motion_timeout_ms, default 2s, 0=off)
// stops all axes if the keyboard goes silent while an axis is running.
//
// T4.4 4i: C-ified (was namespace PtzRouter).

#ifndef PTZ_ROUTER_H
#define PTZ_ROUTER_H

#ifdef PTZ_KEYBOARD_CONTROLLER

#include <stdbool.h>

bool ptz_router_begin(void);

#endif // PTZ_KEYBOARD_CONTROLLER
#endif // PTZ_ROUTER_H