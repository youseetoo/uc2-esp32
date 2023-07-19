#include "ModuleController.h"

namespace RestApi
{
    void getModules()
    {
        serialize(moduleController.get());
    }
};

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
            //x.second = nullptr;
        }
    }
    modules.clear();
    
    log_i("Using config name %s", pinConfig.pindefName);

    // eventually load the LED module
    if (pinConfig.LED_PIN > 0)
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
    modules.insert(std::make_pair(AvailableModules::state, dynamic_cast<Module *>(new  State())));
    log_i("add state");

    // eventually load the motor module
    if (pinConfig.MOTOR_ENABLE > 0)
    {
        modules.insert(std::make_pair(AvailableModules::motor, dynamic_cast<Module *>(new FocusMotor())));
        log_i("add motor");
    }

    // load the rotator module
    if (pinConfig.ROTATOR_ENABLE > 0 and (pinConfig.ROTATOR_X_0>0 or pinConfig.ROTATOR_Y_0>0 or pinConfig.ROTATOR_Z_0>0  or pinConfig.ROTATOR_A_0>0 ))
    {
        modules.insert(std::make_pair(AvailableModules::rotator, dynamic_cast<Module *>(new Rotator())));
        log_i("add rotator");
    }

    // eventually load the motor homing module
    if ( pinConfig.PIN_DEF_END_X > 0 || pinConfig.PIN_DEF_END_Y > 0 || pinConfig.PIN_DEF_END_Z > 0)
    {
        modules.insert(std::make_pair(AvailableModules::home, dynamic_cast<Module *>(new HomeMotor())));
        pinConfig.DIGITAL_IN_1=pinConfig.PIN_DEF_END_X;
        pinConfig.DIGITAL_IN_2=pinConfig.PIN_DEF_END_Y;
        pinConfig.DIGITAL_IN_3=pinConfig.PIN_DEF_END_Z;
        log_i("add home");
    }

    // eventually load the analogin module
    if (pinConfig.analogin_PIN_0 > 0 || pinConfig.analogin_PIN_1 > 0 || pinConfig.analogin_PIN_2 > 0 || pinConfig.analogin_PIN_3 > 0)
    {
        modules.insert(std::make_pair(AvailableModules::analogin, dynamic_cast<Module *>(new AnalogInController())));
        log_i("add analogin");
    }

    // eventually load the pid controller module
    if (pinConfig.pid1 > 0 || pinConfig.pid2 > 0 || pinConfig.pid3 > 0)
    {
        modules.insert(std::make_pair(AvailableModules::pid, dynamic_cast<Module *>(new PidController())));
        log_i("add pid");
    }

    // eventually load the pilaserd controller module
    if (pinConfig.LASER_1 > 0 || pinConfig.LASER_2 > 0 || pinConfig.LASER_3 > 0)
    {
        modules.insert(std::make_pair(AvailableModules::laser, dynamic_cast<Module *>(new LaserController())));
        log_i("add laser");
    }

    // eventually load the dac module
    if (pinConfig.dac_fake_1 > 0 || pinConfig.dac_fake_2 > 0)
    {
        modules.insert(std::make_pair(AvailableModules::dac, dynamic_cast<Module *>(new DacController())));
        log_i("add dac");
    }

    // eventually load the analogout module
    if (pinConfig.analogout_PIN_1 > 0 || pinConfig.analogout_PIN_2 > 0 || pinConfig.analogout_PIN_3 > 0)
    {
        modules.insert(std::make_pair(AvailableModules::analogout, dynamic_cast<Module *>(new AnalogOutController())));
        log_i("add analogout");
    }

    // eventually load the digitalout module
    if (pinConfig.DIGITAL_OUT_1 > 0 || pinConfig.DIGITAL_OUT_2 > 0 ||pinConfig.DIGITAL_OUT_3 > 0)
    {
        modules.insert(std::make_pair(AvailableModules::digitalout, dynamic_cast<Module *>(new DigitalOutController())));
        log_i("add digitalout");
    }

    // eventually load the digitalin module
    if (pinConfig.DIGITAL_IN_1 > 0 || pinConfig.DIGITAL_IN_2 > 0 ||pinConfig.DIGITAL_IN_3 > 0)
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
    if (pinConfig.ANLOG_JOYSTICK_X > 0 || pinConfig.ANLOG_JOYSTICK_Y > 0)
    {
        modules.insert(std::make_pair(AvailableModules::analogJoystick, dynamic_cast<Module *>(new AnalogJoystick())));
        log_i("add scanner");
    }


    //if wifi enable call its setup first bevor calling other mods setup
    if (pinConfig.enableWifi)
    {
        Module * w = get(AvailableModules::wifi);
        w->setup();
    }

    // call setup on all modules
    log_i("setup all modules");
    for (auto &x : modules)
    {
        log_i("setup module:%d isNull:%d", x.first,(x.second == nullptr));
        if (x.second != nullptr && x.first != AvailableModules::wifi)
            x.second->setup();
    }
    //after all mods are loaded start serving http stuff
    if (pinConfig.enableWifi)
    {
        WifiController* w = (WifiController*)get(AvailableModules::wifi);
        w->begin();
        w->createTasks();
    }

}

void ModuleController::loop()
{
    for (auto &x : modules)
    {
        if (x.second != nullptr &&  x.first != AvailableModules::analogJoystick )
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

DynamicJsonDocument ModuleController::get()
{
    DynamicJsonDocument doc(4096);
    doc[key_modules][keyLed] = pinConfig.LED_PIN > 0;
    doc[key_modules][key_motor] = pinConfig.MOTOR_ENABLE > 0;
    doc[key_modules][key_rotator] = pinConfig.ROTATOR_ENABLE > 0;
    doc[key_modules][key_home] = pinConfig.PIN_DEF_END_X > 0 || pinConfig.PIN_DEF_END_Y > 0 || pinConfig.PIN_DEF_END_Z > 0;
    doc[key_modules][key_analogin] = pinConfig.analogin_PIN_0 > 0 || pinConfig.analogin_PIN_1 > 0 || pinConfig.analogin_PIN_2 > 0 || pinConfig.analogin_PIN_3 > 0;
    doc[key_modules][key_pid] =pinConfig.pid1 > 0 || pinConfig.pid2 > 0 || pinConfig.pid3 > 0;
    doc[key_modules][key_laser] = pinConfig.LASER_1 > 0 || pinConfig.LASER_2 > 0 || pinConfig.LASER_3 > 0;
    doc[key_modules][key_dac] = pinConfig.dac_fake_1 > 0 || pinConfig.dac_fake_2 > 0;
    doc[key_modules][key_analogout] = pinConfig.analogout_PIN_1 > 0 || pinConfig.analogout_PIN_2 > 0 || pinConfig.analogout_PIN_3 > 0;
    doc[key_modules][key_digitalout] = pinConfig.DIGITAL_OUT_1 > 0 || pinConfig.DIGITAL_OUT_2 > 0 ||pinConfig.DIGITAL_OUT_3 > 0;
    doc[key_modules][key_digitalin] = pinConfig.DIGITAL_IN_1 > 0 || pinConfig.DIGITAL_IN_2 > 0 ||pinConfig.DIGITAL_IN_3 > 0;
    doc[key_modules][key_scanner] = pinConfig.enableScanner;
    doc[key_modules][key_joy] = pinConfig.ANLOG_JOYSTICK_X > 0 || pinConfig.ANLOG_JOYSTICK_Y > 0;
    return doc;
}

ModuleController moduleController;
