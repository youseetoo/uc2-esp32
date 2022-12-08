#include "ModuleController.h"

namespace RestApi
{
    void getModules()
    {
        serialize(moduleController.get());
    }
    void setModules()
    {
        serialize(moduleController.set(deserialize()));
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
    moduleConfig = Config::getModuleConfig();
    
    // eventually load the LED module
    if (moduleConfig->led)
    {
        modules.insert(std::make_pair(AvailableModules::led, dynamic_cast<Module *>(new LedController())));
        log_i("add led");
    }

    // eventually load the BTController module
    /*
    if (moduleConfig->btcontroller)
    {
        modules.insert(std::make_pair(AvailableModules::btcontroller, dynamic_cast<Module *>(new BTController())));
        log_i("add btcontroller");
    }*/

    // eventually load the state module
    if (moduleConfig->state)
    {
        modules.insert(std::make_pair(AvailableModules::state, dynamic_cast<Module *>(new  State())));
        log_i("add state");
    }

    // eventually load the motor module
    if (moduleConfig->motor)
    {
        modules.insert(std::make_pair(AvailableModules::motor, dynamic_cast<Module *>(new FocusMotor())));
        log_i("add motor");
    }

    // eventually load the motor homing module
    if (moduleConfig->home)
    {
        modules.insert(std::make_pair(AvailableModules::home, dynamic_cast<Module *>(new HomeMotor())));
        log_i("add home");
    }


    // eventually load the analogin module
    if (moduleConfig->analogin)
    {
        modules.insert(std::make_pair(AvailableModules::analogin, dynamic_cast<Module *>(new AnalogInController())));
        log_i("add analogin");
    }

    // eventually load the pid controller module
    if (moduleConfig->pid)
    {
        modules.insert(std::make_pair(AvailableModules::pid, dynamic_cast<Module *>(new PidController())));
        log_i("add pid");
    }

    // eventually load the pilaserd controller module
    if (moduleConfig->laser)
    {
        modules.insert(std::make_pair(AvailableModules::laser, dynamic_cast<Module *>(new LaserController())));
        log_i("add laser");
    }

    // eventually load the dac module
    if (moduleConfig->dac)
    {
        modules.insert(std::make_pair(AvailableModules::dac, dynamic_cast<Module *>(new DacController())));
        log_i("add dac");
    }

    // eventually load the analogout module
    if (moduleConfig->analogout)
    {
        modules.insert(std::make_pair(AvailableModules::analogout, dynamic_cast<Module *>(new AnalogOutController())));
        log_i("add analogout");
    }

    // eventually load the digitalout module
    if (moduleConfig->digitalout)
    {
        modules.insert(std::make_pair(AvailableModules::digitalout, dynamic_cast<Module *>(new DigitalOutController())));
        log_i("add digitalout");
    }

    // eventually load the digitalin module
    if (moduleConfig->digitalin)
    {
        modules.insert(std::make_pair(AvailableModules::digitalin, dynamic_cast<Module *>(new DigitalInController())));
        log_i("add digitalin");
    }

    // eventually load the scanner module
    if (moduleConfig->scanner)
    {
        modules.insert(std::make_pair(AvailableModules::scanner, dynamic_cast<Module *>(new ScannerController())));
        log_i("add scanner");
    }

    // eventually load the analogJoystick module
    if (moduleConfig->analogJoystick)
    {
        modules.insert(std::make_pair(AvailableModules::analogJoystick, dynamic_cast<Module *>(new AnalogJoystick())));
        log_i("add scanner");
    }

    for (auto &x : modules)
    {
        if (x.second != nullptr)
            x.second->setup();
    }
}

void ModuleController::loop()
{
    for (auto &x : modules)
    {
        if (x.second != nullptr)
        {
             x.second->loop();
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
    doc[key_modules][keyLed] = moduleConfig->led;
    doc[key_modules][key_motor] = moduleConfig->motor;
    doc[key_modules][key_home] = moduleConfig->home;
    doc[key_modules][key_analogin] = moduleConfig->analogin;
    doc[key_modules][key_pid] = moduleConfig->pid;
    doc[key_modules][key_laser] = moduleConfig->laser;
    doc[key_modules][key_dac] = moduleConfig->dac;
    doc[key_modules][key_analogout] = moduleConfig->analogout;
    doc[key_modules][key_digitalout] = moduleConfig->digitalout;
    doc[key_modules][key_digitalin] = moduleConfig->digitalin;
    doc[key_modules][key_scanner] = moduleConfig->scanner;
    doc[key_modules][key_joy] = moduleConfig->analogJoystick;
    doc[key_modules][key_wifi] = moduleConfig->wifi;
    return doc;
}
// {"task":"/modules_set", "modules" : {"led" : 1, "motor": 1, "analogin" : 0, "pid" : 0, "laser" : 0, "dac" : 0, "analogout" : 0, "digitalout" : 0, "digitalin" : 0, "scanner" : 0}}
int ModuleController::set(DynamicJsonDocument j)
{
    // set active modules
    if (j.containsKey(key_modules))
    {
        if (j[key_modules].containsKey(keyLed))
            moduleConfig->led = j[key_modules][keyLed];
        if (j[key_modules].containsKey(key_motor))
            moduleConfig->motor = j[key_modules][key_motor];
        if (j[key_modules].containsKey(key_home))
            moduleConfig->home = j[key_modules][key_home];
        if (j[key_modules].containsKey(key_analogin))
            moduleConfig->analogin = j[key_modules][key_analogin];
        if (j[key_modules].containsKey(key_pid))
            moduleConfig->pid = j[key_modules][key_pid];
        if (j[key_modules].containsKey(key_laser))
            moduleConfig->laser = j[key_modules][key_laser];
        if (j[key_modules].containsKey(key_dac))
            moduleConfig->dac = j[key_modules][key_dac];
        if (j[key_modules].containsKey(key_analogout))
            moduleConfig->analogout = j[key_modules][key_analogout];
        if (j[key_modules].containsKey(key_digitalout))
            moduleConfig->digitalout = j[key_modules][key_digitalout];
        if (j[key_modules].containsKey(key_digitalin))
            moduleConfig->digitalin = j[key_modules][key_digitalin];
        if (j[key_modules].containsKey(key_scanner))
            moduleConfig->scanner = j[key_modules][key_scanner];
        if (j[key_modules].containsKey(key_joy))
            moduleConfig->analogJoystick = j[key_modules][key_joy];
        if (j[key_modules].containsKey(key_wifi))
            moduleConfig->wifi = j[key_modules][key_wifi];
                        
        Config::setModuleConfig(moduleConfig);
        setup();
        WifiController::restartWebServer();
    }
    return 1;
}

ModuleController moduleController;
