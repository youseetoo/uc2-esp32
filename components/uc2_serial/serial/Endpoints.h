#pragma once

// Per-compiler "unused" marker: GCC/Clang (ESP-IDF xtensa build) honor
// __attribute__((unused)); MSVC (host test build) does not parse it at all.
#ifdef __GNUC__
#define UC2_UNUSED __attribute__((unused))
#else
#define UC2_UNUSED
#endif

#ifdef LASER_CONTROLLER
UC2_UNUSED static const  char* laser_act_endpoint = "/laser_act";
UC2_UNUSED static const  char* laser_get_endpoint = "/laser_get";
#endif

UC2_UNUSED static const  char* tmc_act_endpoint = "/tmc_act";
UC2_UNUSED static const  char* tmc_get_endpoint = "/tmc_get";

UC2_UNUSED static const  char* state_act_endpoint = "/state_act";
UC2_UNUSED static const  char* state_get_endpoint = "/state_get";
UC2_UNUSED static const  char* state_busy_endpoint = "/b";

#ifdef MOTOR_CONTROLLER
UC2_UNUSED static const  char* motor_act_endpoint = "/motor_act";
UC2_UNUSED static const  char* motor_get_endpoint = "/motor_get";
UC2_UNUSED static const  char* motor_setcalibration_endpoint = "/motor_setcalibration";
#endif

UC2_UNUSED static const  char* galvo_act_endpoint = "/galvo_act";
UC2_UNUSED static const  char* galvo_get_endpoint = "/galvo_get";

UC2_UNUSED static const  char* config_act_endpoint = "/config_act";
UC2_UNUSED static const  char* config_get_endpoint = "/config_get";
UC2_UNUSED static const  char* config_set_endpoint = "/config_set";
UC2_UNUSED static const  char* config_reset_endpoint = "/config_reset";

#ifdef HOME_MOTOR
UC2_UNUSED static const  char* home_act_endpoint = "/home_act";
UC2_UNUSED static const  char* home_get_endpoint = "/home_get";
#endif

#ifdef CAN_BUS_ENABLED
UC2_UNUSED static const  char* can_act_endpoint = "/can_act";
UC2_UNUSED static const  char* can_get_endpoint = "/can_get";
#endif

#ifdef ENCODER_CONTROLLER
UC2_UNUSED static const  char* encoder_act_endpoint = "/encoder_act";
UC2_UNUSED static const  char* encoder_get_endpoint = "/encoder_get";
#endif
#ifdef LINEAR_ENCODER_CONTROLLER
UC2_UNUSED static const  char* linearencoder_act_endpoint = "/linearencoder_act";
UC2_UNUSED static const  char* linearencoder_get_endpoint = "/linearencoder_get";
#endif
#ifdef DAC_CONTROLLER
UC2_UNUSED static const  char* dac_act_endpoint = "/dac_act";
UC2_UNUSED static const  char* dac_get_endpoint = "/dac_get";
#endif
#ifdef ANALOG_OUT_CONTROLLER
UC2_UNUSED static const  char* analogout_act_endpoint = "/analogout_act";
UC2_UNUSED static const  char* analogout_get_endpoint = "/analogout_get";
#endif
// Digital I/O endpoint strings are always defined — DeviceRouter routes
// /digitalout_act / /digitalin_get on builds without a local
// DIGITAL_*_CONTROLLER too (they forward to a remote GPIO slave via SDO).
UC2_UNUSED static const  char* digitalout_act_endpoint = "/digitalout_act";
UC2_UNUSED static const  char* digitalout_get_endpoint = "/digitalout_get";
UC2_UNUSED static const  char* digitalin_act_endpoint = "/digitalin_act";
UC2_UNUSED static const  char* digitalin_get_endpoint = "/digitalin_get";
// GPIO-slave collision detector — local on the GPIO slave, SDO-forwarded on
// the master (see DeviceRouter::handleGpioAct/Get).
UC2_UNUSED static const  char* gpio_act_endpoint = "/gpio_act";
UC2_UNUSED static const  char* gpio_get_endpoint = "/gpio_get";
// Generic I2C passthrough on the GPIO slave — one transaction per /i2c_act
// (write / optional delay / read); /i2c_get re-reads the last response.
// See DeviceRouter::handleI2cAct/Get and I2cBridge.
UC2_UNUSED static const  char* i2c_act_endpoint = "/i2c_act";
UC2_UNUSED static const  char* i2c_get_endpoint = "/i2c_get";
// PTZ keyboard bridge — local-only debug/config on the PTZ bridge node
// (see PtzKeyboard::act/get).
UC2_UNUSED static const  char* ptz_act_endpoint = "/ptz_act";
UC2_UNUSED static const  char* ptz_get_endpoint = "/ptz_get";

