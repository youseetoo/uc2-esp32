/**
 * CanOpenSlave.h
 *
 * Slave-side CANopen-Lite entry points (setup / loop).
 * Called from main.cpp when UC2_CANOPEN_ENABLED && UC2_CANOPEN_SLAVE.
 */
#pragma once

namespace CanOpenSlave {

// Initialize CanOpenStack (slave role) + SlaveController.
// Must be called after local device controllers (FocusMotor, etc.) are set up.
void setup();

// Tick the CANopen stack + SlaveController (heartbeats, TPDOs).
void loop();

} // namespace CanOpenSlave
