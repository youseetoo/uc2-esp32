#include "../../config.h"
#include "LaserController.h"
#include "../../pindef.h"

namespace RestApi
{
	void Laser_act()
	{
		if (moduleController.get(AvailableModules::laser) != nullptr)
			serialize(moduleController.get(AvailableModules::laser)->act(deserialize()));
		else
			log_i("laser controller is null!");
	}

	void Laser_get()
	{
		if (moduleController.get(AvailableModules::laser) != nullptr)
			serialize(moduleController.get(AvailableModules::laser)->get(deserialize()));
		else
			log_i("laser controller is null!");
	}

	void Laser_set()
	{
		log_i("laser set!");
		if (moduleController.get(AvailableModules::laser) != nullptr)
			serialize(moduleController.get(AvailableModules::laser)->set(deserialize()));
		else
			log_i("laser controller is null!");
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
int LaserController::act(DynamicJsonDocument ob)
{
	// here you can do something
	Serial.println("LASER_act_fct");

	isBusy = true;

	int LASERid = (ob)["LASERid"];
	int LASERval = (ob)["LASERval"];
	int LASERdespeckle = (ob)["LASERdespeckle"];
	int LASERdespecklePeriod = 20;
	if (ob.containsKey("LASERdespecklePeriod"))
	{
		LASERdespecklePeriod = (ob)["LASERdespecklePeriod"];
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
	return 1;
}

int LaserController::set(DynamicJsonDocument ob)
{
	// here you can set parameters
	if (ob.containsKey("LASERid") && ob.containsKey("LASERpin"))
	{
		int LASERid = (ob)["LASERid"];
		int LASERpin = (ob)["LASERpin"];
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
	return 1;
}

// Custom function accessible by the API
DynamicJsonDocument LaserController::get(DynamicJsonDocument  ob)
{
	ob.clear();
	ob["LASER1pin"] = pins.LASER_PIN_1;
	ob["LASER2pin"] = pins.LASER_PIN_2;
	ob["LASER3pin"] = pins.LASER_PIN_3;
	return ob;
}

void LaserController::setup()
{
	log_i("Setting Up LASERs");

	Config::getLaserPins(pins);

	// Setting up the differen PWM channels for the laser

	// if laser pin is not defined try loading it from the pindef.h file
	if (not pins.LASER_PIN_1)
	{
		pins.LASER_PIN_1 = PIN_DEF_LASER_1; // default value
	}
	log_i("Laser ID 1, pin: %i", pins.LASER_PIN_1);
	pinMode(pins.LASER_PIN_1, OUTPUT);
	digitalWrite(pins.LASER_PIN_1, LOW);
	ledcSetup(PWM_CHANNEL_LASER_1, pwm_frequency, pwm_resolution);
	ledcAttachPin(pins.LASER_PIN_1, PWM_CHANNEL_LASER_1);
	ledcWrite(PWM_CHANNEL_LASER_1, 10000);
	delay(500);
	ledcWrite(PWM_CHANNEL_LASER_1, 0);

	// if laser pin is not defined try loading it from the pindef.h file
	if (not pins.LASER_PIN_2)
	{
		pins.LASER_PIN_2 = PIN_DEF_LASER_2; // default value
	}
	log_i("Laser ID 2, pin: %i", pins.LASER_PIN_2);
	pinMode(pins.LASER_PIN_2, OUTPUT);
	digitalWrite(pins.LASER_PIN_2, LOW);
	ledcSetup(PWM_CHANNEL_LASER_2, pwm_frequency, pwm_resolution);
	ledcAttachPin(pins.LASER_PIN_2, PWM_CHANNEL_LASER_2);
	ledcWrite(PWM_CHANNEL_LASER_2, 10000);
	delay(500);
	ledcWrite(PWM_CHANNEL_LASER_2, 0);

	// if laser pin is not defined try loading it from the pindef.h file
	if (not pins.LASER_PIN_3)
	{
		pins.LASER_PIN_3 = PIN_DEF_LASER_3; // default value
	}
	log_i("Laser ID 3, pin: %i", pins.LASER_PIN_3);
	pinMode(pins.LASER_PIN_3, OUTPUT);
	digitalWrite(pins.LASER_PIN_3, LOW);
	ledcSetup(PWM_CHANNEL_LASER_3, pwm_frequency, pwm_resolution);
	ledcAttachPin(pins.LASER_PIN_3, PWM_CHANNEL_LASER_3);
	ledcWrite(PWM_CHANNEL_LASER_3, 10000);
	delay(500);
	ledcWrite(PWM_CHANNEL_LASER_3, 0);

	// Write out updated settings to preferences permanently
	Config::setLaserPins(pins);
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
