#pragma once

#ifdef LASER_CONTROLLER
__attribute__ ((unused)) static const  char* laser_act_endpoint = "/laser_act";
__attribute__ ((unused)) static const  char* laser_get_endpoint = "/laser_get";
#endif

__attribute__ ((unused)) static const  char* tmc_act_endpoint = "/tmc_act";
__attribute__ ((unused)) static const  char* tmc_get_endpoint = "/tmc_get";

__attribute__ ((unused)) static const  char* state_act_endpoint = "/state_act";
__attribute__ ((unused)) static const  char* state_get_endpoint = "/state_get";
__attribute__ ((unused)) static const  char* state_busy_endpoint = "/b";

#ifdef MOTOR_CONTROLLER
__attribute__ ((unused)) static const  char* motor_act_endpoint = "/motor_act";
__attribute__ ((unused)) static const  char* motor_get_endpoint = "/motor_get";
__attribute__ ((unused)) static const  char* motor_setcalibration_endpoint = "/motor_setcalibration";
#endif

__attribute__ ((unused)) static const  char* galvo_act_endpoint = "/galvo_act";
__attribute__ ((unused)) static const  char* galvo_get_endpoint = "/galvo_get";

__attribute__ ((unused)) static const  char* config_act_endpoint = "/config_act";
__attribute__ ((unused)) static const  char* config_get_endpoint = "/config_get";

#ifdef HOME_MOTOR
__attribute__ ((unused)) static const  char* home_act_endpoint = "/home_act";
__attribute__ ((unused)) static const  char* home_get_endpoint = "/home_get";
#endif

#ifdef I2C_MASTER
__attribute__ ((unused)) static const  char* i2c_act_endpoint = "/i2c_act";
__attribute__ ((unused)) static const  char* i2c_get_endpoint = "/i2c_get";
#endif

#ifdef CAN_CONTROLLER
__attribute__ ((unused)) static const  char* can_act_endpoint = "/can_act";
__attribute__ ((unused)) static const  char* can_get_endpoint = "/can_get";
#endif

#ifdef ENCODER_CONTROLLER
__attribute__ ((unused)) static const  char* encoder_act_endpoint = "/encoder_act";
__attribute__ ((unused)) static const  char* encoder_get_endpoint = "/encoder_get";
#endif
#ifdef LINEAR_ENCODER_CONTROLLER
__attribute__ ((unused)) static const  char* linearencoder_act_endpoint = "/linearencoder_act";
__attribute__ ((unused)) static const  char* linearencoder_get_endpoint = "/linearencoder_get";
#endif
#ifdef DAC_CONTROLLER
__attribute__ ((unused)) static const  char* dac_act_endpoint = "/dac_act";
__attribute__ ((unused)) static const  char* dac_get_endpoint = "/dac_get";
#endif
#ifdef ANALOG_OUT_CONTROLLER
__attribute__ ((unused)) static const  char* analogout_act_endpoint = "/analogout_act";
__attribute__ ((unused)) static const  char* analogout_get_endpoint = "/analogout_get";
#endif
#ifdef DIGITAL_OUT_CONTROLLER
__attribute__ ((unused)) static const  char* digitalout_act_endpoint = "/digitalout_act";
__attribute__ ((unused)) static const  char* digitalout_get_endpoint = "/digitalout_get";
#endif
#ifdef DIGITAL_IN_CONTROLLER
__attribute__ ((unused)) static const  char* digitalin_act_endpoint = "/digitalin_act";
__attribute__ ((unused)) static const  char* digitalin_get_endpoint = "/digitalin_get";
#endif
#ifdef LED_CONTROLLER
__attribute__ ((unused)) static const  char* ledarr_act_endpoint = "/ledarr_act";
__attribute__ ((unused)) static const  char* ledarr_get_endpoint = "/ledarr_get";
#endif
#ifdef MESSAGE_CONTROLLER
__attribute__ ((unused)) static const  char* message_act_endpoint = "/message_act";
__attribute__ ((unused)) static const  char* message_get_endpoint = "/message_get";
#endif
#ifdef ANALOG_IN_CONTROLLER
__attribute__ ((unused)) static const  char* readanalogin_act_endpoint = "/readanalogin_act";
__attribute__ ((unused)) static const  char* readanalogin_get_endpoint = "/readanalogin_get";
#endif
#ifdef PID_CONTROLLER
__attribute__ ((unused)) static const  char* PID_act_endpoint = "/PID_act";
__attribute__ ((unused)) static const  char* PID_get_endpoint = "/PID_get";
#endif
__attribute__ ((unused)) static const  char* features_endpoint = "/features_get";
__attribute__ ((unused)) static const  char* identity_endpoint = "/identity";
__attribute__ ((unused)) static const  char* ota_endpoint = "/ota";
__attribute__ ((unused)) static const  char* update_endpoint = "/update";
__attribute__ ((unused)) static const  char* scanwifi_endpoint = "/wifi/scan";
__attribute__ ((unused)) static const  char* connectwifi_endpoint = "/wifi/connect";
__attribute__ ((unused)) static const  char* reset_nv_flash_endpoint = "/resetnv";
#ifdef BLUETOOTH
__attribute__ ((unused)) static const  char* bt_scan_endpoint = "/bt_scan";
__attribute__ ((unused)) static const  char* bt_connect_endpoint = "/bt_connect";
__attribute__ ((unused)) static const  char* bt_remove_endpoint = "/bt_remove";
__attribute__ ((unused)) static const  char* bt_paireddevices_endpoint = "/bt_paireddevices";
#endif

__attribute__ ((unused)) static const  char* modules_get_endpoint = "/modules_get";

#ifdef HEAT_CONTROLLER
__attribute__ ((unused)) static const  char* heat_act_endpoint = "/heat_act";
__attribute__ ((unused)) static const  char* heat_get_endpoint = "/heat_get";
__attribute__ ((unused)) static const char* ds18b20_act_endpoint = "/ds18b20_act";
__attribute__ ((unused)) static const char* ds18b20_get_endpoint = "/ds18b20_get";
#endif

