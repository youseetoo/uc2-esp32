#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

#define ESP32S3_MODEL_XIAO

// ── Controller modules enabled on this slave ───────────────────────────────
// The GPIO slave is intentionally minimal:
//   * DigitalInController       — E-stop and (optional) extra logic inputs
//   * DigitalOutController      — GPIO1 / GPIO4 driven remotely from the master
//   * AnalogInController        — GPIO9 collision sensor (resistance read)
//   * GpioCanSlave              — glue that mirrors all of the above into the
//                                 CANopen OD and pushes TPDO2 on changes.
#define DIGITAL_IN_CONTROLLER
#define DIGITAL_OUT_CONTROLLER
#define MESSAGE_CONTROLLER
#define CAN_BUS_ENABLED
#define CAN_CONTROLLER_CANOPEN
#define GPIO_CAN_SLAVE_CONTROLLER  // wires GpioCanSlave::setup/loop into main.cpp

struct UC2_canopen_slave_gpio : PinConfig
{
    /*
     XIAO ESP32S3 pin map for the can-gpio-esp board:

       D0  GPIO_NUM_1   – Remote-controllable output 1   (master writes x2301:01)
       D1  GPIO_NUM_2   – (spare)
       D2  GPIO_NUM_3   – E-stop button input            (active LOW, internal pull-up)
       D3  GPIO_NUM_4   – Remote-controllable output 2   (master writes x2301:02)
       D4  GPIO_NUM_5   – CAN TX
       D5  GPIO_NUM_6   – CAN RX
       D6  GPIO_NUM_43  – (spare) — also UART0 TX (Serial debug)
       D7  GPIO_NUM_44  – (spare) — also UART0 RX
       D8  GPIO_NUM_7   – (spare)
       D9  GPIO_NUM_8   – (spare)
       D10 GPIO_NUM_9   – Collision/resistance sensor ADC (ADC1_CH8)

     Notes:
       * GPIOs 1 and 4 match the user-facing identifiers in the firmware
         (digitaloutid=1 -> DIGITAL_OUT_1; id=2 -> DIGITAL_OUT_2).
       * The collision sensor is read by GpioCanSlave at ~50 Hz with a small
         EWMA filter. The trip threshold is stored in NVS preferences
         (key "gpioCollThr"), seeded from GPIO_COLLISION_THRESHOLD_DEFAULT
         on first boot.
    */

    const char *pindefName = "UC2_canopen_slave_gpio";
    const unsigned long BAUDRATE = 115200;

    // ── CAN bus ──────────────────────────────────────────────────────────
     int8_t CAN_TX =  GPIO_NUM_3;  // D2 in (I2C SDA) CAN Motor Board
     int8_t CAN_RX =  GPIO_NUM_2; // D1 in (I2C SCL)  CAN Motor Board
    uint32_t CAN_ID_CURRENT = CAN_ID_GPIO_0; // default node-id 60

    // ── Remote-controllable digital outputs ──────────────────────────────
    // DIGITAL_OUT_1 and DIGITAL_OUT_2 are driven by writes to OD
    // x2301_digital_output_command[0] and [1] respectively. They are also
    // settable via the slave's local serial /digitalout_act API.
    int8_t DIGITAL_OUT_1 = GPIO_NUM_1; // D0 -> remote-GPIO1
    int8_t DIGITAL_OUT_2 = GPIO_NUM_4; // D3 -> remote-GPIO4
    int8_t DIGITAL_OUT_3 = disabled;

    // ── Digital inputs ───────────────────────────────────────────────────
    // DIGITAL_IN_1 doubles as the E-stop input — DigitalInController::setup()
    // already configures it with INPUT_PULLDOWN. We use INPUT_PULLUP for a
    // momentary-to-GND E-stop button via GPIO_ESTOP_PIN below.
    int8_t DIGITAL_IN_1 =  GPIO_NUM_7; // D8;
    int8_t DIGITAL_IN_2 = disabled;
    int8_t DIGITAL_IN_3 = disabled;

    // GpioCanSlave owns the E-stop input (configures pull-up itself) so we
    // can publish its state on x2300 sub 1 without relying on
    // DigitalInController's PULLDOWN default.
    int8_t GPIO_ESTOP_PIN = disabled; // D2

    // ── Collision sensor (resistance → voltage divider on ADC) ───────────
    // ADC1_CH8 on ESP32-S3 (= GPIO9). 12-bit reads.
    // The sensor idles around ~585 counts; collisions push the value up OR
    // down (bench trace: dips to ~150-350 for ~2 s, bumps to ~650-690).
    // Detection is baseline-relative: reference (auto-seeded/calibrated) ±
    // threshold, confirmed over GPIO_COLLISION_SENSITIVITY_DEFAULT
    // consecutive samples so single-sample spikes (e.g. 55 or 29) are
    // rejected. All three values are runtime-tunable via /gpio_act.
    int8_t   GPIO_COLLISION_ADC                 = GPIO_NUM_9; // D10
    uint16_t GPIO_COLLISION_THRESHOLD_DEFAULT   = 150;        // deviation band, ADC counts
    uint8_t  GPIO_COLLISION_SENSITIVITY_DEFAULT = 4;          // consecutive samples
    uint16_t GPIO_ADC_FILTER_ALPHA_X1024        = 128;        // EWMA alpha * 1024

    // analogin_PIN_0 is wired to the same pin so the standard
    // AnalogInController::get path still works for diagnostics.
    int8_t analogin_PIN_0 = GPIO_NUM_9;

    // ── Routing — everything stays LOCAL on this slave ───────────────────
    // The GPIO slave has no motor / laser / LED / galvo hardware. Force the
    // routing table to OFF so nothing tries to forward those commands.
    int8_t ROUTE_MOTOR[4]  = {2, 2, 2, 2};
    int8_t ROUTE_HOME[4]   = {2, 2, 2, 2};
    int8_t ROUTE_TMC[4]    = {2, 2, 2, 2};
    int8_t ROUTE_LASER[4]  = {2, 2, 2, 2};
    int8_t ROUTE_LASER_4   = 2;
    int8_t ROUTE_LED       = 2;
    int8_t ROUTE_GALVO     = 2;

    // ── I2C disabled ─────────────────────────────────────────────────────
    int8_t I2C_SCL = disabled; // would be D4
    int8_t I2C_SDA = disabled; // would be D5
};

const UC2_canopen_slave_gpio pinConfig;
