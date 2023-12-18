#include <PinConfig.h>
#ifdef ENCODER_CONTROLLER
#include "EncoderController.h"
#include "FastAccelStepper.h"
#include "../digitalin/DigitalInController.h"
#include "../config/ConfigController.h"
#include "HardwareSerial.h"

#include "../../JsonKeys.h"

#ifdef FOCUS_MOTOR
#include "../motor/FocusMotor.h"
#endif

namespace EncoderController
{

	void processEncoderLoop(void *p)
	{
		
	}

	/*
	Handle REST calls to the EncoderController module
	*/
	int act(cJSON *j)
	{
		// set position
		if (DEBUG)
			log_i("encoder_act_fct");

		// set position
		cJSON *calibrate = cJSON_GetObjectItem(j, key_encoder_calibrate);

		if (calibrate != NULL)
		{
			// {"task": "/encoder_act", "calpos": {"steppers": [ { "stepperid": 1, "calibsteps": -32000 , "speed": 10000} ]}}
			// depending on the axis, move for 1000 steps forward and measure the distance using the encoder in mm
			// do the same for the way back and calculate the steps per mm
			// print calibrate cjson
			cJSON *stprs = cJSON_GetObjectItem(calibrate, key_steppers);
			if (stprs != NULL)
			{
				// print stprs
				cJSON *stp = NULL;
				cJSON_ArrayForEach(stp, stprs)
				{
					Stepper s = static_cast<Stepper>(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint);
					// measure current value
					for (int t = 0; t < 2; t++)
					{ // read twice?
						edata[s]->valuePreCalib = readValue(edata[s]->clkPin, edata[s]->dataPin);
						delay(40);
					}
					delay(100); // wait until slide settles
					int calibsteps = cJSON_GetObjectItemCaseSensitive(stp, key_encoder_calibpos)->valueint;
					int speed = cJSON_GetObjectItemCaseSensitive(stp, key_speed)->valueint;
					// get the motor object and chang the values so that it will move 1000 steps forward
					edata[s]->calibsteps = calibsteps;
#ifdef FOCUS_MOTOR
					FocusMotor::data[s]->targetPosition = calibsteps;
					FocusMotor::data[s]->absolutePosition = false;
					FocusMotor::data[s]->speed = speed;
					FocusMotor::startStepper(s);
#endif
					edata[s]->requestCalibration = true;
					log_d("pre calib %f", edata[s]->valuePreCalib);
					/*
						// calibrate for 1000 steps in X
						float steps_to_go = 1000;

						// read encoder value for X and store value
						float posval_init = readValue(edata[s]->clkPin, edata[s]->dataPin);

						// move 1000 steps forward in background
						FocusMotor::move(s, steps_to_go, true);

						// read encoder value for X and store value
						float posval_final = readValue(edata[s]->clkPin, edata[s]->dataPin);

						// calculate steps per mm
						float steps_per_mm = steps_to_go / (posval_final - posval_init);

						// store steps per mm in config
						//ConfigController *config = (ConfigController *)moduleController.get(AvailableModules::config);
						//config->setEncoderStepsPerMM(s, steps_per_mm);
						log_d("calibrated encoder %i with %f steps per mm", s, steps_per_mm);
						//FocusMotor::setPosition(s, cJSON_GetObjectItemCaseSensitive(stp,key_currentpos)->valueint);
						*/
				}
			}
		}
		return 1;
	}

	// returns json {"encoder":{..}}
	cJSON *get(cJSON *docin)
	{
		// {"task":"/encoder_get", "encoder": { "posval": 1,    "id": 1  }}

		log_i("encoder_get_fct");
		int isPos = -1;
		int encoderID = -1;
		cJSON *doc = cJSON_CreateObject();
		cJSON *encoder = cJSON_GetObjectItemCaseSensitive(docin, key_encoder);
		if (cJSON_IsObject(encoder))
		{
			cJSON *posval = cJSON_GetObjectItemCaseSensitive(encoder, key_encoder_getpos);
			if (cJSON_IsNumber(posval))
			{
				isPos = posval->valueint;
			}

			cJSON *id = cJSON_GetObjectItemCaseSensitive(encoder, key_encoder_encoderid);
			if (cJSON_IsNumber(id))
			{
				encoderID = id->valueint;
			}
		}

		encoder = cJSON_CreateObject();
		cJSON_AddItemToObject(doc, key_encoder, encoder);
		float posval = 0;
		if (isPos > 0 and encoderID >= 0)
		{
			cJSON *aritem = cJSON_CreateObject();
			edata[encoderID]->requestPosition = true;
			posval = readValue(edata[encoderID]->clkPin, edata[encoderID]->dataPin);
			edata[encoderID]->posval = posval;

			log_d("read encoder %i get position %f", encoderID, edata[encoderID]->posval);
			cJSON_AddNumberToObject(aritem, "posval", posval);
			cJSON_AddNumberToObject(aritem, "encoderID", encoderID);
			cJSON_AddItemToObject(encoder, "encoder_data", aritem);
		}
		return doc;
	}

