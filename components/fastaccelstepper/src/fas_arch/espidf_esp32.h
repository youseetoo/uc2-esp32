#ifndef FAS_ARCH_ESPIDF_ESP32_H
#define FAS_ARCH_ESPIDF_ESP32_H

// esp32 specific includes
#include <math.h>

// on espidf need to use portDISABLE/ENABLE_INTERRUPTS
#include <freertos/FreeRTOS.h>
#include <freertos/task.h>
#define fasDisableInterrupts portDISABLE_INTERRUPTS
#define fasEnableInterrupts portENABLE_INTERRUPTS

// Arduino-like GPIO (LOW/HIGH/OUTPUT + pinMode/digitalWrite/digitalRead): use
// the project's uc2 HAL (uc2_gpio.h) rather than FAS's own macro shim, which
// redefined these and clashed with uc2_hal when both are in one TU (D2: FAS is
// a C++ leaf that rides on the shared HAL).
#include <uc2_hal/uc2_gpio.h>

#include "fas_arch/common_esp32.h"

#endif /* FAS_ARCH_ESPIDF_ESP32_H */
