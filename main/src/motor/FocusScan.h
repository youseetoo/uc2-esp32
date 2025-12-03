// FocusScan.h
#pragma once
#include "FocusMotor.h"
#include "../i2c/tca_controller.h"
#ifdef LED_CONTROLLER
#include "../led/LedController.h"
#endif
#ifdef LASER_CONTROLLER
#include "../laser/LaserController.h"
#endif
#ifdef CAN_BUS_ENABLED
#include "../can/can_controller.h"
#endif

namespace FocusScan
{
struct FocusScanningData
{
    int32_t zStart       = 0;
    int32_t zStep        = 0;
    uint16_t nZ          = 0;

    uint32_t delayTimePreTrigger  = 0;   // ms
    uint32_t delayTimeTrigger     = 0;   // µs
    uint32_t delayTimePostTrigger = 0;   // ms

    int32_t speed        = 20000;        // steps /s
    int32_t acceleration = 1000000;      // steps /s²

    uint32_t qid         = 0;

    uint8_t ledarrayIntensity         = 0;      // 0-255
    uint8_t lightsourceIntensities[4] = {0};    // up to four lasers

    volatile bool stopped = false;
};

extern FocusScanningData focusScanningData;
extern volatile bool     isRunning;

FocusScanningData *getFocusScanData();
void                focusScanThread(void *arg);
void                focusScanCAN(bool isThread);

} // namespace FocusScan
