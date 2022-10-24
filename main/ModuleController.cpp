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
        log_i("add motor");
    }    
    if (moduleConfig->slm)
    {
        modules.insert(std::make_pair(AvailableModules::slm, dynamic_cast<Module *>(new SlmController())));
        log_i("add slm");
    }
    if (moduleConfig->sensor)
    {
        modules.insert(std::make_pair(AvailableModules::sensor, dynamic_cast<Module *>(new SensorController())));
        log_i("add sensor");
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
    if (moduleConfig->analog)
    {
        modules.insert(std::make_pair(AvailableModules::analog, dynamic_cast<Module *>(new AnalogController())));
        log_i("add analog");
    }
    if (moduleConfig->digital)
    {
        modules.insert(std::make_pair(AvailableModules::digital, dynamic_cast<Module *>(new DigitalController())));
        log_i("add digital");
    }
    if (moduleConfig->scanner)
    {
        modules.insert(std::make_pair(AvailableModules::scanner, dynamic_cast<Module *>(new ScannerController())));
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
    (*jdoc)[key_modules][key_sensor] = moduleConfig->sensor;
    (*jdoc)[key_modules][key_pid] = moduleConfig->pid;
    (*jdoc)[key_modules][key_laser] = moduleConfig->laser;
    (*jdoc)[key_modules][key_dac] = moduleConfig->dac;
    (*jdoc)[key_modules][key_analog] = moduleConfig->analog;
    (*jdoc)[key_modules][key_digital] = moduleConfig->digital;
    (*jdoc)[key_modules][key_scanner] = moduleConfig->scanner;
}
// {"task":"/modules_set", "modules" : {"led" : 1, "motor": 1, "slm" : 0, "sensor" : 0, "pid" : 0, "laser" : 0, "dac" : 0, "analog" : 0, "digital" : 0, "scanner" : 0}}
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
        if ((*jdoc)[key_modules].containsKey(key_sensor))
            moduleConfig->sensor = (*jdoc)[key_modules][key_sensor];
        if ((*jdoc)[key_modules].containsKey(key_pid))
            moduleConfig->pid = (*jdoc)[key_modules][key_pid];
        if ((*jdoc)[key_modules].containsKey(key_laser))
            moduleConfig->laser = (*jdoc)[key_modules][key_laser];
        if ((*jdoc)[key_modules].containsKey(key_dac))
            moduleConfig->dac = (*jdoc)[key_modules][key_dac];
        if ((*jdoc)[key_modules].containsKey(key_analog))
            moduleConfig->analog = (*jdoc)[key_modules][key_analog];
        if ((*jdoc)[key_modules].containsKey(key_digital))
            moduleConfig->digital = (*jdoc)[key_modules][key_digital];
        if ((*jdoc)[key_modules].containsKey(key_scanner))
            moduleConfig->scanner = (*jdoc)[key_modules][key_scanner];
        Config::setModuleConfig(moduleConfig);
        setup();
        WifiController::restartWebServer();
    }
    jdoc->clear();
}

ModuleController moduleController;