#include "ModuleController.h"
#include "PinConfig.h"
#include "src/led/LedController.h"
#include "src/motor/FocusMotor.h"
#include "src/config/ConfigController.h"
#include "src/home/HomeMotor.h"
#include "src/heat/HeatController.h"
#include "src/encoder/EncoderController.h"
#include "src/encoder/LinearEncoderController.h"
#include "src/laser/LaserController.h"
#include "src/pid/PidController.h"
#include "src/analogin/AnalogInController.h"
#include "src/analogin/AnalogJoystick.h"
#include "src/temp/DS18b20Controller.h"
#include "src/image/ImageController.h"
#include "src/dac/DacController.h"
#include "src/analogout/AnalogOutController.h"
#include "src/digitalout/DigitalOutController.h"
#include "src/digitalin/DigitalInController.h"
#include "src/state/State.h"
#include "src/bt/BtController.h"
#include "src/scanner/ScannerController.h"
#include "src/wifi/WifiController.h"
#include "cJSON.h"
#include "Module.h"



void ModuleController::setup()
{

    // activate all modules that are configured

    // moduleConfig.led = true;
    // moduleConfig.motor = true;
    for (auto &x : modules)
    {
        if (x.second != nullptr)
        {
            delete x.second;
            // x.second = nullptr;
        }
    }
    modules.clear();

    log_i("Using config name %s", pinConfig.pindefName);

    // eventually load the LED module
    if (pinConfig.LED_PIN >= 0)
    {
        modules.insert(std::make_pair(AvailableModules::led, dynamic_cast<Module *>(new LedController())));
        log_i("add led");
    }

    // eventually load the BTController module
    if (pinConfig.enableBlueTooth)
    {
        modules.insert(std::make_pair(AvailableModules::btcontroller, dynamic_cast<Module *>(new BtController())));
        log_i("add btcontroller");
    }
    if (pinConfig.enableWifi)
    {
        modules.insert(std::make_pair(AvailableModules::wifi, dynamic_cast<Module *>(new WifiController())));
        log_i("add wificntroller");
    }

    // add the state module
    modules.insert(std::make_pair(AvailableModules::state, dynamic_cast<Module *>(new State())));
    log_i("add state");

    // eventually load the motor module
    if (pinConfig.MOTOR_ENABLE >= 0 || pinConfig.I2C_SCL >= 0)
    {
        modules.insert(std::make_pair(AvailableModules::motor, dynamic_cast<Module *>(new FocusMotor())));
        log_i("add motor");
    }

    // eventually load the motor homing module
    if (pinConfig.PIN_DEF_END_X >= 0 || pinConfig.PIN_DEF_END_Y >= 0 || pinConfig.PIN_DEF_END_Z >= 0)
    {
        modules.insert(std::make_pair(AvailableModules::home, dynamic_cast<Module *>(new HomeMotor())));
        log_i("add home");
    }

    // eventually load the heat module
    if (pinConfig.DS28b20_PIN >= 0 and false)
    {
        // eventually load the ds18b20 module
        modules.insert(std::make_pair(AvailableModules::heat, dynamic_cast<Module *>(new HeatController())));
        log_i("add heat");
        // eventually load the ds18b20 module
        modules.insert(std::make_pair(AvailableModules::ds18b20, dynamic_cast<Module *>(new DS18b20Controller())));
        log_i("add ds18b20");
    }

    // eventually load the image module
    if (pinConfig.IMAGE_ACTIVE >= 0)
    {
        modules.insert(std::make_pair(AvailableModules::image, dynamic_cast<Module *>(new ImageController())));
        log_i("add image");
    }

    // eventually load the motor encoder module
    if (pinConfig.X_CAL_CLK >= 0 || pinConfig.Y_CAL_CLK >= 0 || pinConfig.Z_CAL_CLK >= 0)
    {
        modules.insert(std::make_pair(AvailableModules::encoder, dynamic_cast<Module *>(new EncoderController())));
        /*pinConfig.DIGITAL_IN_1=pinConfig.PIN_DEF_END_X;
        pinConfig.DIGITAL_IN_2=pinConfig.PIN_DEF_END_Y;
        pinConfig.DIGITAL_IN_3=pinConfig.PIN_DEF_END_Z;*/
        log_i("add encoder");
    }

    // eventually load the linear encoder module
    if (pinConfig.ENC_X_A >= 0 || pinConfig.ENC_Y_A >= 0 || pinConfig.ENC_Z_A >= 0)
    {
        // AS5311
        modules.insert(std::make_pair(AvailableModules::linearencoder, dynamic_cast<Module *>(new LinearEncoderController())));
        /*pinConfig.DIGITAL_IN_1=pinConfig.PIN_DEF_END_X;
        pinConfig.DIGITAL_IN_2=pinConfig.PIN_DEF_END_Y;
        pinConfig.DIGITAL_IN_3=pinConfig.PIN_DEF_END_Z;*/
        log_i("add linear encoder");
    }

    // eventually load the analogin module
    if (pinConfig.analogin_PIN_0 >= 0 || pinConfig.analogin_PIN_1 >= 0 || pinConfig.analogin_PIN_2 >= 0 || pinConfig.analogin_PIN_3 >= 0)
    {
        modules.insert(std::make_pair(AvailableModules::analogin, dynamic_cast<Module *>(new AnalogInController())));
        log_i("add analogin");
    }

    // eventually load the pid controller module
    if (pinConfig.pid1 >= 0 || pinConfig.pid2 >= 0 || pinConfig.pid3 >= 0)
    {
        modules.insert(std::make_pair(AvailableModules::pid, dynamic_cast<Module *>(new PidController())));
        log_i("add pid");
    }

    // eventually load the pilaserd controller module
    if (pinConfig.LASER_1 >= 0 || pinConfig.LASER_2 >= 0 || pinConfig.LASER_3 >= 0)
    {
        modules.insert(std::make_pair(AvailableModules::laser, dynamic_cast<Module *>(new LaserController())));
        log_i("add laser");
    }

    // eventually load the dac module
    if (pinConfig.dac_fake_1 >= 0 || pinConfig.dac_fake_2 >= 0)
    {
        modules.insert(std::make_pair(AvailableModules::dac, dynamic_cast<Module *>(new DacController())));
        log_i("add dac");
    }

    // eventually load the analogout module
    if (pinConfig.analogout_PIN_1 >= 0 || pinConfig.analogout_PIN_2 >= 0 || pinConfig.analogout_PIN_3 >= 0)
    {
        modules.insert(std::make_pair(AvailableModules::analogout, dynamic_cast<Module *>(new AnalogOutController())));
        log_i("add analogout");
    }

    // eventually load the digitalout module
    if (pinConfig.DIGITAL_OUT_1 >= 0 || pinConfig.DIGITAL_OUT_2 >= 0 || pinConfig.DIGITAL_OUT_3 >= 0)
    {
        modules.insert(std::make_pair(AvailableModules::digitalout, dynamic_cast<Module *>(new DigitalOutController())));
        log_i("add digitalout");
    }

    // eventually load the digitalin module
    if (pinConfig.DIGITAL_IN_1 >= 0 || pinConfig.DIGITAL_IN_2 >= 0 || pinConfig.DIGITAL_IN_3 >= 0)
    {
        modules.insert(std::make_pair(AvailableModules::digitalin, dynamic_cast<Module *>(new DigitalInController())));
        log_i("add digitalin");
    }

    // eventually load the scanner module
    if (pinConfig.enableScanner)
    {
        modules.insert(std::make_pair(AvailableModules::scanner, dynamic_cast<Module *>(new ScannerController())));
        log_i("add scanner");
    }

    // eventually load the analogJoystick module
    if (pinConfig.ANLOG_JOYSTICK_X >= 0 || pinConfig.ANLOG_JOYSTICK_Y >= 0)
    {
        modules.insert(std::make_pair(AvailableModules::analogJoystick, dynamic_cast<Module *>(new AnalogJoystick())));
        log_i("add scanner");
    }
    // call setup on all modules
    log_i("setup all modules");
    for (auto &x : modules)
    {
        log_i("setup module:%d isNull:%d", x.first, (x.second == nullptr));
        if (x.second != nullptr && x.first != AvailableModules::wifi)
            x.second->setup();
    }
    // after all mods are loaded start serving http stuff
    if (pinConfig.enableWifi)
    {
        Module *w = get(AvailableModules::wifi);
        w->setup();
    }
}

