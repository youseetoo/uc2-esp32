#pragma once

#include "../wifi/Endpoints.h"
#include "../../AvailableModules.h"
struct ModulesToCheck
{
	const char *act;
	const char *get;
	AvailableModules mod;
};

ModulesToCheck modulesToCheck[] = {
	[0] = {.act = state_act_endpoint,
		   .get = state_get_endpoint,
		   .mod = AvailableModules::state},
	[1] = {.act = motor_act_endpoint,
		   .get = motor_get_endpoint,
		   .mod = AvailableModules::motor},
	[2] = {.act = home_act_endpoint,
		   .get = home_get_endpoint,
		   .mod = AvailableModules::home},
	[3] = {.act = encoder_act_endpoint,
		   .get = encoder_get_endpoint,
		   .mod = AvailableModules::encoder},
	[4] = {.act = dac_act_endpoint,
		   .get = dac_get_endpoint,
		   .mod = AvailableModules::dac},
	[5] = {.act = laser_act_endpoint,
		   .get = laser_get_endpoint,
		   .mod = AvailableModules::laser},
	[6] = {.act = analogout_act_endpoint,
		   .get = analogout_get_endpoint,
		   .mod = AvailableModules::analogout},
	[7] = {.act = digitalout_act_endpoint,
		   .get = digitalout_get_endpoint,
		   .mod = AvailableModules::digitalout},
	[8] = {.act = digitalin_act_endpoint,
		   .get = digitalin_get_endpoint,
		   .mod = AvailableModules::digitalin},
	[9] = {.act = ledarr_act_endpoint,
		   .get = ledarr_get_endpoint,
		   .mod = AvailableModules::led},
	[10] = {.act = readanalogin_act_endpoint,
			.get = readanalogin_get_endpoint,
			.mod = AvailableModules::analogin},
	[11] = {.act = PID_act_endpoint,
			.get = PID_get_endpoint,
			.mod = AvailableModules::pid},
};