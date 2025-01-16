#include <PinConfig.h>
#include "LaserController.h"
#include "cJsonTool.h"
#include "JsonKeys.h"
#include "../state/State.h"
#ifdef I2C_LASER 
#include "../i2c/i2c_master.h"
#endif
#ifdef CAN_CONTROLLER
#include "../can/can_controller.h"
#endif
namespace LaserController
{

	void LASER_despeckle(int LASERdespeckle, int LASERid, int LASERperiod)
	{
		log_e("LASERdespeckle %i, LASERid %i, LASERperiod %i", LASERdespeckle, LASERid, LASERperiod);
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

		setPWM(LASER_val_wiggle, PWM_CHANNEL_LASER);
		delay(LASERperiod);
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
		// {"task":"/laser_act", "LASERid":1, "LASERval":1000}, "LASERdespeckle":0, "LASERdespecklePeriod":0}
		// {"task":"/laser_act", "LASERid":2, "LASERval":1000}
		// {"task":"/laser_act", "LASERid":2, "LASERval":10000}
		State::setBusy(true);
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


		#ifdef I2C_LASER
		LaserData laserData;
		laserData.LASERid = LASERid;
		laserData.LASERval = LASERval;
		laserData.LASERdespeckle = LASERdespeckle;
		laserData.LASERdespecklePeriod = LASERdespecklePeriod;
		i2c_master::sendLaserDataI2C(laserData, LASERid);
		return qid; 
		#elif defined CAN_CONTROLLER && not defined(CAN_SLAVE_LASER)
		LaserData laserData;
		laserData.LASERid = LASERid;
		laserData.LASERval = LASERval;
		laserData.LASERdespeckle = LASERdespeckle;
		laserData.LASERdespecklePeriod = LASERdespecklePeriod;
		can_controller::sendLaserDataToCANDriver(laserData);
		return qid;
		#else

		// debugging
		log_i("LaserID %i, LaserVal %i, LaserDespeckle %i, LaserDespecklePeriod %i", LASERid, LASERval, LASERdespeckle, LASERdespecklePeriod);

		/*
		Set Laser PWM Frequency
		*/
		if (setPWMFreq != NULL and isServo == false)
		{ // {"task":"/laser_act", "LASERid":1 ,"LASERFreq":50, "LASERval":1000, "qid":1}
			pwm_frequency = setPWMFreq->valueint;
			log_i("Setting PWM frequency to %i", pwm_frequency);
			if (LASERid == 1 && pinConfig.LASER_1 != 0)
			{
				setupLaser(pinConfig.LASER_1, PWM_CHANNEL_LASER_1, pwm_frequency, pwm_resolution);
			}
			if (LASERid == 2 && pinConfig.LASER_2 != 0)
			{
				setupLaser(pinConfig.LASER_2, PWM_CHANNEL_LASER_2, pwm_frequency, pwm_resolution);
			}
			if (LASERid == 3 && pinConfig.LASER_3 != 0)
			{
				setupLaser(pinConfig.LASER_3, PWM_CHANNEL_LASER_3, pwm_frequency, pwm_resolution);
			}
		}

		/*
		Set Laser PWM REsolution
		*/
		if (setPWMRes != NULL and isServo == false)
		{ // {"task":"/laser_act", "LASERid":2 ,"LASERRes":16} // for servo
			log_i("Setting PWM frequency to %i", pwm_frequency);
			if (LASERid == 1 && pinConfig.LASER_1 != 0)
			{
				setupLaser(pinConfig.LASER_1, PWM_CHANNEL_LASER_1, pwm_frequency, pwm_resolution);
			}
			if (LASERid == 2 && pinConfig.LASER_2 != 0)
			{
				setupLaser(pinConfig.LASER_2, PWM_CHANNEL_LASER_2, pwm_frequency, pwm_resolution);
			}
			if (LASERid == 3 && pinConfig.LASER_3 != 0)
			{
				setupLaser(pinConfig.LASER_3, PWM_CHANNEL_LASER_3, pwm_frequency, pwm_resolution);
			}
		}

		// action LASER 1
		if (LASERid == 1 && pinConfig.LASER_1 >= 0 && hasLASERval)
		{
			LASER_val_1 = LASERval;
			LASER_despeckle_1 = LASERdespeckle;
			LASER_despeckle_period_1 = LASERdespecklePeriod;
			if (isServo)
			{
				// for servo
				// {"task":"/laser_act", "LASERid":1 ,"LASERval":99, "servo":1, "qid":1}
				// {"task":"/laser_act", "LASERid":2 ,"LASERval":90, "servo":1, "qid":1}
				pwm_frequency = 50;
				pwm_resolution = 16;

				configurePWM(pinConfig.LASER_1, pwm_resolution, PWM_CHANNEL_LASER_1, pwm_frequency);
				moveServo(PWM_CHANNEL_LASER_1, LASERval, pwm_frequency, pwm_resolution);
			}
			else
			{
				setPWM(LASER_val_1, PWM_CHANNEL_LASER_1);
			}
			log_i("LASERid %i, LASERval %i", LASERid, LASERval);
			State::setBusy(false);
			return qid;
		}
		// action LASER 2
		else if (LASERid == 2 && pinConfig.LASER_2 >= 0 && hasLASERval)
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
				setPWM(LASER_val_2, PWM_CHANNEL_LASER_2);
			}
			log_i("LASERid %i, LASERval %i", LASERid, LASERval);
			State::setBusy(false);
			return qid;
		}
		// action LASER 3
		else if (LASERid == 3 && pinConfig.LASER_3 >= 0 && hasLASERval)
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
				setPWM(LASER_val_3, PWM_CHANNEL_LASER_3);
			}

			log_i("LASERid %i, LASERval %i", LASERid, LASERval);
			State::setBusy(false);
			return qid;
		}
		// action LASER 0
		else if (LASERid == 0 && pinConfig.LASER_0 >= 0 && hasLASERval)
		{
			// action LASER 0
			// {"task":"/laser_act", "LASERid":0 ,"LASERval":50, "qid":1}
			LASER_val_0 = LASERval;
			LASER_despeckle_0 = LASERdespeckle;
			LASER_despeckle_period_0 = LASERdespecklePeriod;
			if (isServo)
			{
				// for servo
				// {"task":"/laser_act", "LASERid":0 ,"LASERval":50, "servo":1, "qid":1}
				pwm_frequency = 50;
				pwm_resolution = 16;

				configurePWM(pinConfig.LASER_0, pwm_frequency, PWM_CHANNEL_LASER_0, pwm_frequency);
				moveServo(PWM_CHANNEL_LASER_0, LASERval, pwm_frequency, pwm_resolution);
			}
			else
			{
				setPWM(LASER_val_0, PWM_CHANNEL_LASER_0);
			}
			log_i("LASERid %i, LASERval %i", LASERid, LASERval);
			State::setBusy(false);
			return qid;
		}
		else
		{
			State::setBusy(false);
			return 0;
		}
		#endif
	}

	bool setLaserVal(int LASERid, int LASERval)
	{
		#ifdef I2C_LASER
		LaserData laserData;
		laserData.LASERid = LASERid;
		laserData.LASERval = LASERval;
		laserData.LASERdespeckle = 0;
		laserData.LASERdespecklePeriod = 0;
		i2c_master::sendLaserDataI2C(laserData, LASERid);
		return true;
		#elif defined CAN_CONTROLLER && not defined(CAN_SLAVE_LASER)
		LaserData laserData;
		laserData.LASERid = LASERid;
		laserData.LASERval = LASERval;
		laserData.LASERdespeckle = 0;
		laserData.LASERdespecklePeriod = 0;
		can_controller::sendLaserDataToCANDriver(laserData);
		return true;
		#else
		if (LASERid == 0 && LASERval >= 0)
		{
			LASER_val_0 = LASERval;
			setPWM(LASER_val_0, PWM_CHANNEL_LASER_0);
			log_i("LASERid %i, LASERval %i", LASERid, LASERval);
			return true;
		}
		else if (LASERid == 1 && pinConfig.LASER_1 != 0)
		{
			LASER_val_1 = LASERval;
			setPWM(LASER_val_1, PWM_CHANNEL_LASER_1);
			log_i("LASERid %i, LASERval %i", LASERid, LASERval);
			return true;
		}
		else if (LASERid == 2 && pinConfig.LASER_2 != 0)
		{
			LASER_val_2 = LASERval;
			setPWM(LASER_val_2, PWM_CHANNEL_LASER_2);
			log_i("LASERid %i, LASERval %i", LASERid, LASERval);
			return true;
		}
		else if (LASERid == 3 && pinConfig.LASER_3 != 0)
		{
			LASER_val_3 = LASERval;
			setPWM(LASER_val_3, PWM_CHANNEL_LASER_3);
			log_i("LASERid %i, LASERval %i", LASERid, LASERval);
			return true;
		}
		else
		{
			return false;
		}
		#endif
	}

	int getLaserVal(int LASERid)
	{
		int laserVal = 0;
		if (LASERid == 1)
		{
			laserVal = LASER_val_1;
		}
		else if (LASERid == 2)
		{
			laserVal = LASER_val_2;
		}
		else if (LASERid == 3)
		{
			laserVal = LASER_val_3;
		}
		else
		{
			laserVal = 0;
		}
		log_i("LASERid %i, LASERval %i", LASERid, laserVal);
		return laserVal;
	}

	void setupLaser(int laser_pin, int pwm_chan, int pwm_freq, int pwm_res)
	{
		// Setting up the differen PWM channels for the laser
		log_i("Setting up Laser with PWM Channel %i, PWM Frequency %i, PWM Resolution %i", pwm_chan, pwm_freq, pwm_res);
		ledcSetup(pwm_chan, pwm_freq, pwm_res);
		ledcAttachPin(laser_pin, pwm_chan);
	}

	LaserData getLaserData(){
		LaserData laserData;
		laserData.LASERid = 1;
		laserData.LASERval = LASER_val_1;
		laserData.LASERdespeckle = LASER_despeckle_1;
		laserData.LASERdespecklePeriod = LASER_despeckle_period_1;
		return laserData;
	}

	bool laser_on = false;
	bool laser2_on = false;
	void dpad_changed_event(Dpad::Direction pressed)
	{
		if (pressed == Dpad::Direction::up && !laser_on) // FIXME: THE LASER TURNS ALWAYS ON CONNECT - WHY?
		{
			// Switch laser 2 on/off on up/down button press
			log_d("Turning on LAser 10000");
			LaserController::setLaserVal(1, 10000);
			laser_on = true;
		}
		if (pressed == Dpad::Direction::down && laser_on)
		{
			log_d("Turning off LAser ");
			LaserController::setLaserVal(1, 0);
			laser_on = false;
		}

		// LASER 2
		// switch laser 2 on/off on triangle/square button press
		if (pressed == Dpad::Direction::right && !laser2_on)
		{
			log_d("Turning on LAser 2 10000");
			LaserController::setLaserVal(2, 10000);
			laser2_on = true;
		}
		if (pressed == Dpad::Direction::left && laser2_on)
		{
			log_d("Turning off LAser ");
			Serial.println("Turning off LAser ");
			LaserController::setLaserVal(2, 0);
			laser2_on = false;
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
		setupLaser(servoPin, ledChannel, frequency, resolution);
	}

	// Custom function accessible by the API
	// returns json {"laser":{..}}  as qid
	cJSON *get(cJSON *ob)
	{
		/*
		 {"task":"/laser_get"}
		 {"task":"/laser_get", "qid":1}
		 returns {"laser": 	{"LASER1pin":1,
							"LASER2pin":2,
							"LASER3pin":3,
							"LASER1val":0,
							"LASER2val":0,
							"LASER3val":0}} */
		cJSON *j = cJSON_CreateObject();
		cJSON *ls = cJSON_CreateObject();
		cJSON_AddItemToObject(j, key_laser, ls);
		cJsonTool::setJsonInt(ls, "LASER1pin", pinConfig.LASER_1);
		cJsonTool::setJsonInt(ls, "LASER2pin", pinConfig.LASER_2);
		cJsonTool::setJsonInt(ls, "LASER3pin", pinConfig.LASER_3);
		cJsonTool::setJsonInt(ls, "LASER1val", getLaserVal(1));
		cJsonTool::setJsonInt(ls, "LASER2val", getLaserVal(2));
		cJsonTool::setJsonInt(ls, "LASER3val", getLaserVal(3));

		return j;
	}

	void setup()
	{
		log_i("Setting Up LASERs");

		// Setting up the differen PWM channels for the laser
		log_i("Laser ID 1, pin: %i", pinConfig.LASER_1);
		pinMode(pinConfig.LASER_1, OUTPUT);
		digitalWrite(pinConfig.LASER_1, LOW);
		setupLaser(pinConfig.LASER_1, PWM_CHANNEL_LASER_1, pwm_frequency, pwm_resolution);
		setLaserVal(1, 10000);
		delay(10);
		setLaserVal(1, 0);

		log_i("Laser ID 2, pin: %i", pinConfig.LASER_2);
		pinMode(pinConfig.LASER_2, OUTPUT);
		digitalWrite(pinConfig.LASER_2, LOW);
		setupLaser(pinConfig.LASER_2, PWM_CHANNEL_LASER_2, pwm_frequency, pwm_resolution);
		setLaserVal(2, 10000);
		delay(10);
		setLaserVal(2, 0);

		log_i("Laser ID 3, pin: %i", pinConfig.LASER_3);
		pinMode(pinConfig.LASER_3, OUTPUT);
		digitalWrite(pinConfig.LASER_3, LOW);
		setupLaser(pinConfig.LASER_3, PWM_CHANNEL_LASER_3, pwm_frequency, pwm_resolution);
		setLaserVal(3, 10000);
		delay(10);
		setLaserVal(3, 0);

#ifdef HEAT_CONTROLLER
		// Setting up the differen PWM channels for the heating unit
		if (pinConfig.LASER_0 > 0)
		{
			log_i("Heating Unit, pin: %i", pinConfig.LASER_0);
			pinMode(pinConfig.LASER_0, OUTPUT);
			digitalWrite(pinConfig.LASER_0, LOW);
			setupLaser(pinConfig.LASER_0, PWM_CHANNEL_LASER_0, pwm_frequency, pwm_resolution);
			setLaserVal(0, 10000);
			delay(10);
			setLaserVal(0, 0);
			
		}
#endif
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
