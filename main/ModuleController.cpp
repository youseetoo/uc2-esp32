#include "ModuleController.h"

void ModuleController::setup()
{
    moduleConfig.led = true;
    moduleConfig.motor = true;
    if (moduleConfig.led)
    {
        modules.insert(std::make_pair(AvailableModules::led, dynamic_cast<Module *>(new LedController())));
        log_i("add led");
    }
    if (moduleConfig.motor){
        modules.insert(std::make_pair(AvailableModules::motor, dynamic_cast<Module *>(new FocusMotor())));
        log_i("add motor");
    }
    if (moduleConfig.slm)
        modules.insert(std::make_pair(AvailableModules::slm, dynamic_cast<Module *>(new SlmController())));
    if (moduleConfig.sensor)
        modules.insert(std::make_pair(AvailableModules::sensor, dynamic_cast<Module *>(new SensorController())));
    if (moduleConfig.pid)
        modules.insert(std::make_pair(AvailableModules::pid, dynamic_cast<Module *>(new PidController())));
    if (moduleConfig.laser)
        modules.insert(std::make_pair(AvailableModules::laser, dynamic_cast<Module *>(new LaserController())));
    if (moduleConfig.dac)
        modules.insert(std::make_pair(AvailableModules::dac, dynamic_cast<Module *>(new DacController())));
    if (moduleConfig.analog)
        modules.insert(std::make_pair(AvailableModules::analog, dynamic_cast<Module *>(new AnalogController())));
    if (moduleConfig.digital)
        modules.insert(std::make_pair(AvailableModules::digital, dynamic_cast<Module *>(new DigitalController())));
    if (moduleConfig.scanner)
        modules.insert(std::make_pair(AvailableModules::scanner, dynamic_cast<Module *>(new ScannerController())));
    for (auto const &x : modules)
    {
        x.second->setup();
    }
}

void ModuleController::loop()
{
    for (auto const &x : modules)
    {
        if (x.second == nullptr)
        {
            //log_e("second is null, wtf.. module count:%i", modules.size());
            return;
        }
        x.second->loop();
    }
}

Module *ModuleController::get(AvailableModules mod)
{
    return modules[mod];
}

ModuleController moduleController;