#pragma once
#include "cJSON.h"



namespace State
{
	static bool isDEBUG = false;

	static const char *identifier_name = "UC2_Feather";
	static const char *identifier_id = "V2.0";
	static const char *identifier_date = __DATE__ "" __TIME__;
	static const char *identifier_author = "BD";
	static const char *IDENTIFIER_NAME = "uc2-esp";
	static bool config_set = false;

	// timing variables
	static unsigned long startMillis;
	static unsigned long currentMillis;
	static bool isBusy = false; // TODO this is not working!!!

	static bool OTArunning = false;
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
