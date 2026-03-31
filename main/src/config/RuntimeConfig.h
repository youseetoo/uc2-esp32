#pragma once
#include <stdint.h>
#include <stdbool.h>

// Runtime configuration struct that replaces most #ifdef compile switches.
// Each field has a compile-time default derived from the original build flags.
// At boot, NVSConfig::load() fills these fields from NVS, falling back to
// compile-time defaults for any key that was never stored.

struct RuntimeConfig
{
    // ── Module enable/disable flags ──────────────────────────────────
    // Compile-time defaults mirror the original -D flags so that an
    // un-configured NVS partition produces the same behaviour as before.

#ifdef MOTOR_CONTROLLER
    bool motor_enabled = true;
#else
    bool motor_enabled = false;
#endif

#ifdef LASER_CONTROLLER
    bool laser_enabled = true;
#else
    bool laser_enabled = false;
#endif

#ifdef LED_CONTROLLER
    bool led_enabled = true;
#else
    bool led_enabled = false;
#endif

#ifdef HOME_MOTOR
    bool home_motor_enabled = true;
#else
    bool home_motor_enabled = false;
#endif

#ifdef DAC_CONTROLLER
    bool dac_enabled = true;
#else
    bool dac_enabled = false;
#endif

#ifdef ANALOG_IN_CONTROLLER
    bool analog_in_enabled = true;
#else
    bool analog_in_enabled = false;
#endif

#ifdef ANALOG_OUT_CONTROLLER
    bool analog_out_enabled = true;
#else
    bool analog_out_enabled = false;
#endif

#ifdef DIGITAL_IN_CONTROLLER
    bool digital_in_enabled = true;
#else
    bool digital_in_enabled = false;
#endif

#ifdef DIGITAL_OUT_CONTROLLER
    bool digital_out_enabled = true;
#else
    bool digital_out_enabled = false;
#endif

#ifdef LINEAR_ENCODER_CONTROLLER
    bool linear_encoder_enabled = true;
#else
    bool linear_encoder_enabled = false;
#endif

#ifdef PID_CONTROLLER
    bool pid_enabled = true;
#else
    bool pid_enabled = false;
#endif

#ifdef SCANNER_CONTROLLER
    bool scanner_enabled = true;
#else
    bool scanner_enabled = false;
#endif

#ifdef GALVO_CONTROLLER
    bool galvo_enabled = true;
#else
    bool galvo_enabled = false;
#endif

#ifdef MESSAGE_CONTROLLER
    bool message_enabled = true;
#else
    bool message_enabled = false;
#endif

#ifdef OBJECTIVE_CONTROLLER
    bool objective_enabled = true;
#else
    bool objective_enabled = false;
#endif

#ifdef TMC_CONTROLLER
    bool tmc_enabled = true;
#else
    bool tmc_enabled = false;
#endif

#ifdef HEAT_CONTROLLER
    bool heat_enabled = true;
#else
    bool heat_enabled = false;
#endif

#ifdef BLUETOOTH
    bool bluetooth_enabled = true;
#else
    bool bluetooth_enabled = false;
#endif

#ifdef WIFI
    bool wifi_enabled = true;
#else
    bool wifi_enabled = false;
#endif

#ifdef DIAL_CONTROLLER
    bool dial_enabled = true;
#else
    bool dial_enabled = false;
#endif

    // ── CAN bus configuration ────────────────────────────────────────
#ifdef CAN_BUS_ENABLED
    bool can_enabled = true;
#else
    bool can_enabled = false;
#endif

#ifdef CAN_SEND_COMMANDS
    bool can_master = true;
#else
    bool can_master = false;
#endif

#ifdef CAN_RECEIVE_MOTOR
    bool can_slave_motor = true;
#else
    bool can_slave_motor = false;
#endif

#ifdef CAN_RECEIVE_LASER
    bool can_slave_laser = true;
#else
    bool can_slave_laser = false;
#endif

#ifdef CAN_RECEIVE_LED
    bool can_slave_led = true;
#else
    bool can_slave_led = false;
#endif

#ifdef CAN_RECEIVE_GALVO
    bool can_slave_galvo = true;
#else
    bool can_slave_galvo = false;
#endif

#ifdef CAN_HYBRID
    bool can_hybrid = true;
#else
    bool can_hybrid = false;
#endif

    // CAN node ID and bitrate (runtime-overridable)
    uint8_t can_node_id = 0;
    uint32_t can_bitrate = 500000;  // 500 kbit/s default

    // ── Serial protocol ──────────────────────────────────────────────
    // 0 = legacy (newline JSON), 1 = COBS+JSON, 2 = COBS+MsgPack
    uint8_t serial_protocol = 0;
};

// Global singleton — defined in NVSConfig.cpp
extern RuntimeConfig runtimeConfig;
