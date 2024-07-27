#include <PinConfig.h>
#include "LaserController.h"
#include "cJsonTool.h"
#include "JsonKeys.h"

namespace LaserController
{

	void LASER_despeckle(int LASERdespeckle, int LASERid, int LASERperiod)
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

			log_d("%i", LASERid);
			log_d("%i", LASER_val_wiggle);

			ledcWrite(PWM_CHANNEL_LASER, LASER_val_wiggle);

			delay(LASERperiod);
		}
	}

	void setPWM(int pwmValue, int pwmChannel)
	{
		// sets the PWM value for the given channel
		ledcWrite(pwmChannel, pwmValue);
	}

	// Custom function accessible by the API
	int act(cJSON *ob)
	{
		// JSON String
		// {"task":"/laser_act", "LASERid":1, "LASERval":100, "LASERdespeckle":10, "LASERdespecklePeriod":20}
		int qid = cJsonTool::getJsonInt(ob, "qid");
		cJSON *setPWMFreq = cJSON_GetObjectItemCaseSensitive(ob, "LASERFreq");
		cJSON *setPWMRes = cJSON_GetObjectItemCaseSensitive(ob, "LASERRes");
		bool hasLASERval = cJSON_HasObjectItem(ob, "LASERval"); // check if ob contains the key "LASERval"
		bool isServo = cJSON_HasObjectItem(ob, "servo");		// check if ob contains the key "LASERRes"

		// assign values
		int LASERid = 0;
		int LASERval = 0;
		int LASERdespeckle = 0;
		int LASERdespecklePeriod = 0;
		// default values overridden
		LASERid = cJsonTool::getJsonInt(ob, "LASERid");
		LASERval = cJsonTool::getJsonInt(ob, "LASERval");
		LASERdespeckle = cJsonTool::getJsonInt(ob, "LASERdespeckle");
		LASERdespecklePeriod = cJsonTool::getJsonInt(ob, "LASERdespecklePeriod");
		// debugging
		log_i("LaserID %i, LaserVal %i, LaserDespeckle %i, LaserDespecklePeriod %i", LASERid, LASERval, LASERdespeckle, LASERdespecklePeriod);

		/*
		Set Laser PWM Frequency
		*/
		if (setPWMFreq != NULL and isServo == false)
		{ // {"task":"/laser_act", "LASERid":1 ,"LASERFreq":50, "LASERval":1000, "qid":1}
			pwm_frequency = setPWMFreq->valueint;
			log_i("Setting PWM frequency to %i", pwmFreq);
			if (LASERid == 1 && pinConfig.LASER_1 != 0)
			{
				ledcSetup(PWM_CHANNEL_LASER_1, pwm_frequency, pwm_resolution);
			}
			if (LASERid == 2 && pinConfig.LASER_2 != 0)
			{
				ledcSetup(PWM_CHANNEL_LASER_2, pwm_frequency, pwm_resolution);
			}
			if (LASERid == 3 && pinConfig.LASER_3 != 0)
			{
				ledcSetup(PWM_CHANNEL_LASER_3, pwm_frequency, pwm_resolution);
			}
		}

		/*
		Set Laser PWM REsolution
		*/
		if (setPWMRes != NULL and isServo == false)
		{ // {"task":"/laser_act", "LASERid":1 ,"LASERRes":16} // for servo
			log_i("Setting PWM frequency to %i", pwmFreq);
			if (LASERid == 1 && pinConfig.LASER_1 != 0)
			{
				ledcSetup(PWM_CHANNEL_LASER_1, pwm_frequency, pwm_resolution);
			}
			if (LASERid == 2 && pinConfig.LASER_2 != 0)
			{
				ledcSetup(PWM_CHANNEL_LASER_2, pwm_frequency, pwm_resolution);
			}
			if (LASERid == 3 && pinConfig.LASER_3 != 0)
			{
				ledcSetup(PWM_CHANNEL_LASER_3, pwm_frequency, pwm_resolution);
			}
		}

		// action LASER 1
		if (LASERid == 1 && pinConfig.LASER_1 != 0 && hasLASERval)
		{
			LASER_val_1 = LASERval;
			LASER_despeckle_1 = LASERdespeckle;
			LASER_despeckle_period_1 = LASERdespecklePeriod;
			if (isServo)
			{
				// for servo
				// {"task":"/laser_act", "LASERid":1 ,"LASERval":100, "servo":1, "qid":1}
				pwm_frequency = 50;
				pwm_resolution = 16;

				configurePWM(pinConfig.LASER_1, pwm_resolution, PWM_CHANNEL_LASER_1, pwm_frequency);
				moveServo(PWM_CHANNEL_LASER_1, LASERval, pwm_frequency, pwm_resolution);
			}
			else 
			{
				ledcWrite(PWM_CHANNEL_LASER_1, LASERval);
			}
			log_i("LASERid %i, LASERval %i", LASERid, LASERval);
			return qid;
		}
		// action LASER 2
		else if (LASERid == 2 && pinConfig.LASER_2 != 0 && hasLASERval)
		{
			LASER_val_2 = LASERval;
			LASER_despeckle_2 = LASERdespeckle;
			LASER_despeckle_period_2 = LASERdespecklePeriod;
			if (isServo)
			{
				// for servo
				// {"task":"/laser_act", "LASERid":2 ,"LASERval":50, "servo":1, "qid":1}
				pwm_frequency = 50;
				pwm_resolution = 16;

				configurePWM(pinConfig.LASER_2, pwm_frequency, PWM_CHANNEL_LASER_2, pwm_frequency);
				moveServo(PWM_CHANNEL_LASER_2, LASERval, pwm_frequency, pwm_resolution);
			}
			else
			{
				ledcWrite(PWM_CHANNEL_LASER_2, LASERval);
			}
			log_i("LASERid %i, LASERval %i", LASERid, LASERval);
			return qid;
		}
		// action LASER 3
		else if (LASERid == 3 && pinConfig.LASER_3 != 0 && hasLASERval)
		{
			LASER_val_3 = LASERval;
			LASER_despeckle_3 = LASERdespeckle;
			LASER_despeckle_period_3 = LASERdespecklePeriod;
			if (isServo)
			{
				// for servo
				// {"task":"/laser_act", "LASERid":3 ,"LASERval":50, "servo":1, "qid":1}
				pwm_frequency = 50;
				pwm_resolution = 16;

				configurePWM(pinConfig.LASER_3, pwm_frequency, PWM_CHANNEL_LASER_3, pwm_frequency);
				moveServo(PWM_CHANNEL_LASER_3, LASERval, pwm_frequency, pwm_resolution);
			}
			else
			{
				ledcWrite(PWM_CHANNEL_LASER_3, LASERval);
			}

			log_i("LASERid %i, LASERval %i", LASERid, LASERval);
			return qid;
		}
		else
		{
			return 0;
		}
	}

	void moveServo(int ledChannel, int angle, int frequency, int resolution)
	{
		// Map the angle to the corresponding pulse width
		int pulseWidth = map(angle, 0, 180, minPulseWidth, maxPulseWidth);
		// Convert the pulse width to a duty cycle value
		int dutyCycle = map(pulseWidth, 0, 1000000 / frequency, 0, (1 << resolution) - 1);
		// Write the duty cycle to the LEDC channel
		ledcWrite(ledChannel, dutyCycle);
	}

	void configurePWM(int servoPin, int resolution, int ledChannel, int frequency)
	{
		// Detach the pin
		ledcDetachPin(servoPin);
		// Configure the LEDC PWM channel with the new frequency
		ledcSetup(ledChannel, frequency, resolution);
		// Attach the LEDC channel to the specified GPIO pin
		ledcAttachPin(servoPin, ledChannel);


		
	}

	// Custom function accessible by the API
	// returns json {"laser":{..}}  as qid
	cJSON *get(cJSON *ob)
	{
		cJSON *j = cJSON_CreateObject();
		cJSON *ls = cJSON_CreateObject();
		cJSON_AddItemToObject(j, key_laser, ls);
		cJsonTool::setJsonInt(ls, "LASER1pin", pinConfig.LASER_1);
		cJsonTool::setJsonInt(ls, "LASER2pin", pinConfig.LASER_2);
		cJsonTool::setJsonInt(ls, "LASER3pin", pinConfig.LASER_3);
		return j;
	}

	void setup()
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

	void loop()
	{
		// attempting to despeckle by wiggeling the temperature-dependent modes of the laser?
		if (LASER_despeckle_1 > 0 && LASER_val_1 > 0 && pinConfig.LASER_1 != 0)
			LASER_despeckle(LASER_despeckle_1, 1, LASER_despeckle_period_1);
		if (LASER_despeckle_2 > 0 && LASER_val_2 > 0 && pinConfig.LASER_2 != 0)
			LASER_despeckle(LASER_despeckle_2, 2, LASER_despeckle_period_2);
		if (LASER_despeckle_3 > 0 && LASER_val_3 > 0 && pinConfig.LASER_3 != 0)
			LASER_despeckle(LASER_despeckle_3, 3, LASER_despeckle_period_3);
	}
}
