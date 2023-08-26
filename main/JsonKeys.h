#pragma once
//using a key for preferences allows only a length of 15 chars

__attribute__ ((unused)) static const PROGMEM char * keyLEDPin = "ledArrPin";
__attribute__ ((unused)) static const PROGMEM char * keyLEDCount = "ledArrNum";

__attribute__ ((unused)) static const PROGMEM char * keyQueueID = "qid";

__attribute__ ((unused)) static const PROGMEM char * keyDigitalOut1Pin = "digitaloutPin1";
__attribute__ ((unused)) static const PROGMEM char * keyDigitalOut2Pin = "digitaloutPin2";
__attribute__ ((unused)) static const PROGMEM char * keyDigitalOut3Pin = "digitaloutPin3";

__attribute__ ((unused)) static const PROGMEM char * keyDigitalIn1Pin = "digitalinPin1";
__attribute__ ((unused)) static const PROGMEM char * keyDigitalIn2Pin = "digitalinPin2";
__attribute__ ((unused)) static const PROGMEM char * keyDigitalIn3Pin = "digitalinPin3";

__attribute__ ((unused)) static const PROGMEM char * keyDigitalOut1IsTrigger = "digitalout1IsTrigger";
__attribute__ ((unused)) static const PROGMEM char * keyDigitalOut2IsTrigger = "digitalout2IsTrigger";
__attribute__ ((unused)) static const PROGMEM char * keyDigitalOut3IsTrigger = "digitalout3IsTrigger";

__attribute__ ((unused)) static const PROGMEM char * keyDigitalOut1TriggerDelayOn = "digitalout1TriggerDelayOn"; 
__attribute__ ((unused)) static const PROGMEM char * keyDigitalOut2TriggerDelayOn = "digitalout2TriggerDelayOn";
__attribute__ ((unused)) static const PROGMEM char * keyDigitalOut3TriggerDelayOn = "digitalout3TriggerDelayOn";

__attribute__ ((unused)) static const PROGMEM char * keyDigitalOut1TriggerDelayOff = "digitalout1TriggerDelayOff";
__attribute__ ((unused)) static const PROGMEM char * keyDigitalOut2TriggerDelayOff = "digitalout2TriggerDelayOff";
__attribute__ ((unused)) static const PROGMEM char * keyDigitalOut3TriggerDelayOff = "digitalout3TriggerDelayOff";

__attribute__ ((unused)) static const PROGMEM char * keyDigitalOutid = "digitaloutid";
__attribute__ ((unused)) static const PROGMEM char * keyDigitalOutVal = "digitaloutval";
__attribute__ ((unused)) static const PROGMEM char * keyDigitalOutIsTriggerReset = "digitaloutistriggerreset";

__attribute__ ((unused)) static const PROGMEM char * keyAnalogIn1Pin = "analoginPin1";
__attribute__ ((unused)) static const PROGMEM char * keyAnalogIn2Pin = "analoginPin2";
__attribute__ ((unused)) static const PROGMEM char * keyAnalogIn3Pin = "analoginPin3";

__attribute__ ((unused)) static const PROGMEM char * keyAnalogOut1Pin = "analogoutPin1";
__attribute__ ((unused)) static const PROGMEM char * keyAnalogOut2Pin = "analogoutPin2";
__attribute__ ((unused)) static const PROGMEM char * keyAnalogOut3Pin = "analogoutPin3";

__attribute__ ((unused)) static const PROGMEM char * keyLaser1Pin = "laserPin1";
__attribute__ ((unused)) static const PROGMEM char * keyLaser2Pin = "laserPin2";
__attribute__ ((unused)) static const PROGMEM char * keyLaser3Pin = "laserPin3";

__attribute__ ((unused)) static const PROGMEM char * keyDACfake1Pin = "dacFake1";
__attribute__ ((unused)) static const PROGMEM char * keyDACfake2Pin = "dacFake2";

__attribute__ ((unused)) static const PROGMEM char * keyIdentifier = "identifier";
__attribute__ ((unused)) static const PROGMEM char * key_return = "return";

__attribute__ ((unused)) static const PROGMEM char * keyIsBooting  = "isBooting";
__attribute__ ((unused)) static const PROGMEM char * keyWifiSSID = "ssid";
__attribute__ ((unused)) static const PROGMEM char * keyWifiPW = "PW";
__attribute__ ((unused)) static const PROGMEM char * keyWifiAP = "AP";

__attribute__ ((unused)) static const PROGMEM char * keyLed = "led";
__attribute__ ((unused)) static const PROGMEM char * keyLEDArrMode = "LEDArrMode";

__attribute__ ((unused)) static const PROGMEM char * keyRed = "r";
__attribute__ ((unused)) static const PROGMEM char * keyGreen = "g";
__attribute__ ((unused)) static const PROGMEM char * keyBlue = "b";
__attribute__ ((unused)) static const PROGMEM char * keyid = "id";
__attribute__ ((unused)) static const PROGMEM char * key_led_array = "led_array";
__attribute__ ((unused)) static const PROGMEM char * key_led_isOn = "led_ison";

__attribute__ ((unused)) static const PROGMEM char * key_config = "config";

