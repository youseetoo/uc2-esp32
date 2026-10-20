// T4.1: pure C (was the C++ FocusScan namespace).
#pragma once
#include <stdbool.h>
#include <stdint.h>
#include "FocusMotor.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct FocusScanningData
{
    int32_t zStart;
    int32_t zStep;
    uint16_t nZ;

    uint32_t delayTimePreTrigger;   // ms
    uint32_t delayTimeTrigger;      // us
    uint32_t delayTimePostTrigger;  // ms

    int32_t speed;        // steps /s
    int32_t acceleration; // steps /s^2

    uint32_t qid;

    uint8_t ledarrayIntensity;         // 0-255
    uint8_t lightsourceIntensities[4]; // up to four lasers

    volatile bool stopped;
} FocusScanningData;

extern FocusScanningData focus_scanning_data;
extern volatile bool     focus_scan_is_running;

FocusScanningData *focus_scan_get_data(void);
void               focus_scan_thread(void *arg);
void               focus_scan_can(bool isThread);

#ifdef LED_CONTROLLER
// C shim implemented by uc2_led (C-ported later in the T4.1 batch): execute a
// LedCommand in CIRCLE mode with r=g=b=rgb, given radius and qid. Keeps
// FocusScan.c free of the C++ LedController.h (enum class + Adafruit).
void led_exec_circ_command(uint8_t rgb, uint8_t radius, uint8_t qid);
#endif

#ifdef LASER_CONTROLLER
// C shim over LaserController::setLaserVal(id, val, qid) (uc2_io, C-ported
// later in the T4.1 batch).
bool laser_set_val_c(int laser_id, int value, int qid);
#endif

#ifdef __cplusplus
}
#endif
