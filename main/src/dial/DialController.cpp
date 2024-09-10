#include <PinConfig.h>
#include "DialController.h"
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

    void pullDialI2C(){
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

		// FIXME: Never reaches this position..
		pullDialI2C();
		//log_i("dial_val_1: %i, dial_val_2: %i, dial_val_3: %i", dial_val_1, dial_val_2, dial_val_3);
	}
} // namespace DialController
