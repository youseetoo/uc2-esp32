#include "ModuleController.h"

void ModuleController::setup()
{
    #ifdef IS_LED
        modules.insert(std::make_pair(AvailableModules::led, dynamic_cast<Module*>(new LedController())));
    #endif
}

void ModuleController::loop()
{
    for (auto const& x : modules)
    {
        x.second->loop();
    }
}

Module * ModuleController::get(AvailableModules mod)
{
   return modules[mod];
}

ModuleController moduleController;