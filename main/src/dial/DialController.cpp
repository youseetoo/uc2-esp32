#include <PinConfig.h>
#include "DialController.h"
#include "../motor/FocusMotor.h"
#include "../home/HomeMotor.h"
#include "../laser/LaserController.h"
#include "Arduino.h"
#include "../../JsonKeys.h"
#include "cJsonTool.h"
#include "../i2c/tca_controller.h"
#include "../i2c/i2c_controller.h"

namespace DialController
{
	// Custom function accessible by the API
	int act(cJSON *jsonDocument)
	{
		int qid = cJsonTool::getJsonInt(jsonDocument, "qid");
		// here you can do something
		log_d("dial_act_fct");
		return qid;
	}

	// Custom function accessible by the API
	cJSON *get(cJSON *jsonDocument)
	{

		// GET SOME PARAMETERS HERE
		cJSON *monitor_json = jsonDocument;

		return monitor_json;
	}

	DialData getDialValues()
	{
		// Convert the positions array to a DialData struct
		DialData mDialData;
		mDialData.pos_a = values[0];
		mDialData.pos_x = values[1];
		mDialData.pos_y = values[2];
		mDialData.pos_z = values[3];
		mDialData.intensity = values[4];
		return mDialData;
	}

	void setDialValues(DialData mDialData)
	{
		// Convert the DialData struct to the positions array
		values[0] = mDialData.pos_a;
		values[1] = mDialData.pos_x;
		values[2] = mDialData.pos_y;
		values[3] = mDialData.pos_z;
		values[4] = mDialData.intensity;
	}

	void writeTextDisplay(String text)
	{
#ifdef I2C_SLAVE
		M5Dial.Display.clear();
		M5Dial.Display.drawString(text, M5Dial.Display.width() / 2, M5Dial.Display.height() / 2);
#endif
	}

	void updateDisplay()
	{
#ifdef I2C_SLAVE
		M5Dial.Display.clear();
		M5Dial.Display.drawString(String(axisNames[currentAxis]) + "=" + String(values[currentAxis]),
								  M5Dial.Display.width() / 2, M5Dial.Display.height() / 2);
		M5Dial.Display.drawString("Step: " + String(stepSize), M5Dial.Display.width() / 2,
								  M5Dial.Display.height() / 2 + 30);
#endif
	}

	void pushMotorPosToDial()
	{
#ifdef I2C_MASTER
		// This is the Master pushing the data to the DIAL I2C slave (i.e. 4 motor positions) to sync the display with the motors
		uint8_t slave_addr = pinConfig.I2C_ADD_M5_DIAL;
		if (!i2c_controller::isAddressInI2CDevices(slave_addr))
		{
			log_e("Error: Dial address not found in i2cAddresses");
			return;
		}
		Wire.beginTransmission(slave_addr);
		mDialData.pos_a = FocusMotor::getData()[0]->currentPosition;
		mDialData.pos_x = FocusMotor::getData()[1]->currentPosition;
		mDialData.pos_y = FocusMotor::getData()[2]->currentPosition;
		mDialData.pos_z = FocusMotor::getData()[3]->currentPosition;
		#ifdef LASER_CONTROLLER 
		mDialData.intensity = LaserController::getLaserVal(1);
		#else
		mDialData.intensity = 0;
		#endif
		log_i("Motor positions sent to dial: %i, %i, %i, %i, %i", mDialData.pos_a, mDialData.pos_x, mDialData.pos_y, mDialData.pos_z, mDialData.intensity);
		Wire.write((uint8_t *)&mDialData, sizeof(DialData));
		Wire.endTransmission();
		positionsPushedToDial = true; // otherwise it will probably always go to 0,0,0,0  on start
#endif
	}