void ModuleController::loop()
{
    for (auto &x : modules)
    {
        if (x.second != nullptr && x.first != AvailableModules::analogJoystick)
        {
            x.second->loop();
            vTaskDelay(1);
        }
    }
}

Module *ModuleController::get(AvailableModules mod)
{
    return modules[mod];
}

cJSON *ModuleController::get()
{
    cJSON *doc = cJSON_CreateObject();
    cJSON *mod = cJSON_CreateObject();
    cJSON_AddItemToObject(doc, key_modules, mod);
    cJSON_AddItemToObject(mod, keyLed, cJSON_CreateNumber(pinConfig.LED_PIN >= 0));
    cJSON_AddItemToObject(mod, key_motor, cJSON_CreateNumber(pinConfig.MOTOR_ENABLE >= 0));
    cJSON_AddItemToObject(mod, key_encoder, cJSON_CreateNumber((pinConfig.X_CAL_CLK >= 0 || pinConfig.Y_CAL_CLK >= 0 || pinConfig.Z_CAL_CLK >= 0)));
    cJSON_AddItemToObject(mod, key_linearencoder, cJSON_CreateNumber((pinConfig.X_ENC_PWM >= 0 || pinConfig.Y_ENC_PWM >= 0 || pinConfig.Z_ENC_PWM >= 0)));
    cJSON_AddItemToObject(mod, key_home, cJSON_CreateNumber((pinConfig.PIN_DEF_END_X >= 0 || pinConfig.PIN_DEF_END_Y >= 0 || pinConfig.PIN_DEF_END_Z >= 0)));
    cJSON_AddItemToObject(mod, key_heat, cJSON_CreateNumber(pinConfig.DS28b20_PIN >= 0));
    cJSON_AddItemToObject(mod, key_analogin, cJSON_CreateNumber((pinConfig.analogin_PIN_0 >= 0 || pinConfig.analogin_PIN_1 >= 0 || pinConfig.analogin_PIN_2 >= 0 || pinConfig.analogin_PIN_3 >= 0)));
    cJSON_AddItemToObject(mod, key_ds18b20, cJSON_CreateNumber(pinConfig.DS28b20_PIN >= 0));
    cJSON_AddItemToObject(mod, key_image, cJSON_CreateNumber(pinConfig.IMAGE_ACTIVE >= 0));
    cJSON_AddItemToObject(mod, key_pid, cJSON_CreateNumber((pinConfig.pid1 >= 0 || pinConfig.pid2 >= 0 || pinConfig.pid3 >= 0)));
    cJSON_AddItemToObject(mod, key_laser, cJSON_CreateNumber((pinConfig.LASER_1 >= 0 || pinConfig.LASER_2 >= 0 || pinConfig.LASER_3 >= 0)));
    cJSON_AddItemToObject(mod, key_dac, cJSON_CreateNumber((pinConfig.dac_fake_1 >= 0 || pinConfig.dac_fake_2 >= 0)));
    cJSON_AddItemToObject(mod, key_analogout, cJSON_CreateNumber((pinConfig.analogout_PIN_1 >= 0 || pinConfig.analogout_PIN_2 >= 0 || pinConfig.analogout_PIN_3 >= 0)));
    cJSON_AddItemToObject(mod, key_digitalout, cJSON_CreateNumber((pinConfig.DIGITAL_OUT_1 >= 0 || pinConfig.DIGITAL_OUT_2 >= 0 || pinConfig.DIGITAL_OUT_3 >= 0)));
    cJSON_AddItemToObject(mod, key_digitalin, cJSON_CreateNumber((pinConfig.DIGITAL_IN_1 >= 0 || pinConfig.DIGITAL_IN_2 >= 0 || pinConfig.DIGITAL_IN_3 >= 0)));
    cJSON_AddItemToObject(mod, key_scanner, cJSON_CreateNumber(pinConfig.enableScanner));
    cJSON_AddItemToObject(mod, key_joy, cJSON_CreateNumber((pinConfig.ANLOG_JOYSTICK_X >= 0 || pinConfig.ANLOG_JOYSTICK_Y >= 0)));

    return doc;
}

ModuleController moduleController;
