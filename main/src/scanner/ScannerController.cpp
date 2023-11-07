
#include "ScannerController.h"
#include "cJsonTool.h"
#include "Arduino.h"
#include "PinConfig.h"
#include "JsonKeys.h"
#include "soc/timer_group_struct.h"
#include "soc/timer_group_reg.h"
#ifdef LASER_CONTROLLER
#include "../laser/LaserController.h"
#endif


#include "../../cJsonTool.h"
namespace ScannerController
{

	void loop()
	{
		if (DEBUG)
			Serial.println("Start FrameStack");
		int roundTripCounter = 0;
		/*
		  {"task": "/scanner_act",
		  "scannernFrames":100,
		  "scannerMode":"classic",
		  "scannerXFrameMin":0,
		  "scannerXFrameMax":255,
		  "scannerYFrameMin":0,
		  "scannerYFrameMax":255,
		  "scannerEnable":0,
		  "scannerXFrameMin":1,
		  "scannerXFrameMax":1,
		  "scannerYFrameMin":1,
		  "scannerYFrameMax":1,
		  "scannerXStep":15,
		  "scannerYStep":15,
		  "scannerLaserVal":32000,
		  "scannerExposure":10,
		  "scannerDelay":1000}
		*/

		for (int iFrame = 0; iFrame <= scannernFrames; iFrame++)
		{
			// shifting phase in x
			for (int idelayX = scannerXFrameMin; idelayX <= scannerXFrameMax; idelayX++)
			{
				// shifting phase in y
				for (int idelayY = scannerYFrameMin; idelayY <= scannerYFrameMax; idelayY++)
				{
					// iteratinv over all pixels in x
					for (int ix = scannerxMin; ix < scannerxMax - scannerXStep; ix += scannerXStep)
					{
						// move X-mirror
						dacWrite(scannerPinX, ix + idelayX);
						// Serial.print("X");Serial.print(ix + idelayX);

						for (int iy = scanneryMin; iy < scanneryMax - scannerYStep; iy += scannerYStep)
						{
							// move Y-mirror triangle
							int scannerPosY = 0;
							if ((roundTripCounter % 2) == 0)
							{
								scannerPosY = iy + idelayY;
							}
							else
							{
								scannerPosY = (scanneryMax - scannerYStep) - (iy + idelayY);
							}
							roundTripCounter++;
							dacWrite(scannerPinY, scannerPosY);
							// Serial.print("Y");Serial.println(scannerPosY);
							delayMicroseconds(scannerDelay);
// expose Laser
#ifdef LASER_CONTROLLER
							ledcWrite(LaserController::PWM_CHANNEL_LASER_1, scannerLaserVal); // digitalWrite(LASER_PIN_1, HIGH); //
							delayMicroseconds(scannerExposure);
							ledcWrite(LaserController::PWM_CHANNEL_LASER_1, 0); //             digitalWrite(LASER_PIN_1, LOW); //
							delayMicroseconds(scannerDelay);
#endif
						}
					}
				}
			}
		}
		scannernFrames = 0;
		if (DEBUG)
			Serial.println("Ending FrameStack");
	}