	void pullParamsFromDial()
	{
#ifdef I2C_MASTER
		// This is the MASTER pulling the data from the DIAL I2C slave (i.e. 4 motor positions)
		uint8_t slave_addr = pinConfig.I2C_ADD_M5_DIAL;

		// Request data from the slave but only if inside i2cAddresses
		if (!i2c_controller::isAddressInI2CDevices(slave_addr))
		{
			log_e("Error: Dial address not found in i2cAddresses");
			return;
		}

		// REQUEST DATA FROM DIAL
		DialData mDialData;
		Wire.requestFrom(slave_addr, sizeof(DialData));
		int dataSize = Wire.available();
		if (dataSize == sizeof(DialData))
		{
			Wire.readBytes((char *)&mDialData, sizeof(DialData));
			// check of all elelements are zero, if so we don't want to assign them to the motors as we likly catch the startup of the dial
			if (mDialData.pos_a == 0 and mDialData.pos_x == 0 and mDialData.pos_y == 0 and mDialData.pos_z == 0 and mDialData.intensity == 0)
			{
				return;
			}

			// compare the current position of the motor with the dial state and drive the motor to the dial state
			for (int iMotor = 0; iMotor < 4; iMotor++)
			{
				long position2go = 0;
				if (iMotor == 0)
					position2go = mDialData.pos_a;
				if (iMotor == 1)
					position2go = mDialData.pos_x;
				if (iMotor == 2)
					position2go = mDialData.pos_y;
				if (iMotor == 3)
					position2go = mDialData.pos_z;
				// assign the dial state to the motor
				// if we run in forever mode we don't want to change the position as we likely use the ps4 controller
				// check if homeMotor is null
				bool isMotorHoming = false;
				if (not(HomeMotor::hdata[iMotor] == nullptr))
				{
					isMotorHoming = HomeMotor::hdata[iMotor]->homeIsActive;
				}
				bool isMotorForever = FocusMotor::getData()[iMotor]->isforever;
				if (isMotorForever or isMotorHoming or
					FocusMotor::getData()[iMotor]->currentPosition == position2go)
					continue;
				log_i("Motor %i: Current position: %i, Dial position: %i", iMotor, FocusMotor::getData()[iMotor]->currentPosition, position2go);

				// check if current position and position2go are within a reasonable range 
				// if not we don't want to move the motor
				if (abs(FocusMotor::getData()[iMotor]->currentPosition - position2go) > 10000)
				{
					log_e("Error: Motor %i is too far away from dial position %i", iMotor, position2go);
					continue;
				}
				// Here we drive the motor to the dial state
				Stepper mStepper = static_cast<Stepper>(iMotor);
				FocusMotor::setAutoEnable(false);
				FocusMotor::setEnable(true);
				FocusMotor::getData()[mStepper]->absolutePosition = 1;
				FocusMotor::getData()[mStepper]->targetPosition = position2go;
				FocusMotor::getData()[mStepper]->isforever = 0;
				FocusMotor::getData()[mStepper]->isaccelerated = 1;
				FocusMotor::getData()[mStepper]->acceleration = 10000;
				FocusMotor::getData()[mStepper]->speed = 10000;
				FocusMotor::getData()[mStepper]->isEnable = 1;
				FocusMotor::getData()[mStepper]->qid = 0;
				FocusMotor::getData()[mStepper]->isStop = 0;
				FocusMotor::startStepper(mStepper);
			}
#ifdef LASER_CONTROLLER
			// for intensity only
			int intensity = mDialData.intensity;
			if (lastIntensity != intensity){
				lastIntensity = intensity;
				LaserController::setLaserVal(LaserController::PWM_CHANNEL_LASER_1, intensity);
			}
#endif
		}
		else
		{
			log_e("Error: Incorrect data size received in dial from address %i. Data size is %i", slave_addr, dataSize);
		}

#endif
		return;
	}

	void loop()
	{
#ifdef I2C_MASTER
		if (ticksLastPosPulled >= ticksPosPullInterval and positionsPushedToDial)
		{
			ticksLastPosPulled = 0;
			// Here we want to pull the dial data from the I2C bus and assign it to the motors
			pullParamsFromDial();
		}
		ticksLastPosPulled++;
#endif
#ifdef I2C_SLAVE
		// here we readout the dial values from the M5Stack Dial - so we are the slave
		M5Dial.update();

		if (M5Dial.BtnA.wasPressed())
		{
			M5Dial.Speaker.tone(8000, 20);
			M5Dial.Display.clear();
			M5Dial.Display.drawString("Reboot", M5Dial.Display.width() / 2,
									  M5Dial.Display.height() / 2);
			delay(500);
			ESP.restart();
		}

		long newEncoderPos = M5Dial.Encoder.read();
		if (newEncoderPos != encoderPos)
		{
			values[currentAxis] += (newEncoderPos - encoderPos) * stepSize;
			encoderPos = newEncoderPos;
		}
		updateDisplay();

		auto t = M5Dial.Touch.getDetail();

		// Handle touch begin
		if (t.state == 3)
		{ // TOUCH_BEGIN
			touchStartTime = millis();
		}

		// Handle touch end
		if (t.state == 2 or t.state == 7)
		{ // TOUCH_END
			long touchDuration = millis() - touchStartTime;
			if (touchDuration < LONG_PRESS_DURATION)
			{
				// Short press: switch axis
				currentAxis = (currentAxis + 1) % 5;
				updateDisplay();
			}
			else if (touchDuration >= 100)
			{
				// Long press: change step size
				if (stepSize == 1)
				{
					stepSize = 10;
				}
				else if (stepSize == 10)
				{
					stepSize = 100;
				}
				else if (stepSize == 100)
				{
					stepSize = 1000;
				}
				else
				{
					stepSize = 1;
				}

				// for illumination we don't want to go beyond 100
				if (currentAxis == 4)
				{
					if (stepSize > 100)
					{
						stepSize = 0;
					}
				}
				updateDisplay();
			}
		}

#endif
		// log_i("dial_val_1: %i, dial_val_2: %i, dial_val_3: %i", dial_val_1, dial_val_2, dial_val_3);
	}

	void setup()
	{
		// Here you can setup the dial controller
		// For example you can setup the I2C bus
		// or setup the M5Stack Dial

#ifdef I2C_SLAVE
		mDialData.pos_a = 0;
		mDialData.pos_x = 0;
		mDialData.pos_y = 0;
		mDialData.pos_z = 0;
		auto cfg = M5.config();
		M5Dial.begin(cfg, true, false);
		M5Dial.Display.setTextColor(WHITE);
		M5Dial.Display.setTextDatum(middle_center);
		M5Dial.Display.setTextFont(&fonts::Orbitron_Light_32);
		M5Dial.Display.setTextSize(1);
		M5Dial.Display.drawString("X=" + String(values[currentAxis]), M5Dial.Display.width() / 2,
								  M5Dial.Display.height() / 2);
#endif
	}
} // namespace DialController
