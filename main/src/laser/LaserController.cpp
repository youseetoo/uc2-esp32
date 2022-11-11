#include "../../config.h"
#include "LaserController.h"

namespace RestApi
{
	void Laser_act()
	{
		deserialize();
		if (moduleController.get(AvailableModules::laser) != nullptr)
			moduleController.get(AvailableModules::laser)->act();
		else
			log_i("laser controller is null!");
		serialize();
	}

	void Laser_get()
	{
		deserialize();
		if (moduleController.get(AvailableModules::laser) != nullptr)
			moduleController.get(AvailableModules::laser)->get();
		else
			log_i("laser controller is null!");
		serialize();
	}

	void Laser_set()
	{
		log_i("laser set!");
		deserialize();
		if (moduleController.get(AvailableModules::laser) != nullptr)
			moduleController.get(AvailableModules::laser)->set();
		else
			log_i("laser controller is null!");
		serialize();
	}
}

LaserController::LaserController(/* args */)
{
}

LaserController::~LaserController()
{
}

void LaserController::LASER_despeckle(int LASERdespeckle, int LASERid, int LASERperiod)
{

	if (!isBusy)
	{

		int LASER_val_wiggle = 0;
		int PWM_CHANNEL_LASER = 0;
		if (LASERid == 1)
		{
			LASER_val_wiggle = LASER_val_1;
			PWM_CHANNEL_LASER = PWM_CHANNEL_LASER_1;
		}
		else if (LASERid == 2)
		{
			LASER_val_wiggle = LASER_val_2;
			PWM_CHANNEL_LASER = PWM_CHANNEL_LASER_2;
		}
		else if (LASERid == 3)
		{
			LASER_val_wiggle = LASER_val_3;
			PWM_CHANNEL_LASER = PWM_CHANNEL_LASER_3;
		}
		// add random number to current value to let it oscliate
		long laserwiggle = random(-LASERdespeckle, LASERdespeckle);
		LASER_val_wiggle += laserwiggle;
		if (LASER_val_wiggle > pwm_max)
			LASER_val_wiggle -= (2 * abs(laserwiggle));
		if (LASER_val_wiggle < 0)
			LASER_val_wiggle += (2 * abs(laserwiggle));

		if (DEBUG)
			Serial.println(LASERid);
		if (DEBUG)
			Serial.println(LASER_val_wiggle);

		ledcWrite(PWM_CHANNEL_LASER, LASER_val_wiggle);

		delay(LASERperiod);
	}
}

// Custom function accessible by the API
void LaserController::act()
{
	// here you can do something
	Serial.println("LASER_act_fct");

	isBusy = true;

	int LASERid = (*WifiController::getJDoc())["LASERid"];
	int LASERval = (*WifiController::getJDoc())["LASERval"];
	int LASERdespeckle = (*WifiController::getJDoc())["LASERdespeckle"];
	int LASERdespecklePeriod = 20;
	if (WifiController::getJDoc()->containsKey("LASERdespecklePeriod"))
	{
		LASERdespecklePeriod = (*WifiController::getJDoc())["LASERdespecklePeriod"];
	}

	if (DEBUG)
	{
		Serial.print("LASERid ");
		Serial.println(LASERid);
		Serial.print("LASERval ");
		Serial.println(LASERval);
		Serial.print("LASERdespeckle ");
		Serial.println(LASERdespeckle);
		Serial.print("LASERdespecklePeriod ");
		Serial.println(LASERdespecklePeriod);
	}
	
	// clear document
	WifiController::getJDoc()->clear();

	if (LASERid == 1 && pins.LASER_PIN_1 != 0)
	{
		LASER_val_1 = LASERval;
		LASER_despeckle_1 = LASERdespeckle;
		LASER_despeckle_period_1 = LASERdespecklePeriod;
		if (DEBUG)
		{
			Serial.print("LaserPIN ");
			Serial.println(pins.LASER_PIN_1);
		}
		ledcWrite(PWM_CHANNEL_LASER_1, LASERval);
		(*WifiController::getJDoc())[key_return] = 1;
	}
	else if (LASERid == 2  && pins.LASER_PIN_2 != 0)
	{
		LASER_val_2 = LASERval;
		LASER_despeckle_2 = LASERdespeckle;
		LASER_despeckle_period_2 = LASERdespecklePeriod;
		if (DEBUG)
		{
			Serial.print("LaserPIN ");
			Serial.println(pins.LASER_PIN_2);
		}
		ledcWrite(PWM_CHANNEL_LASER_2, LASERval);
		(*WifiController::getJDoc())[key_return] = 1;
	}
	else if (LASERid == 3  && pins.LASER_PIN_3 != 0)
	{
		LASER_val_3 = LASERval;
		LASER_despeckle_3 = LASERdespeckle;
		LASER_despeckle_period_3 = LASERdespecklePeriod;
		if (DEBUG)
		{
			Serial.print("LaserPIN ");
			Serial.println(pins.LASER_PIN_3);
		}
		ledcWrite(PWM_CHANNEL_LASER_3, LASERval);
		(*WifiController::getJDoc())[key_return] = 1;
	}
	else{
		(*WifiController::getJDoc())[key_return] = 0;
	}


	isBusy = false;
}

