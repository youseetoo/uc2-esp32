#include "ModuleController.h"

namespace RestApi
{
    void getModules()
    {
        deserialize();
        moduleController.get();
        serialize();
    }
    void setModules()
    {
        deserialize();
        moduleController.set();
        serialize();
    }
};

void ModuleController::setup()
{

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
    if (moduleConfig->led)
    {
        modules.insert(std::make_pair(AvailableModules::led, dynamic_cast<Module *>(new LedController())));
        log_i("add led");
    }
    if (moduleConfig->motor)
    {
        modules.insert(std::make_pair(AvailableModules::motor, dynamic_cast<Module *>(new FocusMotor())));
        log_i("add motor");
    }
    if (moduleConfig->home)
    {
        modules.insert(std::make_pair(AvailableModules::home, dynamic_cast<Module *>(new HomeMotor())));
        log_i("add home");
    }    
    if (moduleConfig->slm)
    {
        modules.insert(std::make_pair(AvailableModules::slm, dynamic_cast<Module *>(new SlmController())));
        log_i("add slm");
    }
    if (moduleConfig->analogin)
    {
        modules.insert(std::make_pair(AvailableModules::analogin, dynamic_cast<Module *>(new AnalogInController())));
        log_i("add analogin");
    }
    if (moduleConfig->pid)
    {
        modules.insert(std::make_pair(AvailableModules::pid, dynamic_cast<Module *>(new PidController())));
        log_i("add pid");
    }
    if (moduleConfig->laser)
    {
        modules.insert(std::make_pair(AvailableModules::laser, dynamic_cast<Module *>(new LaserController())));
        log_i("add laser");
    }
    if (moduleConfig->dac)
    {
        modules.insert(std::make_pair(AvailableModules::dac, dynamic_cast<Module *>(new DacController())));
        log_i("add dac");
    }
    if (moduleConfig->analogout)
    {
        modules.insert(std::make_pair(AvailableModules::analogout, dynamic_cast<Module *>(new AnalogOutController())));
        log_i("add analogout");
    }
    if (moduleConfig->digitalout)
    {
        modules.insert(std::make_pair(AvailableModules::digitalout, dynamic_cast<Module *>(new DigitalOutController())));
        log_i("add digitalout");
    }
    if (moduleConfig->digitalin)
    {
        modules.insert(std::make_pair(AvailableModules::digitalin, dynamic_cast<Module *>(new DigitalInController())));
        log_i("add digitalin");
    }
    if (moduleConfig->scanner)
    {
        modules.insert(std::make_pair(AvailableModules::scanner, dynamic_cast<Module *>(new ScannerController())));
        log_i("add scanner");
    }
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

void ModuleController::get()
{
    DynamicJsonDocument *jdoc = WifiController::getJDoc();
    jdoc->clear();
    (*jdoc)[key_modules][keyLed] = moduleConfig->led;
    (*jdoc)[key_modules][key_motor] = moduleConfig->motor;
    (*jdoc)[key_modules][key_home] = moduleConfig->home;
    (*jdoc)[key_modules][key_slm] = moduleConfig->slm;
    (*jdoc)[key_modules][key_analogin] = moduleConfig->analogin;
    (*jdoc)[key_modules][key_pid] = moduleConfig->pid;
    (*jdoc)[key_modules][key_laser] = moduleConfig->laser;
    (*jdoc)[key_modules][key_dac] = moduleConfig->dac;
    (*jdoc)[key_modules][key_analogout] = moduleConfig->analogout;
    (*jdoc)[key_modules][key_digitalout] = moduleConfig->digitalout;
    (*jdoc)[key_modules][key_digitalin] = moduleConfig->digitalin;
    (*jdoc)[key_modules][key_scanner] = moduleConfig->scanner;
    (*jdoc)[key_modules][key_joy] = moduleConfig->analogJoystick;
}
// {"task":"/modules_set", "modules" : {"led" : 1, "motor": 1, "slm" : 0, "analogin" : 0, "pid" : 0, "laser" : 0, "dac" : 0, "analogout" : 0, "digitalout" : 0, "digitalin" : 0, "scanner" : 0}}
void ModuleController::set()
{
    DynamicJsonDocument *jdoc = WifiController::getJDoc();
    if (jdoc->containsKey(key_modules))
    {
        if ((*jdoc)[key_modules].containsKey(keyLed))
            moduleConfig->led = (*jdoc)[key_modules][keyLed];
        if ((*jdoc)[key_modules].containsKey(key_motor))
            moduleConfig->motor = (*jdoc)[key_modules][key_motor];
        if ((*jdoc)[key_modules].containsKey(key_home))
            moduleConfig->home = (*jdoc)[key_modules][key_home];
        if ((*jdoc)[key_modules].containsKey(key_slm))
            moduleConfig->slm = (*jdoc)[key_modules][key_slm];
        if ((*jdoc)[key_modules].containsKey(key_analogin))
            moduleConfig->analogin = (*jdoc)[key_modules][key_analogin];
        if ((*jdoc)[key_modules].containsKey(key_pid))
            moduleConfig->pid = (*jdoc)[key_modules][key_pid];
        if ((*jdoc)[key_modules].containsKey(key_laser))
            moduleConfig->laser = (*jdoc)[key_modules][key_laser];
        if ((*jdoc)[key_modules].containsKey(key_dac))
            moduleConfig->dac = (*jdoc)[key_modules][key_dac];
        if ((*jdoc)[key_modules].containsKey(key_analogout))
            moduleConfig->analogout = (*jdoc)[key_modules][key_analogout];
        if ((*jdoc)[key_modules].containsKey(key_digitalout))
            moduleConfig->digitalout = (*jdoc)[key_modules][key_digitalout];
        if ((*jdoc)[key_modules].containsKey(key_digitalin))
            moduleConfig->digitalin = (*jdoc)[key_modules][key_digitalin];
        if ((*jdoc)[key_modules].containsKey(key_scanner))
            moduleConfig->scanner = (*jdoc)[key_modules][key_scanner];
        if ((*jdoc)[key_modules].containsKey(key_joy))
            moduleConfig->analogJoystick = (*jdoc)[key_modules][key_joy];
        Config::setModuleConfig(moduleConfig);
        setup();
        WifiController::restartWebServer();
    }
    jdoc->clear();
}

ModuleController moduleController;