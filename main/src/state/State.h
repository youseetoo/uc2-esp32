#pragma once
#include "../../Module.h"


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
	bool isSending = false;

	int act(cJSON * ob) override;
	cJSON *  get(cJSON *  ob) override;

	void setup() override;
	void printInfo();
	void loop() override;
};
