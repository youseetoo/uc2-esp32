// T6.6: UC2 CANopen slave LED (XIAO ESP32-S3) — board pin table, pure C.
//
// Starts from PINCONFIG_DEFAULTS_FIELDS (all pins disabled / safe defaults)
// and overrides only the fields this board wires up. Former C++ derived struct
// `UC2_canopen_slave_led : PinConfig` (lived in PinConfig.h).
//
// UC2 Illumination Board: NeoPixel ring stack (4 concentric rings, 136 LEDs
// total) + single high-power white LED driven by PWM (exposed as logical laser
// 0) + heat-sink thermistors (rev. H).
#include "pinconfig_defaults.h"

const struct PinConfig pinConfig = {
    PINCONFIG_DEFAULTS_FIELDS
    /* ── board overrides (after the defaults — later designators win) ── */
    .pindefName = "UC2_canopen_slave_led",
    .BAUDRATE = 115200,

    /* Heat-sink thermistors (rev. H): four NCP15XH103F03RC 10k NTCs, each
       wired 3V3 -> NTC -> node -> 1k -> GND and buffered by an LM324 */
    .thermal_PIN_0 = GPIO_NUM_2,   // D1  – temperature-sensor_1
    .thermal_PIN_1 = GPIO_NUM_9,   // D10 – temperature-sensor_2
    .thermal_PIN_2 = GPIO_NUM_8,   // D9  – temperature-sensor_3
    .thermal_PIN_3 = GPIO_NUM_7,   // D8  – temperature-sensor_4
    .thermal_enabled_default = true,

    /* NeoPixel ring stack */
    .LED_PIN = GPIO_NUM_3,         // D2 – RGB ring data
    .LED_COUNT = 136,              // RING_OUTEST_START + RING_OUTEST_COUNT

    /* White LED PWM (single channel, exposed as logical laser 0) */
    .LASER_0 = GPIO_NUM_4,         // D3 – white LED PWM
    .testLaserPinOnBoot = true,
    .LASER_PWM_FREQUENCY = 10000,  // Hz (10 kHz to avoid flicker)
    .LASER_PWM_RESOLUTION = 11,    // bits (2048 steps)

    /* CAN bus */
    .CAN_TX = GPIO_NUM_5,          // D4
    .CAN_RX = GPIO_NUM_6,          // D5
    .CAN_ID_CURRENT = CAN_ID_LED_0, // default 30

    /* Route overrides: make explicit so inferred behaviour cannot accidentally
       turn locally driven peripherals OFF on a CAN_SLAVE node */
    .ROUTE_LED = 0,                // LED ring: LOCAL
    .ROUTE_LASER = {0, 2, 2, 2},   // ch0 LOCAL (PWM), ch1..3 OFF
    .ROUTE_LASER_4 = 2,            // no extra laser on this board
    .ROUTE_MOTOR = {2, 2, 2, 2},
    .ROUTE_HOME  = {2, 2, 2, 2},
    .ROUTE_TMC   = {2, 2, 2, 2},
    .ROUTE_GALVO = 2,

    /* I2C — routed on rev. H but left disabled until something on the bus is
       actually driven from firmware */
    .I2C_SCL = GPIO_NUM_NC,
    .I2C_SDA = GPIO_NUM_NC,

    /* Fan — tacho feedback from the fan socket */
    .FAN_TACHO_PIN = GPIO_NUM_44,  // D7
};
