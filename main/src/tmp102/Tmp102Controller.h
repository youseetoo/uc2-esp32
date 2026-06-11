#pragma once
#include <PinConfig.h>
#include "cJSON.h"

// Tmp102Controller
// ---------------------------------------------------------------------------
// Drives the two TMP102AIDRLR temperature sensors on the CAN-HAT v2:
//   * 0x4A → PCB temperature (HAT itself)
//   * 0x4B → ambient air temperature inside the electronics box
//
// Also exposes the ESP32 internal temperature as a fallback / sanity check.
// The TMP102 is auto-converting after a single config write enabling the
// default 4 Hz, 12-bit continuous mode (POR default already does this, but
// we touch the config register explicitly so a misconfigured sensor recovers
// after reset).
//
// Endpoints:
//   /temp_get → { "temp": { "pcb": 42.5, "air": 31.2, "esp": 51.0,
//                            "pcb_ok": true, "air_ok": true } }
//
// All temperatures are degrees Celsius. A sensor that fails to respond on
// I2C is reported as NaN with the matching *_ok flag set to false; the
// FanController uses these flags to fall back to the ESP internal sensor.
namespace Tmp102Controller
{
    static constexpr uint8_t ADDR_PCB = 0x4A; // I2C addr - PCB sensor
    static constexpr uint8_t ADDR_AIR = 0x4B; // I2C addr - air-temperature sensor

    void setup();
    void loop();

    // Last cached reading. Returns NAN if the sensor wasn't reachable.
    float getPcbC();
    float getAirC();
    float getEspC();

    // Worst-case (max) of pcb/air readings — used by the fan curve.
    float getMaxAmbientC();

    int    act(cJSON *root);
    cJSON *get(cJSON *root);
}
