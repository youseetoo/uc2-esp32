#include "LaserController.h"


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

		setPWM(LASER_val_wiggle, PWM_CHANNEL_LASER);

		delay(LASERperiod);
	}
}

void LaserController::setPWM(int pwmValue, int pwmChannel){
	// sets the PWM value for the given channel
	ledcWrite(pwmChannel, pwmValue);
}

// Custom function accessible by the API
int LaserController::act(cJSON * ob)
{
	// JSON String
	// {"task":"/laser_act", "LASERid":1, "LASERval":100, "LASERdespeckle":10, "LASERdespecklePeriod":20}

	int qid = getJsonInt(ob, keyQueueID);

	// assign values
	int LASERid = 0;
	int LASERval = 0;
	int LASERdespeckle = 0;
	int LASERdespecklePeriod = 0;
	// default values overridden
	LASERid=getJsonInt(ob,"LASERid");
	LASERval=getJsonInt(ob,"LASERval");
	LASERdespeckle=getJsonInt(ob,"LASERdespeckle");
	LASERdespecklePeriod=getJsonInt(ob,"LASERdespecklePeriod");
	// debugging
	log_i("LaserID %i, LaserVal %i, LaserDespeckle %i, LaserDespecklePeriod %i", LASERid, LASERval, LASERdespeckle, LASERdespecklePeriod);

	// action LASER 1
	if (LASERid == 1 && pinConfig.LASER_1 != 0)
	{
		LASER_val_1 = LASERval;
		LASER_despeckle_1 = LASERdespeckle;
		LASER_despeckle_period_1 = LASERdespecklePeriod;
		setPWM(LASERval, PWM_CHANNEL_LASER_1);
		log_i("LASERid %i, LASERval %i", LASERid, LASERval);
		return qid;
	}
	// action LASER 2
	else if (LASERid == 2 && pinConfig.LASER_2 != 0)
	{
		LASER_val_2 = LASERval;
		LASER_despeckle_2 = LASERdespeckle;
		LASER_despeckle_period_2 = LASERdespecklePeriod;
		setPWM(LASERval, PWM_CHANNEL_LASER_2);
		log_i("LASERid %i, LASERval %i", LASERid, LASERval);
		return qid;
	}
	// action LASER 3
	else if (LASERid == 3 && pinConfig.LASER_3 != 0)
	{
		LASER_val_3 = LASERval;
		LASER_despeckle_3 = LASERdespeckle;
		LASER_despeckle_period_3 = LASERdespecklePeriod;
		setPWM(LASERval, PWM_CHANNEL_LASER_3);
		log_i("LASERid %i, LASERval %i", LASERid, LASERval);
		return qid;
	}
	else
	{
		return -qid;
	}
}

// Custom function accessible by the API
cJSON * LaserController::get(cJSON * ob)
{
	int qid = getJsonInt(ob, keyQueueID);
	cJSON * j = cJSON_CreateObject();
	setJsonInt(j,"LASER1pin", pinConfig.LASER_1);
	setJsonInt(j,"LASER2pin", pinConfig.LASER_2);
	setJsonInt(j,"LASER3pin", pinConfig.LASER_3);
	setJsonInt(j,keyQueueID, qid);
	return j;
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
	setPWM(10000, PWM_CHANNEL_LASER_1);
	delay(5);
	setPWM(0, PWM_CHANNEL_LASER_1);

	log_i("Laser ID 2, pin: %i", pinConfig.LASER_2);
	pinMode(pinConfig.LASER_2, OUTPUT);
	digitalWrite(pinConfig.LASER_2, LOW);
	ledcSetup(PWM_CHANNEL_LASER_2, pwm_frequency, pwm_resolution);
	ledcAttachPin(pinConfig.LASER_2, PWM_CHANNEL_LASER_2);
	setPWM(10000, PWM_CHANNEL_LASER_2);
	delay(5);
	setPWM(0, PWM_CHANNEL_LASER_2);

	log_i("Laser ID 3, pin: %i", pinConfig.LASER_3);
	pinMode(pinConfig.LASER_3, OUTPUT);
	digitalWrite(pinConfig.LASER_3, LOW);
	ledcSetup(PWM_CHANNEL_LASER_3, pwm_frequency, pwm_resolution);
	ledcAttachPin(pinConfig.LASER_3, PWM_CHANNEL_LASER_3);
	setPWM(10000, PWM_CHANNEL_LASER_3);
	delay(5);
	setPWM(0, PWM_CHANNEL_LASER_3);
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
