#include "LaserController.h"

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
	// JSON String
	// {"task":"/laser_act", "LASERid":1, "LASERval":100, "LASERdespeckle":10, "LASERdespecklePeriod":20}

	// assign values
	int LASERid = 0;
	int LASERval = 0;
	int LASERdespeckle = 0;
	int LASERdespecklePeriod = 0;
	// default values overridden
	if(ob.containsKey("LASERid")) LASERid = (ob)["LASERid"];
	if(ob.containsKey("LASERval")) LASERval = (ob)["LASERval"];
	if(ob.containsKey("LASERdespeckle")) LASERdespeckle = (ob)["LASERdespeckle"];
	if(ob.containsKey("LASERdespecklePeriod")) LASERdespecklePeriod = (ob)["LASERdespecklePeriod"];
	ob.clear();

	// debugging
	log_i("LaserID %i, LaserVal %i, LaserDespeckle %i, LaserDespecklePeriod %i", LASERid, LASERval, LASERdespeckle, LASERdespecklePeriod);

	// action LASER 1
	if (LASERid == 1 && pinConfig.LASER_1 != 0)
	{
		LASER_val_1 = LASERval;
		LASER_despeckle_1 = LASERdespeckle;
		LASER_despeckle_period_1 = LASERdespecklePeriod;
		ledcWrite(PWM_CHANNEL_LASER_1, LASERval);
		log_i("LASERid %i, LASERval %i", LASERid, LASERval);
		ob[key_return] = 1;
	}
	// action LASER 2
	else if (LASERid == 2  && pinConfig.LASER_2 != 0)
	{
		LASER_val_2 = LASERval;
		LASER_despeckle_2 = LASERdespeckle;
		LASER_despeckle_period_2 = LASERdespecklePeriod;
		ledcWrite(PWM_CHANNEL_LASER_2, LASERval);
		log_i("LASERid %i, LASERval %i", LASERid, LASERval);
		ob[key_return] = 1;
	}
	// action LASER 3
	else if (LASERid == 3  && pinConfig.LASER_3 != 0)
	{
		LASER_val_3 = LASERval;
		LASER_despeckle_3 = LASERdespeckle;
		LASER_despeckle_period_3 = LASERdespecklePeriod;
		ledcWrite(PWM_CHANNEL_LASER_3, LASERval);
		log_i("LASERid %i, LASERval %i", LASERid, LASERval);
		ob[key_return] = 1;
	}
	else{
		ob[key_return] = 0;
	}

	return ob[key_return];
}

// Custom function accessible by the API
DynamicJsonDocument LaserController::get(DynamicJsonDocument  ob)
{
	ob.clear();
	ob["LASER1pin"] = pinConfig.LASER_1;
	ob["LASER2pin"] = pinConfig.LASER_2;
	ob["LASER3pin"] = pinConfig.LASER_3;
	return ob;
}

void LaserController::setup()
{
	log_i("Setting Up LASERs");


	// Setting up the differen PWM channels for the laser
	log_i("Laser ID 1, pin: %i", pinConfig.LASER_1);
	pinMode(pinConfig.LASER_1, OUTPUT);
	digitalWrite(pinConfig.LASER_1, LOW);
	ledcSetup(PWM_CHANNEL_LASER_1, pwm_frequency, pwm_resolution);
	ledcAttachPin(pinConfig.LASER_1, PWM_CHANNEL_LASER_1);
	ledcWrite(PWM_CHANNEL_LASER_1, 10000);
	delay(10);
	ledcWrite(PWM_CHANNEL_LASER_1, 0);

	log_i("Laser ID 2, pin: %i", pinConfig.LASER_2);
	pinMode(pinConfig.LASER_2, OUTPUT);
	digitalWrite(pinConfig.LASER_2, LOW);
	ledcSetup(PWM_CHANNEL_LASER_2, pwm_frequency, pwm_resolution);
	ledcAttachPin(pinConfig.LASER_2, PWM_CHANNEL_LASER_2);
	ledcWrite(PWM_CHANNEL_LASER_2, 10000);
	delay(10);
	ledcWrite(PWM_CHANNEL_LASER_2, 0);

	log_i("Laser ID 3, pin: %i", pinConfig.LASER_3);
	pinMode(pinConfig.LASER_3, OUTPUT);
	digitalWrite(pinConfig.LASER_3, LOW);
	ledcSetup(PWM_CHANNEL_LASER_3, pwm_frequency, pwm_resolution);
	ledcAttachPin(pinConfig.LASER_3, PWM_CHANNEL_LASER_3);
	ledcWrite(PWM_CHANNEL_LASER_3, 10000);
	delay(10);
	ledcWrite(PWM_CHANNEL_LASER_3, 0);
}

void LaserController::loop()
{
	// attempting to despeckle by wiggeling the temperature-dependent modes of the laser?
	if (LASER_despeckle_1 > 0 && LASER_val_1 > 0 && pinConfig.LASER_1 != 0)
		LASER_despeckle(LASER_despeckle_1, 1, LASER_despeckle_period_1);
	if (LASER_despeckle_2 > 0 && LASER_val_2 > 0 && pinConfig.LASER_2 != 0)
		LASER_despeckle(LASER_despeckle_2, 2, LASER_despeckle_period_2);
	if (LASER_despeckle_3 > 0 && LASER_val_3 > 0 && pinConfig.LASER_3 != 0)
		LASER_despeckle(LASER_despeckle_3, 3, LASER_despeckle_period_3);
}
