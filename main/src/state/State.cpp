#include "State.h"

namespace RestApi
{
	void State_act()
	{
		deserialize();
		state.act();
		serialize();
	}

	void State_get()
	{
		deserialize();
		state.get();
		serialize();
	}

	void State_set()
	{
		deserialize();
		state.set();
		serialize();
	}
}

State::State(){};
State::~State(){};

void State::setup()
{
}

// Custom function accessible by the API
void State::act()
{
	// here you can do something
	if (DEBUG)
		log_i("state_act_fct");

	// assign default values to thhe variables
	if ((WifiController::getJDoc())->containsKey("restart"))
	{
		ESP.restart();
	}
	// assign default values to thhe variables
	if (WifiController::getJDoc()->containsKey("delay"))
	{
		int mdelayms = (*WifiController::getJDoc())["delay"];
		delay(mdelayms);
	}
	if (WifiController::getJDoc()->containsKey("isBusy"))
	{
		isBusy = (*WifiController::getJDoc())["isBusy"];
	}

	if (WifiController::getJDoc()->containsKey("pscontroller"))
	{
#if defined IS_PS3 || defined IS_PS4
		ps_c.IS_PSCONTROLER_ACTIVE = (*WifiController::getJDoc())["pscontroller"];
#endif
	}
	WifiController::getJDoc()->clear();
	(*WifiController::getJDoc())["return"] = 1;
}

void State::set()
{
	// here you can set parameters

	int isdebug = (*WifiController::getJDoc())["isdebug"];
	DEBUG = isdebug;
	WifiController::getJDoc()->clear();
	(*WifiController::getJDoc())["return"] = 1;
}

// Custom function accessible by the API
void State::get()
{
	// GET SOME PARAMETERS HERE
	if (WifiController::getJDoc()->containsKey("isBusy"))
	{
		WifiController::getJDoc()->clear();
		(*WifiController::getJDoc())["isBusy"] = isBusy; // returns state of function that takes longer to finalize (e.g. motor)
	}

	else if (WifiController::getJDoc()->containsKey("pscontroller"))
	{
		WifiController::getJDoc()->clear();
#if defined IS_PS3 || defined IS_PS4
		(*WifiController::getJDoc())["pscontroller"] = ps_c.IS_PSCONTROLER_ACTIVE; // returns state of function that takes longer to finalize (e.g. motor)
#endif
	}
	else
	{
		WifiController::getJDoc()->clear();
		(*WifiController::getJDoc())["identifier_name"] = identifier_name;
		(*WifiController::getJDoc())["identifier_id"] = identifier_id;
		(*WifiController::getJDoc())["identifier_date"] = identifier_date;
		(*WifiController::getJDoc())["identifier_author"] = identifier_author;
		//(*jsonDocument)["identifier_setup"] = pins->identifier_setup;
		(*WifiController::getJDoc())["IDENTIFIER_NAME"] = IDENTIFIER_NAME;
		(*WifiController::getJDoc())["configIsSet"] = config_set; // TODO: Implement! 
	}
}

void State::printInfo()
{
	if (DEBUG)
		log_i("You can use this software by sending JSON strings, A full documentation can be found here:");
	if (DEBUG)
		log_i("https://github.com/openUC2/UC2-REST/");
	// log_i("A first try can be: \{\"task\": \"/state_get\"");
}

State state;
