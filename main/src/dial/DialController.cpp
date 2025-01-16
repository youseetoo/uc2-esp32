#include <PinConfig.h>
#include "DialController.h"
#include "../motor/FocusMotor.h"
#include "../home/HomeMotor.h"
#include "../laser/LaserController.h"
#include "Arduino.h"
#include "../../JsonKeys.h"
#include "cJsonTool.h"
#include "../i2c/tca_controller.h"

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
#ifdef I2C_SLAVE_DIAL
		M5Dial.Display.clear();
		M5Dial.Display.drawString(text, M5Dial.Display.width() / 2, M5Dial.Display.height() / 2);
#endif
	}

	void updateDisplay()
	{
#ifdef I2C_SLAVE_DIAL
		M5Dial.Display.clear();
		M5Dial.Display.drawString(String(axisNames[currentAxis]) + "=" + String(values[currentAxis]),
								  M5Dial.Display.width() / 2, M5Dial.Display.height() / 2);
		M5Dial.Display.drawString("Step: " + String(stepSize), M5Dial.Display.width() / 2,
								  M5Dial.Display.height() / 2 + 30);
#endif
	}

	void loop()
	{
#ifdef I2C_SLAVE_DIAL
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

#ifdef I2C_SLAVE_DIAL
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
