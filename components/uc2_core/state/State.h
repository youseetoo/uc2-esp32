#pragma once
#include "cJSON.h"



namespace State
{
	// T1.3: per-TU namespace statics (each includer gets its own copy);
	// [[maybe_unused]] because not every TU reads every member.
	[[maybe_unused]] static bool isDEBUG = false;

	[[maybe_unused]] static const char *identifier_name = "UC2_Feather";
	[[maybe_unused]] static const char *identifier_id = "V2.0";
	[[maybe_unused]] static const char *identifier_date = __DATE__ "" __TIME__;
	[[maybe_unused]] static const char *identifier_author = "BD";
	[[maybe_unused]] static const char *IDENTIFIER_NAME = "uc2-esp";
	[[maybe_unused]] static bool config_set = false;

	// timing variables
	[[maybe_unused]] static unsigned long startMillis;
	[[maybe_unused]] static unsigned long currentMillis;
	[[maybe_unused]] static bool isBusy = false; // TODO this is not working!!!

	[[maybe_unused]] static bool OTArunning = false;
	int act(cJSON * ob);
	cJSON *  get(cJSON *  ob);
	cJSON *getModules();

	void setup();
	void printInfo();

	void setBusy(bool busy);
	bool getBusy();

	// CAN-bus power control (BUSPOWER_OFF_PIN gate; HIGH = OFF, LOW = ON).
	// Default ON at boot. Exposed via /state_act {"power":0|1} and /state_get.
	// setBusPower is also called by the E-stop handler to cut all slave power.
	void setBusPower(bool on);
	bool getBusPower();

	void startOTA();
	void stopOTA();
};
