#pragma once

class IMotorBackend;

// Factory: creates the right motor backend based on runtimeConfig + pinConfig.
// This is the SINGLE place where the compile-time-vs-runtime, local-vs-remote
// routing decision lives.
IMotorBackend* createMotorBackend();
