#include <PinConfig.h>
#include "DialController.h"
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
		if (Wire.available() == sizeof(DialData))
		{
			Wire.readBytes((char *)&mDialData, sizeof(DialData));
			Serial.print("Dial data received: X=");
			Serial.print(mDialData.pos_x);
			Serial.print(", Y=");
			Serial.print(mDialData.pos_y);
			Serial.print(", Z=");
			Serial.print(mDialData.pos_z);
			Serial.print(", A=");
			Serial.println(mDialData.pos_a);
			}
		else
		{
			log_e("Error: Incorrect data size received in dial Data. Data size is %i", Wire.available());
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
