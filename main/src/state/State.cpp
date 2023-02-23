#include "State.h"


namespace RestApi
{
	void State_act()
	{
		moduleController.get(AvailableModules::state)->act(deserialize());
	}

	void State_get()
	{
		serialize(moduleController.get(AvailableModules::state)->set(deserialize()));
	}

	void State_set()
	{
		serialize(moduleController.get(AvailableModules::state)->set(deserialize()));
	}
}

State::State() : Module() { log_i("ctor"); }
State::~State() { log_i("~ctor"); }



void State::setup()
{
}

void State::loop()
{
}

// Custom function accessible by the API
int State::act(DynamicJsonDocument doc)
{
	// here you can do something
	if (DEBUG)
		log_i("state_act_fct");

	// assign default values to thhe variables
	if (doc.containsKey("restart"))
	{
		ESP.restart();
	}
	// assign default values to thhe variables
	if (doc.containsKey("delay"))
	{
		int mdelayms = doc["delay"];
		delay(mdelayms);
	}
	if (doc.containsKey("isBusy"))
	{
		isBusy = doc["isBusy"];
	}

	if (doc.containsKey("isRest")){
		log_i("resetPreferences");
		Preferences preferences;
		const char *prefNamespace = "UC2";
		preferences.begin(prefNamespace, false);
		preferences.clear();
		preferences.end();
		ESP.restart();
		return true;
	}

	if (doc.containsKey("pscontroller"))
	{
#if defined IS_PS3 || defined IS_PS4
		ps_c.IS_PSCONTROLER_ACTIVE = doc["pscontroller"];
#endif
	}
	doc.clear();
	doc["return"] = 1;
	return 1;
}

int State::set(DynamicJsonDocument doc)
{
	// here you can set parameters

	int isdebug = doc["isdebug"];
	DEBUG = isdebug;
	doc.clear();
	doc["return"] = 1;
	return 1;
}

// Custom function accessible by the API
DynamicJsonDocument State::get(DynamicJsonDocument docin)
{

	DynamicJsonDocument doc(4096); //StaticJsonDocument<512> doc; // create return doc

	// GET SOME PARAMETERS HERE
	if (docin.containsKey("isBusy"))
	{
		docin.clear();
		doc["isBusy"] = isBusy; // returns state of function that takes longer to finalize (e.g. motor)
	}
	else if (docin.containsKey("pscontroller"))
	{
		docin.clear();
#if defined IS_PS3 || defined IS_PS4
		doc["pscontroller"] = ps_c.IS_PSCONTROLER_ACTIVE; // returns state of function that takes longer to finalize (e.g. motor)
#endif
	}
	else
	{
		doc.clear();
		doc["identifier_name"] = identifier_name;
		doc["identifier_id"] = identifier_id;
		doc["identifier_date"] = identifier_date;
		doc["identifier_author"] = identifier_author;
		doc["IDENTIFIER_NAME"] = IDENTIFIER_NAME;
		doc["configIsSet"] = config_set; // TODO: Implement! 
		doc["pindef"] = "pindef.h";
	}
	return doc;
}

void State::printInfo()
{
	if (DEBUG)
		log_i("You can use this software by sending JSON strings, A full documentation can be found here:");
	if (DEBUG)
		log_i("https://github.com/openUC2/UC2-REST/");
	// log_i("A first try can be: \{\"task\": \"/state_get\"");
}


