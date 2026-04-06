/**
 * CANopenModule.h
 *
 * CANopenNode integration for UC2 — replaces can_transport for CANopen builds.
 * Bridges the CANopenNode Object Dictionary to existing UC2 Module controllers.
 *
 * Only compiled when CAN_CONTROLLER_CANOPEN is defined in the build flags.
 */
#pragma once

#include <Arduino.h>
#include <stdint.h>

#ifdef CAN_CONTROLLER_CANOPEN

extern "C" {
#include <CANopen.h>
#include "OD.h"
}

class CANopenModule {
public:
    void setup();   // Init TWAI + CANopenNode + start FreeRTOS tasks
    void loop();    // NMT state monitoring, LED indication

    // SDO access (used by master's serial->CAN bridge)
    static bool writeSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                         uint8_t* data, size_t dataSize);
    static bool readSDO(uint8_t nodeId, uint16_t index, uint8_t subIndex,
                        uint8_t* buf, size_t bufSize, size_t* readSize);

    // Check if CANopen stack is running
    static bool isOperational();

private:
    static void CAN_ctrl_task(void* arg);
    static void CO_main_task(void* arg);
    static void CO_tmr_task(void* arg);
    static void CO_interrupt_task(void* arg);

    // OD <-> Module sync (slave only, called from CO_tmr_task)
    static void syncRpdoToModules();   // OD_RAM -> Module.act()
    static void syncModulesToTpdo();   // Module.get() -> OD_RAM
};

// Global CANopenNode object (same pattern as MWE)
extern CO_t* CO;

#endif // CAN_CONTROLLER_CANOPEN
