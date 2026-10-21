#pragma once
// Tmp102Controller (Phase 4: pure C)
// ---------------------------------------------------------------------------
// Drives the two TMP102AIDRLR temperature sensors on the CAN-HAT v2:
//   * 0x4A -> PCB temperature (HAT itself)
//   * 0x4B -> ambient air temperature inside the electronics box
//
// Also exposes the ESP32 internal temperature as a fallback / sanity check.
// The TMP102 is auto-converting after a single config write enabling the
// default 4 Hz, 12-bit continuous mode (POR default already does this, but
// we touch the config register explicitly so a misconfigured sensor recovers
// after reset).
//
// Endpoints:
//   /temp_get -> { "temp": { "pcb": 42.5, "air": 31.2, "esp": 51.0,
//                            "pcb_ok": true, "air_ok": true } }
//
// All temperatures are degrees Celsius. A sensor that fails to respond on
// I2C is reported as NaN with the matching *_ok flag set to false; the
// FanController uses these flags to fall back to the ESP internal sensor.
#include <stdbool.h>
#include <stdint.h>
#include "PinConfigDefault.h"
#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

#define TMP102_ADDR_PCB 0x4A  // I2C addr - PCB sensor
#define TMP102_ADDR_AIR 0x4B  // I2C addr - air-temperature sensor

void   tmp102_setup(void);
void   tmp102_loop(void);

// Last cached reading. Returns NAN if the sensor wasn't reachable.
float  tmp102_get_pcb_c(void);
float  tmp102_get_air_c(void);
float  tmp102_get_esp_c(void);

// Worst-case (max) of pcb/air/esp readings — used by the fan curve.
float  tmp102_get_max_ambient_c(void);

int    tmp102_act(cJSON *root);
cJSON *tmp102_get(cJSON *root);

#ifdef __cplusplus
}  // extern "C"
#endif
