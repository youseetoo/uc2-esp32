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
// a TPDO2 event frame (payload documented in emitEventTpdo, PtzRouter.cpp)
// which the master turns into a {"ptz":{...}} serial JSON line for
// UC2-REST / ImSwitch (→ snap image, toggle laser, whatever the host maps).
//
// A motion watchdog (PtzKeyboard::motionTimeoutMs, default 2 s, 0 = off)
// stops all axes if the keyboard goes silent while an axis is running, so a
// lost stop-frame cannot leave the stage crawling into an endstop.
#pragma once

#ifdef PTZ_KEYBOARD_CONTROLLER

namespace PtzRouter
{
    // Spawn the consumer task. Must run after CANopenModule::setup() and
    // UC2::RoutingTable::buildDefault() so REMOTE routes are available.
    bool begin();
}

#endif // PTZ_KEYBOARD_CONTROLLER
