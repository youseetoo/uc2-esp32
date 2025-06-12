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
	static bool IsSyncReturnBehaviourFlag = false; // this is used to switch the return message behaviour such that there is no asynchronous/event-based message sent to the client

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
	bool saveIsSyncReturnBehaviour(bool value);
	bool loadIsSyncReturnBehaviour();

	void setBusy(bool busy);
	bool getBusy();

	void startOTA();
	void stopOTA();
};