void LaserController::set()
{
	// here you can set parameters
	if (WifiController::getJDoc()->containsKey("LASERid") && WifiController::getJDoc()->containsKey("LASERpin"))
	{
		int LASERid = (*WifiController::getJDoc())["LASERid"];
		int LASERpin = (*WifiController::getJDoc())["LASERpin"];
		log_i("LaserId: %i Pin:%i", LASERid, LASERpin);
		if (LASERpin != 0)
		{
			if (LASERid == 1)
			{
				pins.LASER_PIN_1 = LASERpin;
				pinMode(pins.LASER_PIN_1, OUTPUT);
				digitalWrite(pins.LASER_PIN_1, LOW);
				/* setup the PWM ports and reset them to 0*/
				ledcSetup(PWM_CHANNEL_LASER_1, pwm_frequency, pwm_resolution);
				ledcAttachPin(pins.LASER_PIN_1, PWM_CHANNEL_LASER_1);
				ledcWrite(PWM_CHANNEL_LASER_1, 0);
			}
			else if (LASERid == 2)
			{
				pins.LASER_PIN_2 = LASERpin;
				pinMode(pins.LASER_PIN_2, OUTPUT);
				digitalWrite(pins.LASER_PIN_2, LOW);
				/* setup the PWM ports and reset them to 0*/
				ledcSetup(PWM_CHANNEL_LASER_2, pwm_frequency, pwm_resolution);
				ledcAttachPin(pins.LASER_PIN_2, PWM_CHANNEL_LASER_2);
				ledcWrite(PWM_CHANNEL_LASER_2, 0);
			}
			else if (LASERid == 3)
			{
				pins.LASER_PIN_3 = LASERpin;
				pinMode(pins.LASER_PIN_3, OUTPUT);
				digitalWrite(pins.LASER_PIN_3, LOW);
				/* setup the PWM ports and reset them to 0*/
				ledcSetup(PWM_CHANNEL_LASER_3, pwm_frequency, pwm_resolution);
				ledcAttachPin(pins.LASER_PIN_3, PWM_CHANNEL_LASER_3);
				ledcWrite(PWM_CHANNEL_LASER_3, 0);
			}
		}
		Config::setLaserPins(pins);
	}
	WifiController::getJDoc()->clear();
}

// Custom function accessible by the API
void LaserController::get()
{
	WifiController::getJDoc()->clear();
			int LASERid = (*WifiController::getJDoc())["LASERid"];
		int LASERpin = (*WifiController::getJDoc())["LASERpin"];
	(*WifiController::getJDoc())["LASER1pin"] = pins.LASER_PIN_1;
	(*WifiController::getJDoc())["LASER2pin"] = pins.LASER_PIN_2;
	(*WifiController::getJDoc())["LASER3pin"] = pins.LASER_PIN_3;
}

void LaserController::setup()
{
	Serial.println("Setting Up LASERs");

	Config::getLaserPins(pins);
	// switch of the LASER directly
	if (pins.LASER_PIN_1 != 0)
	{
		pinMode(pins.LASER_PIN_1, OUTPUT);
		digitalWrite(pins.LASER_PIN_1, LOW);
		ledcSetup(PWM_CHANNEL_LASER_1, pwm_frequency, pwm_resolution);
		ledcAttachPin(pins.LASER_PIN_1, PWM_CHANNEL_LASER_1);
		ledcWrite(PWM_CHANNEL_LASER_1, 10000);
		delay(500);
		ledcWrite(PWM_CHANNEL_LASER_1, 0);
	}

	if (pins.LASER_PIN_2 != 0)
	{
		pinMode(pins.LASER_PIN_2, OUTPUT);
		digitalWrite(pins.LASER_PIN_2, LOW);
		ledcSetup(PWM_CHANNEL_LASER_2, pwm_frequency, pwm_resolution);
		ledcAttachPin(pins.LASER_PIN_2, PWM_CHANNEL_LASER_2);
		ledcWrite(PWM_CHANNEL_LASER_2, 10000);
		delay(500);
		ledcWrite(PWM_CHANNEL_LASER_2, 0);
	}

	if (pins.LASER_PIN_3 != 0)
	{
		pinMode(pins.LASER_PIN_3, OUTPUT);
		digitalWrite(pins.LASER_PIN_3, LOW);
		ledcSetup(PWM_CHANNEL_LASER_3, pwm_frequency, pwm_resolution);
		ledcAttachPin(pins.LASER_PIN_3, PWM_CHANNEL_LASER_3);
		ledcWrite(PWM_CHANNEL_LASER_3, 10000);
		delay(500);
		ledcWrite(PWM_CHANNEL_LASER_3, 0);
	}
}

void LaserController::loop()
{
	// attempting to despeckle by wiggeling the temperature-dependent modes of the laser?
	if (LASER_despeckle_1 > 0 && LASER_val_1 > 0 && pins.LASER_PIN_1 != 0)
		LASER_despeckle(LASER_despeckle_1, 1, LASER_despeckle_period_1);
	if (LASER_despeckle_2 > 0 && LASER_val_2 > 0 && pins.LASER_PIN_2 != 0)
		LASER_despeckle(LASER_despeckle_2, 2, LASER_despeckle_period_2);
	if (LASER_despeckle_3 > 0 && LASER_val_3 > 0 && pins.LASER_PIN_3 != 0)
		LASER_despeckle(LASER_despeckle_3, 3, LASER_despeckle_period_3);
}
