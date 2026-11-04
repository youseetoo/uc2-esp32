// serial_dispatch_island.cpp — C++ island for still-C++ bridges (T4.4 4g).
//
// serial_dispatch.c (C since T4.4 4g) cannot call C++ namespaces directly.
// extern "C" shims here bridge the C handlers to C++ bridge code.
//
// T4.4 step 4i: PtzKeyboard is now C (C handlers call ptz_keyboard_act/get).
// T4.4 step 4j: JoystickRouter is now C (C handlers call joystick_router_act/get).
//
// All C++ bridges have been C-ified; this file is intentionally empty.
// Kept as a build artifact for the T4.4 series (no need to reconfigure
// CMake when all island code is gone).