#ifdef JOYSTICK_USBHOST_PROVIDER
// DS4-over-USB-OTG joystick bridge — local-only speed-scaling config on the
// joystick bridge node (see JoystickRouter::act/get).
UC2_UNUSED static const  char* joystick_act_endpoint = "/joystick_act";
UC2_UNUSED static const  char* joystick_get_endpoint = "/joystick_get";
#endif
#ifdef LED_CONTROLLER
UC2_UNUSED static const  char* ledarr_act_endpoint = "/ledarr_act";
UC2_UNUSED static const  char* ledarr_get_endpoint = "/ledarr_get";
#endif

// Status indicator (on-board NeoPixel) and beeper for the FRAME / CAN HAT.
// These are local-only signalling interfaces; independent from /ledarr_act.
UC2_UNUSED static const  char* signal_act_endpoint = "/signal_act";
UC2_UNUSED static const  char* signal_get_endpoint = "/signal_get";
UC2_UNUSED static const  char* indicator_act_endpoint = "/indicator_act"; // alias for /signal_act
UC2_UNUSED static const  char* buzzer_act_endpoint = "/buzzer_act";
#ifdef MESSAGE_CONTROLLER
UC2_UNUSED static const  char* message_act_endpoint = "/message_act";
UC2_UNUSED static const  char* message_get_endpoint = "/message_get";
#endif
#ifdef ANALOG_IN_CONTROLLER
UC2_UNUSED static const  char* readanalogin_act_endpoint = "/readanalogin_act";
UC2_UNUSED static const  char* readanalogin_get_endpoint = "/readanalogin_get";
#endif
#ifdef PID_CONTROLLER
UC2_UNUSED static const  char* PID_act_endpoint = "/PID_act";
UC2_UNUSED static const  char* PID_get_endpoint = "/PID_get";
#endif
UC2_UNUSED static const  char* reset_nv_flash_endpoint = "/resetnv";
// OTA-over-CAN start command (used by DeviceRouter; not an HTTP endpoint).
UC2_UNUSED static const  char* ota_start_endpoint = "/ota_start";
#ifdef BLUETOOTH
UC2_UNUSED static const  char* bt_disconnect_endpoint = "/bt_disconnect";
UC2_UNUSED static const  char* bt_scan_endpoint = "/bt_scan";
UC2_UNUSED static const  char* bt_connect_endpoint = "/bt_connect";
UC2_UNUSED static const  char* bt_remove_endpoint = "/bt_remove";
UC2_UNUSED static const  char* bt_paireddevices_endpoint = "/bt_paireddevices";
#endif

UC2_UNUSED static const  char* modules_get_endpoint = "/modules_get";

// QID tracking endpoints
UC2_UNUSED static const  char* qid_state_endpoint = "/qid_state";
UC2_UNUSED static const  char* qid_pause_endpoint = "/qid_pause";
UC2_UNUSED static const  char* qid_resume_endpoint = "/qid_resume";

#ifdef HEAT_CONTROLLER
UC2_UNUSED static const  char* heat_act_endpoint = "/heat_act";
UC2_UNUSED static const  char* heat_get_endpoint = "/heat_get";
UC2_UNUSED static const char* ds18b20_act_endpoint = "/ds18b20_act";
UC2_UNUSED static const char* ds18b20_get_endpoint = "/ds18b20_get";
#endif

// Chassis-cooling endpoints (TMP102 + MCP4017 + tacho) and the illumination
// board's NTC heat-sink monitoring, which shares /temp_get and /temp_act.
#if defined(FAN_CONTROLLER) || defined(TMP102_CONTROLLER) || defined(THERMAL_CONTROLLER)
UC2_UNUSED static const  char* temp_get_endpoint = "/temp_get";
UC2_UNUSED static const  char* temp_act_endpoint = "/temp_act";
UC2_UNUSED static const  char* fan_act_endpoint  = "/fan_act";
UC2_UNUSED static const  char* fan_get_endpoint  = "/fan_get";
#endif

// Routing table inspection/override (CANopen builds)
UC2_UNUSED static const  char* route_get_endpoint = "/route_get";
UC2_UNUSED static const  char* route_set_endpoint = "/route_set";

