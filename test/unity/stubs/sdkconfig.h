// Host stub: sdkconfig.h — mirrors the canopen_master app's CONFIG_UC2_*
// selections (apps/canopen_master/sdkconfig, grep-verified 2026-07-22), so
// the C TUs compile the same code paths as the canopen_master ESP32 build.
// Keep in sync if that app's sdkconfig changes.
#pragma once

#define CONFIG_UC2_NODE_ROLE 1
#define CONFIG_UC2_CORE_DEBUG_LEVEL 5

#define CONFIG_UC2_BLUETOOTH 1
#define CONFIG_UC2_BTHID 1
#define CONFIG_UC2_MOTOR_CONTROLLER 1
#define CONFIG_UC2_LASER_CONTROLLER 1
#define CONFIG_UC2_LED_CONTROLLER 1
#define CONFIG_UC2_DIGITAL_IN_CONTROLLER 1
#define CONFIG_UC2_DAC_CONTROLLER 1
#define CONFIG_UC2_HOME_MOTOR 1
#define CONFIG_UC2_MESSAGE_CONTROLLER 1
#define CONFIG_UC2_STAGE_SCAN 1
#define CONFIG_UC2_CAN_BUS_ENABLED 1
#define CONFIG_UC2_CAN_CONTROLLER_CANOPEN 1
#define CONFIG_UC2_GALVO_CONTROLLER 1
#define CONFIG_UC2_DIAL_CONTROLLER 1
#define CONFIG_UC2_TMC_CONTROLLER 1
#define CONFIG_UC2_TMP102_CONTROLLER 1
#define CONFIG_UC2_FAN_CONTROLLER 1
#define CONFIG_UC2_CANOPEN_OTA_NO_RECEIVER 1
#define CONFIG_UC2_FORCE_BT_CLASSIC_ONLY 1
