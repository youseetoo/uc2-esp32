// GpioCanSlave.h — small CANopen slave that publishes GPIO + collision state
// to TPDO2 (edges only) and consumes remote digital-output commands and
// collision-detector configuration via SDO.
//
// Compiled only when GPIO_CAN_SLAVE_CONTROLLER is defined (see
// main/config/UC2_canopen_slave_gpio/PinConfig.h).
//
// ── Collision detection (baseline-deviation) ───────────────────────────────
// The sensor has an "idle" ADC value (the reference). A collision manifests
// as the value deviating from the reference — rising OR falling. Detection:
//   * fast EWMA filter over the raw ADC (smooths sample noise)
//   * a sample is "out-of-band" when |filtered - reference| > threshold
//   * `sensitivity` consecutive out-of-band samples → trip = 1
//   * `sensitivity` consecutive in-band samples     → trip = 0
// Single-sample spikes (ADC glitches) never reach the count and are ignored.
// A slow rolling mean of the sensor is kept for calibration/diagnostics; it
// freezes while a deviation is being counted or a trip is active so a crash
// cannot drag the baseline along.
//
// ── OD interface (all SDO-only; values never broadcast) ────────────────────
//   0x2330 collision_reference   u16 rw   idle baseline (0 = auto-seed at boot)
//   0x2331 collision_threshold   u16 rw   deviation band, ADC counts
//   0x2332 collision_sensitivity u8  rw   consecutive samples to trip/clear
//   0x2333 collision_command     u8  rw   1 = calibrate (reference := mean)
//   0x2334 collision_mean        u16 ro   slow rolling mean (poll on demand)
//   0x2310 sub1/sub2             u16 ro   fast-filtered / raw ADC (TPDO payload)
//   0x2300 sub1                  u8  ro   E-stop level; sub4 = flags byte
//                                          (bit0 = collision trip, bit1 = E-stop)
//   0x2301 sub1/sub2             u8  rw   remote digital outputs
//
// The trip EVENT is pushed immediately via TPDO2 (COB-ID 0x280 + nodeId);
// there is no periodic TPDO — node liveness comes from the CANopen heartbeat.
// Config values persist in NVS (gpioCollRef / gpioCollThr / gpioCollSens).

#pragma once
#include <stdint.h>
#include "cJSON.h"

namespace GpioCanSlave
{
    void setup();
    void loop();

    // Collision-detector configuration (persisted to NVS on change)
    uint8_t  getMode();                  // 0 = AUTO (adaptive), 1 = MANUAL
    void     setMode(uint8_t mode);
    uint16_t getReference();
    void     setReference(uint16_t value);
    uint16_t getThreshold();
    void     setThreshold(uint16_t value);
    uint8_t  getSensitivity();
    void     setSensitivity(uint8_t value);
    // reference := current rolling mean; returns the new reference (MANUAL)
    uint16_t calibrate();

    // JSON entry-points, dispatched by DeviceRouter on the slave itself:
    //   {"task":"/gpio_act", "threshold":150, "sensitivity":4,
    //    "reference":585, "calibrate":1, "qid":N}   (all fields optional)
    //   {"task":"/gpio_get"}
    cJSON*  act(cJSON* doc);
    cJSON*  get(cJSON* doc);
}
