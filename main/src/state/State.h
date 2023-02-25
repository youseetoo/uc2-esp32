#pragma once
#include "ArduinoJson.h"
#include "../config/JsonKeys.h"
#include "esp_log.h"
#if defined IS_PS3 || defined IS_PS4
#endif
#include "../wifi/WifiController.h"
#include "../config/ConfigController.h"

namespace RestApi
{
	void State_act();
    void State_get();
};



class State : public Module
{
public:
	State();
	~State();
	bool DEBUG = false;

	const char *identifier_name = "UC2_Feather";
	const char *identifier_id = "V2.0";
	const char *identifier_date = __DATE__ "" __TIME__;
	const char *identifier_author = "BD";
	const char *IDENTIFIER_NAME = "uc2-esp";
	bool config_set = false;

	// timing variables
	unsigned long startMillis;
	unsigned long currentMillis;
	bool isBusy = false; // TODO this is not working!!!

	int act(DynamicJsonDocument  ob) override;
	DynamicJsonDocument get(DynamicJsonDocument ob) override;

	void setup() override;
	void printInfo();
	void loop() override;
};
