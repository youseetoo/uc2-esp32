#include <PinConfig.h>
#include "DialController.h"
#include "Arduino.h"
#include "../../JsonKeys.h"
#include "cJsonTool.h"
#include "../i2c/tca_controller.h"

kjhjkh
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

	void pullDialI2C()
	{
		uint8_t slave_addr = pinConfig.I2C_ADD_M5_DIAL;
		// Request data from the slave but only if inside i2cAddresses
		if (!i2c_controller::isAddressInI2CDevices(slave_addr))
		{
			return MotorState();
		}
		Wire.requestFrom(slave_addr, sizeof(MotorState));
		MotorState motorState; // Initialize with default values
		// Check if the expected amount of data is received
		if (Wire.available() == sizeof(MotorState))
		{
			Wire.readBytes((uint8_t *)&motorState, sizeof(motorState));
		}
		else
		{
			log_e("Error: Incorrect data size received");
		}

		return motorState;
	}

	void loop()
	{

		if (pinConfig.IS_I2C_MASTER)
		{
			pullDialI2C(); // here we want to pull the dial data from the I2C bus and assign it to the motors
		}
		else
		{
#ifdef M5DIAL
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
				USBSerial.printf("Touch duration: %d\n", touchDuration);
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
		}
#endif
		// log_i("dial_val_1: %i, dial_val_2: %i, dial_val_3: %i", dial_val_1, dial_val_2, dial_val_3);
	}

	void setup()
	{
// Here you can setup the dial controller
// For example you can setup the I2C bus
// or setup the M5Stack Dial
#ifdef M5DIAL
		USBSerial.begin(115200); this prevents us from uploading firmware without pushing the boot button 
		auto cfg = M5.config();
		M5Dial.begin(cfg, true, false);
		M5Dial.Display.setTextColor(WHITE);
		M5Dial.Display.setTextDatum(middle_center);
		M5Dial.Display.setTextFont(&fonts::Orbitron_Light_32);
		M5Dial.Display.setTextSize(1);
		M5Dial.Display.drawString("X=" + String(positions[currentAxis]), M5Dial.Display.width() / 2,
								  M5Dial.Display.height() / 2);
		USBSerial.println("M5Dial setup");
#endif
	}
} // namespace DialController
