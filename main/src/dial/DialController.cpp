#include <PinConfig.h>
#include "DialController.h"
#include "../motor/FocusMotor.h"
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

	DialData getPositionValues()
	{
		// Convert the positions array to a DialData struct
		DialData mDialData;
		mDialData.pos_a = positions[0];
		mDialData.pos_x = positions[1];
		mDialData.pos_y = positions[2];
		mDialData.pos_z = positions[3];
		return mDialData;
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
		M5Dial.Display.drawString(String(axisNames[currentAxis]) + "=" + String(positions[currentAxis]),
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
		log_i("Pushing motor positions to dial");
		Wire.beginTransmission(slave_addr);
		mPosData.pos_a = FocusMotor::getData()[0]->currentPosition;
		mPosData.pos_x = FocusMotor::getData()[1]->currentPosition;
		mPosData.pos_y = FocusMotor::getData()[2]->currentPosition;
		mPosData.pos_z = FocusMotor::getData()[3]->currentPosition;
		Wire.write((uint8_t *)&mPosData, sizeof(DialData));
		Wire.endTransmission();
		#endif
	}

	void pullMotorPosFromDial()
	{
		// This is the MASTER pulling the data from the DIAL I2C slave (i.e. 4 motor positions)
		uint8_t slave_addr = pinConfig.I2C_ADD_M5_DIAL;

		// Request data from the slave but only if inside i2cAddresses
		if (!i2c_controller::isAddressInI2CDevices(slave_addr))
		{
			log_e("Error: Dial address not found in i2cAddresses");
			return;
		}
		DialData mDialData;
		Wire.requestFrom(slave_addr, sizeof(DialData));
		int dataSize = Wire.available();
		if (dataSize == sizeof(DialData))
		{
			Wire.readBytes((char *)&mDialData, sizeof(DialData));

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
				if (FocusMotor::getData()[iMotor]->currentPosition == position2go)
					continue;
				//log_i("Motor %i: Current position: %i, Dial position: %i", iMotor, FocusMotor::getData()[iMotor]->currentPosition, position2go);

				// Here we drive the motor to the dial state
				Stepper mStepper = static_cast<Stepper>(iMotor);
				FocusMotor::setAutoEnable(true);
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
		}
		else
		{
			log_e("Error: Incorrect data size received in dial from address %i. Data size is %i", slave_addr, dataSize);
		}

		return;
	}

	void loop()
	{
#ifdef I2C_MASTER
		if (ticksLastPosPulled >= ticksPosPullInterval)
		{
			ticksLastPosPulled = 0;
			// Here we want to pull the dial data from the I2C bus and assign it to the motors
			pullMotorPosFromDial();
		}
		ticksLastPosPulled++;
#endif
#ifdef I2C_SLAVE
		// here we readout the dial values from the M5Stack Dial - so we are the slave
		M5Dial.update();

		long newEncoderPos = M5Dial.Encoder.read();
		if (newEncoderPos != encoderPos)
		{
			positions[currentAxis] += (newEncoderPos - encoderPos) * stepSize;
			encoderPos = newEncoderPos;
			updateDisplay();
		}

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
				currentAxis = (currentAxis + 1) % 4;
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
				updateDisplay();
			}
		}

#endif
		// log_i("dial_val_1: %i, dial_val_2: %i, dial_val_3: %i", dial_val_1, dial_val_2, dial_val_3);
	}

	void sendMotorPosI2C()
	{
		return;
		// This is the Master pushing the data to the DIAL I2C slave (i.e. 4 motor positions) to sync the display with the motors
		uint8_t slave_addr = pinConfig.I2C_ADD_M5_DIAL;
		if (!i2c_controller::isAddressInI2CDevices(slave_addr))
		{
			log_e("Error: Dial address not found in i2cAddresses");
			return;
		}
		Wire.beginTransmission(slave_addr);
		mPosData.pos_a = FocusMotor::getData()[0]->currentPosition;
		mPosData.pos_x = FocusMotor::getData()[1]->currentPosition;
		mPosData.pos_y = FocusMotor::getData()[2]->currentPosition;
		mPosData.pos_z = FocusMotor::getData()[3]->currentPosition;
		Wire.write((uint8_t *)&mPosData, sizeof(DialData));
		Wire.endTransmission();
	}

	void setup()
	{
		// Here you can setup the dial controller
		// For example you can setup the I2C bus
		// or setup the M5Stack Dial

#ifdef I2C_SLAVE
		mPosData.pos_a = 0;
		mPosData.pos_x = 0;
		mPosData.pos_y = 0;
		mPosData.pos_z = 0;
		auto cfg = M5.config();
		M5Dial.begin(cfg, true, false);
		M5Dial.Display.setTextColor(WHITE);
		M5Dial.Display.setTextDatum(middle_center);
		M5Dial.Display.setTextFont(&fonts::Orbitron_Light_32);
		M5Dial.Display.setTextSize(1);
		M5Dial.Display.drawString("X=" + String(positions[currentAxis]), M5Dial.Display.width() / 2,
								  M5Dial.Display.height() / 2);
#endif
	}
} // namespace DialController
