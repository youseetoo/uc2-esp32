/**
 * CanOpenMaster.h
 *
 * Master-side CANopen-Lite entry points (setup / loop).
 * Called from main.cpp when UC2_CANOPEN_ENABLED && UC2_CANOPEN_MASTER.
 */
#pragma once

namespace CanOpenMaster {

// Initialize CanOpenStack (master role) + DeviceRouter.
// Must be called after local device controllers (FocusMotor, etc.) are set up.
void setup();

// Tick the CANopen stack + DeviceRouter (heartbeats, NMT, SDO timeouts).
void loop();

} // namespace CanOpenMaster