	// returns json {"encoder":{..}}
	void sendPosition(int axis)
	{
		// send home done to client
		cJSON *json = cJSON_CreateObject();
		cJSON *encoder = cJSON_CreateObject();
		cJSON_AddItemToObject(json, key_encoder, encoder);
		cJSON *steppers = cJSON_CreateObject();
		cJSON_AddItemToObject(encoder, key_steppers, steppers);
		cJSON *axs = cJSON_CreateNumber(axis);
		cJSON *done = cJSON_CreateNumber(true);
		cJSON_AddItemToObject(steppers, "axis", axs);
		cJSON_AddItemToObject(steppers, "isDone", done);
		Serial.println("++");
		char *ret = cJSON_Print(json);
		cJSON_Delete(json);
		Serial.println(ret);
		free(ret);
		Serial.println("--");
	}

	/*
		get called repeatedly, dont block this
	*/
	void loop()
	{
		#ifdef FOCUS_MOTOR
			// check if we need to read the encoder for all motors
			for (int i = 0; i < FocusMotor::data.size(); i++)
			{
				if (edata[i]->requestCalibration and FocusMotor::data[i]->stopped)
				{
					edata[i]->requestCalibration = false;
					delay(1000); // wait until slide settles
					for (int t = 0; t < 2; t++)
					{ // read twice?
						edata[i]->valuePostCalib = readValue(edata[i]->clkPin, edata[i]->dataPin);
						delay(40);
					}
					edata[i]->stepsPerMM = edata[i]->calibsteps / (edata[i]->valuePostCalib - edata[i]->valuePreCalib);
					log_d("post calib %f", edata[i]->valuePostCalib);
					log_d("calibrated encoder %i with %f steps per mm", i, edata[i]->stepsPerMM);
				}
			}
		
		#endif
	}

	/*
	not needed all stuff get setup inside motor and digitalin, but must get implemented
	*/
	void setup()
	{
		log_i("Encoder setup");

		// initialize the encoder pins as inputs
		pinMode(pinConfig.X_CAL_CLK, INPUT);
		pinMode(pinConfig.X_CAL_DATA, INPUT);
		pinMode(pinConfig.Y_CAL_CLK, INPUT);
		pinMode(pinConfig.Y_CAL_DATA, INPUT);
		pinMode(pinConfig.Z_CAL_CLK, INPUT);
		pinMode(pinConfig.Z_CAL_DATA, INPUT);

		for (int i = 0; i < 4; i++)
		{
			// A,X,y,z // the zeroth is unused but we need to keep the loops happy
			edata[i] = new EncoderData();
			edata[i]->encoderID = i;
		}
		edata[0]->clkPin = -1;
		edata[0]->dataPin = -1;
		edata[1]->clkPin = pinConfig.X_CAL_CLK;
		edata[1]->dataPin = pinConfig.X_CAL_DATA;
		edata[2]->clkPin = pinConfig.Y_CAL_CLK;
		edata[2]->dataPin = pinConfig.Y_CAL_DATA;
		edata[3]->clkPin = pinConfig.Z_CAL_CLK;
		edata[3]->dataPin = pinConfig.Z_CAL_DATA;
	}

	float readValue(int clockpin, int datapin)
	{
		float returnValue = -99999.;
		unsigned long tMicros;
		unsigned long tStart = micros();
		// log_d("Reading value with pins %i %i", clockpin, datapin);
		while (digitalRead(clockpin) == HIGH)
		{
			// timeout
			if (micros() - tStart > 10000)
			{
				log_e("timeout1");
				return -99999.;
			}
		}					// If clock is HIGH, wait until it turns to LOW
		tMicros = micros(); // Record the current micros() value

		while (digitalRead(clockpin) == LOW)
		{
			// timeout
			if (micros() - tStart > 200000)
			{
				log_e("timeout2");
				return -99999.;
			}
		} // Wait for the end of the HIGH pulse

		if ((micros() - tMicros) > 500)
		{ // If the HIGH pulse was longer than 500 microseconds, we are at the start of a new bit sequence
			// decode value
			returnValue = decode(clockpin, datapin);
		}
		return returnValue;
	}

	// Decode Function - Decodes the received IR signal
	float decode(int clockpin, int datapin)
	{
		float result;
		int sign = 1;	// Initialize sign to positive
		long value = 0; // Initialize value to zero

		for (int i = 0; i < 23; i++)
		{
			while (digitalRead(clockpin) == HIGH)
			{
			} // Wait until clock returns to HIGH (the first bit is not needed)
			while (digitalRead(clockpin) == LOW)
			{
			} // Wait until clock returns to LOW

			if (digitalRead(datapin) == LOW)
			{
				if (i < 20)
				{
					value |= 1 << i; // Set the bit in 'value' at position 'i' to 1
				}
				if (i == 20)
				{
					sign = -1; // Set sign to negative
				}
			}
		}

		result = (value * sign) / 100.00; // Calculate the result (value with sign and two decimal places)
		// delay(50); // Delay for a short time to avoid continuous decoding
		return result;
	}
}
#endif