__attribute__ ((unused)) static const PROGMEM char * key_motor = "motor";
__attribute__ ((unused)) static const PROGMEM char * key_home = "home";
__attribute__ ((unused)) static const PROGMEM char * key_home_endpospin = "endpospin";
__attribute__ ((unused)) static const PROGMEM char * key_home_timeout = "timeout";
__attribute__ ((unused)) static const PROGMEM char * key_home_speed = "speed";
__attribute__ ((unused)) static const PROGMEM char * key_home_maxspeed = "maxspeed";
__attribute__ ((unused)) static const PROGMEM char * key_home_direction = "direction";
__attribute__ ((unused)) static const PROGMEM char * key_home_timestarted = "timestarted";
__attribute__ ((unused)) static const PROGMEM char * key_home_isactive = "isactive";
__attribute__ ((unused)) static const PROGMEM char * key_home_endposrelease = "endposrelease";
__attribute__ ((unused)) static const PROGMEM char * key_home_endstoppolarity = "endstoppolarity";


__attribute__ ((unused)) static const PROGMEM char * key_encoder = "encoder";
__attribute__ ((unused)) static const PROGMEM char * key_encoder_timeout = "timeout";
__attribute__ ((unused)) static const PROGMEM char * key_encoder_getpos = "posval";
__attribute__ ((unused)) static const PROGMEM char * key_encoder_calibrate = "calpos";
__attribute__ ((unused)) static const PROGMEM char * key_encoder_runto = "direction";
__attribute__ ((unused)) static const PROGMEM char * key_encoder_encoderid = "id";
__attribute__ ((unused)) static const PROGMEM char * key_encoder_calibpos = "calibsteps";

__attribute__ ((unused)) static const PROGMEM char * key_rotator = "rotator";

__attribute__ ((unused)) static const PROGMEM char * key_steppers = "steppers";
__attribute__ ((unused)) static const PROGMEM char * key_setposition = "setpos";
__attribute__ ((unused)) static const PROGMEM char * key_currentpos = "posval";
__attribute__ ((unused)) static const PROGMEM char * key_stepperid = "stepperid";
__attribute__ ((unused)) static const PROGMEM char * key_speed = "speed";
__attribute__ ((unused)) static const PROGMEM char * key_speedmax = "speedmax";
__attribute__ ((unused)) static const PROGMEM char * key_position = "position";
__attribute__ ((unused)) static const PROGMEM char * key_stopped = "isbusy";
__attribute__ ((unused)) static const PROGMEM char * key_isabs = "isabs";
__attribute__ ((unused)) static const PROGMEM char * key_isstop = "isstop";
__attribute__ ((unused)) static const PROGMEM char * key_isaccel = "isaccel";
__attribute__ ((unused)) static const PROGMEM char * key_acceleration = "accel";
__attribute__ ((unused)) static const PROGMEM char * key_isen = "isen";
__attribute__ ((unused)) static const PROGMEM char * key_isenauto = "isenauto";
__attribute__ ((unused)) static const PROGMEM char * key_isforever = "isforever";
__attribute__ ((unused)) static const PROGMEM char * key_dirpin = "dir";
__attribute__ ((unused)) static const PROGMEM char * key_steppin = "step";
__attribute__ ((unused)) static const PROGMEM char * key_enablepin = "enable";
__attribute__ ((unused)) static const PROGMEM char * key_dirpin_inverted = "dir_inverted";
__attribute__ ((unused)) static const PROGMEM char * key_steppin_inverted = "step_inverted";
__attribute__ ((unused)) static const PROGMEM char * key_enablepin_inverted = "enable_inverted";
__attribute__ ((unused)) static const PROGMEM char * key_min_position = "min_pos";
__attribute__ ((unused)) static const PROGMEM char * key_max_position = "max_pos";

__attribute__ ((unused)) static const PROGMEM char *dateKey = "date";
__attribute__ ((unused)) static const PROGMEM char *key_modules = "modules";
__attribute__ ((unused)) static const PROGMEM char *key_analogin = "analogin";
__attribute__ ((unused)) static const PROGMEM char *key_pid = "pid";
__attribute__ ((unused)) static const PROGMEM char *key_laser = "laser";
__attribute__ ((unused)) static const PROGMEM char *key_dac = "dac";
__attribute__ ((unused)) static const PROGMEM char *key_analogout = "analogout";
__attribute__ ((unused)) static const PROGMEM char *key_digitalout = "digitalout";
__attribute__ ((unused)) static const PROGMEM char *key_digitalin = "digitalin";
__attribute__ ((unused)) static const PROGMEM char *key_scanner = "scanner";
__attribute__ ((unused)) static const PROGMEM char *key_joy = "joy";
__attribute__ ((unused)) static const PROGMEM char *key_wifi = "wifi";
__attribute__ ((unused)) static const PROGMEM char *key_joiypinX = "joyX";
__attribute__ ((unused)) static const PROGMEM char *key_joiypinY = "joyY";
__attribute__ ((unused)) static const PROGMEM char *key_readanaloginID = "readanaloginID";
__attribute__ ((unused)) static const PROGMEM char *key_N_analogin_avg = "N_analogin_avg";
__attribute__ ((unused)) static const PROGMEM char *key_readanaloginPIN = "readanaloginPIN";
__attribute__ ((unused)) static const PROGMEM char *key_PIDID = "PIDID";
__attribute__ ((unused)) static const PROGMEM char *key_PIDPIN = "PIDPIN";
__attribute__ ((unused)) static const PROGMEM char *key_PIDactive = "PIDactive";
__attribute__ ((unused)) static const PROGMEM char *key_Kp = "Kp";
__attribute__ ((unused)) static const PROGMEM char *key_Ki = "Ki";
__attribute__ ((unused)) static const PROGMEM char *key_Kd = "Kd";
__attribute__ ((unused)) static const PROGMEM char *key_target = "target";
__attribute__ ((unused)) static const PROGMEM char *key_PID_updaterate = "PID_updaterate";