	// Custom function accessible by the API
	int act(cJSON *ob)
	{

		// here you can do something
		if (DEBUG)
			Serial.println("scanner_act_fct");

		// select scanning mode
		char *scannerMode = cJSON_GetObjectItemCaseSensitive(ob, "scannerMode")->valuestring; // (ob)["scannerMode"];

		if (strcmp(scannerMode, "pattern") == 0)
		{
			if (DEBUG)
				Serial.println("pattern");
			// individual pattern gets adressed
			int arraySize = 0;
			scannerExposure = 0;
			scannerLaserVal = 32000;
			scannernFrames = 1;
			scannerDelay = 0;
			cJsonTool::setJsonInt(ob, "scannerExposure", scannerExposure);
			cJsonTool::setJsonInt(ob, "scannerLaserVal", scannerLaserVal);
			cJsonTool::setJsonInt(ob, "scannerDelay", scannerDelay);
			cJsonTool::setJsonInt(ob, "scannernFrames", scannernFrames);
			cJsonTool::setJsonInt(ob, "arraySize", arraySize);

			for (int iFrame = 0; iFrame < scannernFrames; iFrame++)
			{
				for (int i = 0; i < arraySize; i++)
				{															// Iterate through results
					int scannerIndex = cJSON_GetArrayItem(ob, i)->valueint; // ob["i"][i]; // Implicit cast
					int scannerPosY = scannerIndex % 255;
					int scannerPosX = scannerIndex / 255;
					dacWrite(scannerPinY, scannerPosY);
					dacWrite(scannerPinX, scannerPosX);

					/*
					 * Serial.print(scannerPosY);
					Serial.print("/");
					Serial.println(scannerPosX);
					 */

					// Serial.print("Y");Serial.println(scannerPosY);
					delayMicroseconds(scannerDelay);
					// expose Laser
					#ifdef LASER_CONTROLLER
						ledcWrite(LaserController::PWM_CHANNEL_LASER_1, scannerLaserVal); // digitalWrite(LASER_PIN_1, HIGH); //
						delayMicroseconds(scannerExposure);
						ledcWrite(LaserController::PWM_CHANNEL_LASER_1, 0); //             digitalWrite(LASER_PIN_1, LOW); //
						delayMicroseconds(scannerDelay);
					#endif
				}
			}
		}

		else if (strcmp(scannerMode, "classic") == 0)
		{
			if (DEBUG)
				Serial.println("classic");

			// assert values
			scannerxMin = 0;
			scanneryMin = 0;
			scannerxMax = 255;
			scanneryMax = 255;
			scannerExposure = 0;
			scannerEnable = 0;
			scannerLaserVal = 32000;

			scannerXFrameMax = 5;
			scannerXFrameMin = 0;
			scannerYFrameMax = 5;
			scannerYFrameMin = 0;
			scannerXStep = 5;
			scannerYStep = 5;
			scannernFrames = 1;

			scannerDelay = 0;

			cJsonTool::setJsonInt(ob, "scannernFrames", scannernFrames);
			cJsonTool::setJsonInt(ob, "scannerXFrameMax", scannerXFrameMax);
			cJsonTool::setJsonInt(ob, "scannerXFrameMin", scannerXFrameMin);
			cJsonTool::setJsonInt(ob, "scannerYFrameMax", scannerYFrameMax);
			cJsonTool::setJsonInt(ob, "scannerYFrameMin", scannerYFrameMin);
			cJsonTool::setJsonInt(ob, "scannerXStep", scannerXStep);
			cJsonTool::setJsonInt(ob, "scannerYStep", scannerYStep);
			cJsonTool::setJsonInt(ob, "scannerxMin", scannerxMin);
			cJsonTool::setJsonInt(ob, "scannerLaserVal", scannerLaserVal);
			cJsonTool::setJsonInt(ob, "scanneryMin", scanneryMin);
			cJsonTool::setJsonInt(ob, "scannerxMax", scannerxMax);
			cJsonTool::setJsonInt(ob, "scanneryMax", scanneryMax);
			cJsonTool::setJsonInt(ob, "scannerExposure", scannerExposure);
			cJsonTool::setJsonInt(ob, "scannerEnable", scannerEnable);
			cJsonTool::setJsonInt(ob, "scannerDelay", scannerDelay);

			if (DEBUG)
				Serial.print("scannerxMin ");
			Serial.println(scannerxMin);
			if (DEBUG)
				Serial.print("scanneryMin ");
			Serial.println(scanneryMin);
			if (DEBUG)
				Serial.print("scannerxMax ");
			Serial.println(scannerxMax);
			if (DEBUG)
				Serial.print("scanneryMax ");
			Serial.println(scanneryMax);
			if (DEBUG)
				Serial.print("scannerExposure ");
			Serial.println(scannerExposure);
			if (DEBUG)
				Serial.print("scannerEnable ");
			Serial.println(scannerEnable);
			if (DEBUG)
				Serial.print("scannerLaserVal ");
			Serial.println(scannerLaserVal);
			if (DEBUG)
				Serial.print("scannerXFrameMax ");
			Serial.println(scannerXFrameMax);
			if (DEBUG)
				Serial.print("scannerXFrameMin ");
			Serial.println(scannerXFrameMin);
			if (DEBUG)
				Serial.print("scannerYFrameMax ");
			Serial.println(scannerYFrameMax);
			if (DEBUG)
				Serial.print("scannerYFrameMin ");
			Serial.println(scannerYFrameMin);
			if (DEBUG)
				Serial.print("scannerXStep ");
			Serial.println(scannerXStep);
			if (DEBUG)
				Serial.print("scannerYStep ");
			Serial.println(scannerYStep);
			if (DEBUG)
				Serial.print("scannernFrames ");
			Serial.println(scannernFrames);
			if (DEBUG)
				Serial.print("scannerDelay ");
			Serial.println(scannerDelay);

			if (DEBUG)
				Serial.println("Start controlGalvoTask");
			isScanRunning = scannerEnable; // Trigger a frame acquisition
			if (DEBUG)
				Serial.println("Done with setting up Tasks");
		}
		return 1;
	}

	/***************************************************************************************************/
	/******************************************* SCANNER     *******************************************/
	/***************************************************************************************************/
	/****************************************** ********************************************************/

	void setup()
	{
		log_d("Setup ScannerController");
		loop(); // run not as a task
		disableCore0WDT();
		xTaskCreate(controlGalvoTask, "controlGalvoTask", 10000, NULL, 1, NULL);
	}

	void controlGalvoTask(void *parameter)
	{
		Serial.println("Starting Scanner Thread");
		while (1)
		{
			// loop forever
			if (isScanRunning || scannernFrames > 0)
			{
				loop();
			}
			else
			{
				vTaskDelay(100);
			}
		}
		vTaskDelete(NULL);
	}
}