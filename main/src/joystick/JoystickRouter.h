// JoystickRouter.h — Bridge-side gamepad → CANopen dispatcher.
//
// Consumes Ds4State from JoystickUsbHost queue, applies the gamepad mapping
// (sticks → motors, R1/R2/L1/L2 → single-step Z, Triangle → laser toggle,
// Square → LED pattern cycle), and emits expedited SDO writes directly via
// CANopenModule.
//
// The bridge build has none of MOTOR_CONTROLLER / LASER_CONTROLLER /
// LED_CONTROLLER defined, so we cannot reuse DeviceRouter::handle*Act. We
// mirror the REMOTE branches of those handlers (DeviceRouter.cpp:285-315,
// :475, :660-696) with direct CANopenModule::writeSDO_* calls.
#pragma once

#ifdef JOYSTICK_USBHOST_PROVIDER

namespace JoystickRouter
{
    // Spawn the consumer task. Must be called after CANopenModule::setup()
    // and UC2::RoutingTable::buildDefault() so routes are available.
    bool begin();
}

#endif // JOYSTICK_USBHOST_PROVIDER
