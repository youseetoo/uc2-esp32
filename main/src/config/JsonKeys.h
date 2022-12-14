#pragma once
//using a key for preferences allows only a length of 15 chars
static const PROGMEM  char * keyMotorXStepPin  = "motXstp";
static const PROGMEM  char * keyMotorXStepPinInverted  = "motXstp_inv";
static const PROGMEM  char * keyMotorXDirPin = "motXdir";
static const PROGMEM  char * keyMotorXDirPinInverted = "motXdir_inv";
static const PROGMEM  char * keyMotorYStepPin = "motYstp";
static const PROGMEM  char * keyMotorYStepPinInverted = "motYstp_inv";
static const PROGMEM  char * keyMotorYDirPin = "motYdir";
static const PROGMEM  char * keyMotorYDirPinInverted = "motYdir_inv";
static const PROGMEM  char * keyMotorZStepPin = "motZstp";
static const PROGMEM  char * keyMotorZStepPinInverted = "motZstp_inv";
static const PROGMEM  char * keyMotorZDirPin = "motZdir";
static const PROGMEM  char * keyMotorZDirPinInverted = "motZdir_inv";
static const PROGMEM char * keyMotorAStepPin = "motAstp";
static const PROGMEM char * keyMotorAStepPinInverted = "motAstp_inv";
static const PROGMEM char * keyMotorADirPin = "motAdir";
static const PROGMEM char * keyMotorADirPinInverted = "motAdir_inv";
static const PROGMEM char * keyMotorEnableX = "motEnableX";
static const PROGMEM char * keyMotorEnableXinverted = "motEnableX_inv";
static const PROGMEM char * keyMotorEnableY = "motEnableY";
static const PROGMEM char * keyMotorEnableYinverted = "motEnableY_inv";
static const PROGMEM char * keyMotorEnableZ = "motEnableZ";
static const PROGMEM char * keyMotorEnableZinverted = "motEnableZ_inv";
static const PROGMEM char * keyMotorEnableA = "motEnableA";
static const PROGMEM char * keyMotorEnableAinverted = "motEnableA_inv";

static const PROGMEM char * keyLEDPin = "ledArrPin";
static const PROGMEM char * keyLEDCount = "ledArrNum";

static const PROGMEM char * keyDigitalOut1Pin = "digitaloutPin1";
static const PROGMEM char * keyDigitalOut2Pin = "digitaloutPin2";
static const PROGMEM char * keyDigitalOut3Pin = "digitaloutPin3";

static const PROGMEM char * keyDigitalIn1Pin = "digitalinPin1";
static const PROGMEM char * keyDigitalIn2Pin = "digitalinPin2";
static const PROGMEM char * keyDigitalIn3Pin = "digitalinPin3";

static const PROGMEM char * keyAnalogIn1Pin = "analoginPin1";
static const PROGMEM char * keyAnalogIn2Pin = "analoginPin2";
static const PROGMEM char * keyAnalogIn3Pin = "analoginPin3";

static const PROGMEM char * keyAnalogOut1Pin = "analogoutPin1";
static const PROGMEM char * keyAnalogOut2Pin = "analogoutPin2";
static const PROGMEM char * keyAnalogOut3Pin = "analogoutPin3";

static const PROGMEM char * keyLaser1Pin = "laserPin1";
static const PROGMEM char * keyLaser2Pin = "laserPin2";
static const PROGMEM char * keyLaser3Pin = "laserPin3";

static const PROGMEM char * keyDACfake1Pin = "dacFake1";
static const PROGMEM char * keyDACfake2Pin = "dacFake2";

static const PROGMEM char * keyIdentifier = "identifier";
static const PROGMEM char * key_return = "return";

static const PROGMEM char * keyIsBooting  = "isBooting";
static const PROGMEM char * keyWifiSSID = "ssid";
static const PROGMEM char * keyWifiPW = "PW";
static const PROGMEM char * keyWifiAP = "AP";

static const PROGMEM char * keyLed = "led";
static const PROGMEM char * keyLEDArrMode = "LEDArrMode";

static const PROGMEM char * keyRed = "red";
static const PROGMEM char * keyGreen = "green";
static const PROGMEM char * keyBlue = "blue";
static const PROGMEM char * keyid = "id";
static const PROGMEM char * key_led_array = "led_array";
static const PROGMEM char * key_led_isOn = "led_ison";

static const PROGMEM char * key_config = "config";

static const PROGMEM char * key_motor = "motor";
static const PROGMEM char * key_home = "home";
static const PROGMEM char * key_home_endpospin = "endpospin";
static const PROGMEM char * key_home_timeout = "timeout";
static const PROGMEM char * key_home_speed = "speed";
static const PROGMEM char * key_home_maxspeed = "maxspeed";
static const PROGMEM char * key_home_direction = "direction";
static const PROGMEM char * key_home_timestarted = "timestarted";
static const PROGMEM char * key_home_isactive = "isactive";
static const PROGMEM char * key_home_endposrelease = "endposrelease";

static const PROGMEM char * key_steppers = "steppers";
static const PROGMEM char * key_stepperid = "stepperid";
static const PROGMEM char * key_speed = "speed";
static const PROGMEM char * key_speedmax = "speedmax";
static const PROGMEM char * key_position = "position";
static const PROGMEM char * key_stopped = "isbusy";
static const PROGMEM char * key_isabs = "isabs";
static const PROGMEM char * key_isstop = "isstop";
static const PROGMEM char * key_isaccel = "isaccel";
static const PROGMEM char * key_acceleration = "acceleration";
static const PROGMEM char * key_isen = "isen";
static const PROGMEM char * key_isforever = "isforever";
static const PROGMEM char * key_dirpin = "dir";
static const PROGMEM char * key_steppin = "step";
static const PROGMEM char * key_enablepin = "enable";
static const PROGMEM char * key_dirpin_inverted = "dir_inverted";
static const PROGMEM char * key_steppin_inverted = "step_inverted";
static const PROGMEM char * key_enablepin_inverted = "enable_inverted";
static const PROGMEM char * key_min_position = "min_pos";
static const PROGMEM char * key_max_position = "max_pos";

static const PROGMEM char *dateKey = "date";
static const PROGMEM char *key_modules = "modules";
static const PROGMEM char *key_analogin = "analogin";
static const PROGMEM char *key_pid = "pid";
static const PROGMEM char *key_laser = "laser";
static const PROGMEM char *key_dac = "dac";
static const PROGMEM char *key_analogout = "analogout";
static const PROGMEM char *key_digitalout = "digitalout";
static const PROGMEM char *key_digitalin = "digitalin";
static const PROGMEM char *key_scanner = "scanner";
static const PROGMEM char *key_joy = "joy";
static const PROGMEM char *key_wifi = "wifi";
static const PROGMEM char *key_joiypinX = "joyX";
static const PROGMEM char *key_joiypinY = "joyY";
static const PROGMEM char *key_readanaloginID = "readanaloginID";
static const PROGMEM char *key_N_analogin_avg = "N_analogin_avg";
static const PROGMEM char *key_readanaloginPIN = "readanaloginPIN";
static const PROGMEM char *key_PIDID = "PIDID";
static const PROGMEM char *key_PIDPIN = "PIDPIN";
static const PROGMEM char *key_PIDactive = "PIDactive";
static const PROGMEM char *key_Kp = "Kp";
static const PROGMEM char *key_Ki = "Ki";
static const PROGMEM char *key_Kd = "Kd";
static const PROGMEM char *key_target = "target";
static const PROGMEM char *key_PID_updaterate = "PID_updaterate";
