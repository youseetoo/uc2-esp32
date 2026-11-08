// T6.6: xiao_ledring — board pin table, pure C.
//
// Starts from PINCONFIG_DEFAULTS_FIELDS (all pins disabled / safe defaults)
// and overrides only the fields this board wires up. Former C++ derived struct
// `UC2_3_Xiao_Slave_Laser : PinConfig` (lived in PinConfig.h).
#include <driver/gpio.h>
#include "pinconfig_defaults.h"

const struct PinConfig pinConfig = {
    PINCONFIG_DEFAULTS_FIELDS
    /* ── board overrides (after the defaults — later designators win) ── */
    .pindefName = "UC2_3_I2CSlaveLaser",
    .BAUDRATE = 115200,

    /* Laser control pins (PWM for lasers) */
    .LASER_1 = GPIO_NUM_2,   // D1 (signal_1, Laser 0)
    .LASER_2 = GPIO_NUM_3,   // D2 (signal_2, Laser 1)
    .LASER_3 = GPIO_NUM_5,   // D4 (signal_3, Laser 2)
    .LASER_4 = GPIO_NUM_6,   // D5 (signal_4, Laser 3)

    /* Interlock status */
    .DIGITAL_IN_1 = GPIO_NUM_8,  // D9 (LO when interlock has power)
    .DIGITAL_IN_2 = GPIO_NUM_1,  // D0 (LO when interlock tripped, enable pullup)

    /* CAN communication */
    .CAN_TX = GPIO_NUM_4,   // D3 (CAN-SEND)
    .CAN_RX = GPIO_NUM_9,   // D10 (CAN-RECV)
    .CAN_ID_CURRENT = 20,  // CAN_ID_LASER_0 — broadcasting address for laser PWM control

    /* I2C — disabled in this setup */
    .I2C_SCL = GPIO_NUM_NC,
    .I2C_SDA = GPIO_NUM_NC,
};
