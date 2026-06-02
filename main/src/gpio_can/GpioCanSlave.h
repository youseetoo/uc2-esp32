// GpioCanSlave.h — small CANopen slave that publishes GPIO + analog state
// to TPDO2 and consumes remote digital-output commands from x2301.
//
// Compiled only when GPIO_CAN_SLAVE_CONTROLLER is defined (see
// main/config/UC2_canopen_slave_gpio/PinConfig.h).
//
// State mirrored into the OD:
//   OD_RAM.x2300_digital_input_state[0]  = E-stop pin level        (bit 0)
//   OD_RAM.x2300_digital_input_state[1]  = DIGITAL_IN_2 level      (bit 0)  [if wired]
//   OD_RAM.x2300_digital_input_state[2]  = DIGITAL_IN_3 level      (bit 0)  [if wired]
//   OD_RAM.x2300_digital_input_state[3]  = status flags byte
//                                            bit 0 = collision-trip
//                                            bit 1 = E-stop active
//                                            bit 2..7 reserved
//   OD_RAM.x2310_analog_input_value[0]   = collision ADC, filtered (u16)
//   OD_RAM.x2310_analog_input_value[1]   = collision ADC, raw      (u16)
//
// State consumed from the OD (master writes these via SDO/RPDO):
//   OD_RAM.x2301_digital_output_command[0]  -> DigitalOutController id 1 -> DIGITAL_OUT_1
//   OD_RAM.x2301_digital_output_command[1]  -> DigitalOutController id 2 -> DIGITAL_OUT_2
//
// The collision-threshold is held in NVS preferences (key "gpioCollThr")
// and may be changed at runtime via setThreshold().

#pragma once
#include <stdint.h>
#include "cJSON.h"

namespace GpioCanSlave
{
    void setup();
    void loop();

    uint16_t getThreshold();
    void     setThreshold(uint16_t value);

    // Optional JSON entry-point (mirrors the other controllers) for
    // SerialProcess to expose /gpio_cfg over the slave's own serial port.
    int     act(cJSON* doc);
    cJSON*  get(cJSON* doc);
}
