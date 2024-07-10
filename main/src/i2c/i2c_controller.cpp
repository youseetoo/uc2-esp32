#include "i2c_controller.h"
#include "PinConfig.h"
#include "Wire.h"
#include "cJsonTool.h"
#include "JsonKeys.h"

#include "../serial/SerialProcess.h"
#define SLAVE_ADDRESS 0x40 // I2C address of the ESP32

#define SLAVE_ADDRESS 0x40
#define MAX_I2C_BUFFER_SIZE 32

using namespace SerialProcess;
namespace i2c_controller
{
#ifdef USE_I2C
	void i2c_scan()
	{
		// TODO: We need to ensure that we are in the correct mode (e.g. I2C master mode)
		byte error, address;
		int nDevices;

		Serial.println("Scanning...");

		nDevices = 0;
		Wire.begin(pinConfig.I2C_SDA, pinConfig.I2C_SCL); // Initialize I2C with defined pins and address

		Serial.println("Sending patterns to the ESP32");
		for (int i = 0; i < 10; i++)
		{
			Wire.beginTransmission(SLAVE_ADDRESS);
			Wire.write(i); // Send the pattern number to the ESP32
			Wire.endTransmission();
			delay(500);
		}

		for (address = 1; address < 127; address++)
		{
			// The i2c_scanner uses the return value of
			// the Write.endTransmisstion to see if
			// a device did acknowledge to the address.

			Wire.beginTransmission(address);

			error = Wire.endTransmission();

			if (error == 0)
			{
				Serial.print("I2C device found at address 0x");
				if (address < 16)
					Serial.print("0");
				Serial.print(address, HEX);
				Serial.println("  !");

				nDevices++;
			}
			else if (error == 4)
			{
				Serial.print("Unknow error at address 0x");
				if (address < 16)
					Serial.print("0");
				Serial.println(address, HEX);
			}
		}
		if (nDevices == 0)
			Serial.println("No I2C devices found\n");
		else
			Serial.println("done\n");
	}

	void setup()
	{
		// Begin I2C slave communication with the defined pins and address
		if (pinConfig.I2C_ADD_SLAVE >= 0)
		{
			Wire.begin(pinConfig.I2C_ADD_SLAVE, pinConfig.I2C_SDA, pinConfig.I2C_SCL, 100000);
			Wire.onReceive(receiveEvent);
		}
	}

	void loop()
	{
		// nothing to do here
	}

	int act(cJSON *ob)
	{
		// This converts an incoming json string from Serial into a json object and sends it to the I2C slave under a given address
		// JSON String
		// {"task":"/i2c_act", "LED":1}
		log_i("I2C act");
		log_i("Ob: %s", cJSON_Print(ob));
		if (pinConfig.I2C_ADD_SLAVE >= 0)
			return -1; // I2C is in slave mode
		int qid = cJsonTool::getJsonInt(ob, "qid");
		// TODO: Maybe it would be better to do this in Serial direclty with an additional flag for I2C communication (e.g. relay anything to I2C)
		String jsonString = "{\"task\":\"/ledarr_act\", \"led\":{\"LEDArrMode\":8, \"led_array\":[{\"id\":1, \"r\":255, \"g\":255, \"b\":255},{\"id\":2, \"r\":255, \"g\":255, \"b\":255},{\"id\":3, \"r\":255, \"g\":255, \"b\":255},{\"id\":4, \"r\":255, \"g\":255, \"b\":255},{\"id\":5, \"r\":255, \"g\":255, \"b\":255},{\"id\":6, \"r\":255, \"g\":255, \"b\":255},{\"id\":7, \"r\":255, \"g\":255, \"b\":255},{\"id\":8, \"r\":255, \"g\":255, \"b\":255},{\"id\":9, \"r\":255, \"g\":255, \"b\":255}]}}";
		uint8_t slave_addr = SLAVE_ADDRESS;
		sendJsonString(jsonString, slave_addr);
		// TODO: we would need to wait for some repsonse - or better have a reading queue for the I2C devices to send back data to the serial
		return qid;
	}

	cJSON *get(cJSON *ob)
	{
		// scan I2C for all devices
		// {"task":"/i2c_get"}
		if (!pinConfig.I2C_ADD_SLAVE >= 0)
			i2c_scan(); // I2C is in slave mode
		log_i("I2C get");
		cJSON *j = cJSON_CreateObject();
		cJSON *ls = cJSON_CreateObject();
		cJSON_AddItemToObject(j, "i2c", ls);
		cJsonTool::setJsonInt(ls, "Info", 1);
		return j;
	}

	void sendJsonString(String jsonString, uint8_t slave_addr)
	{
		// This sends a json string to the I2C slave under a given address
		Wire.begin(pinConfig.I2C_SDA, pinConfig.I2C_SCL); // switch to I2C master mode ??
		int jsonLength = jsonString.length();
		int totalPackets = (jsonLength + MAX_I2C_BUFFER_SIZE - 3) / (MAX_I2C_BUFFER_SIZE - 3); // Calculate total packets

		for (int i = 0; i < totalPackets; i++)
		{
			Wire.beginTransmission(slave_addr);

			Wire.write(i);			  // Packet index
			Wire.write(totalPackets); // Total packets

			int startIdx = i * (MAX_I2C_BUFFER_SIZE - 3);
			int endIdx = startIdx + (MAX_I2C_BUFFER_SIZE - 3);

			if (endIdx > jsonLength)
			{
				endIdx = jsonLength;
			}

			for (int j = startIdx; j < endIdx; j++)
			{
				Wire.write(jsonString[j]);
			}

			Wire.endTransmission();
			vTaskDelay(2);
		}
	}

	void receiveEvent(int numBytes)
	{
		/*
		This receives data over I2C that gets parsed into a json for later action. 
		Potentially we need to add the message to the serial queue for processing on 
		individual devices?
		*/
		static String receivedJsonString = "";
		static int expectedPackets = 0;
		static int receivedPackets = 0;

		log_i("Received %d bytes", numBytes);
		while (Wire.available())
		{
			if (numBytes < 3)
				return; // Invalid packet

			int packetIndex = Wire.read();
			int totalPackets = Wire.read();

			if (packetIndex == 0)
			{
				receivedJsonString = "";
				expectedPackets = totalPackets;
				receivedPackets = 0;
			}

			while (Wire.available())
			{
				char c = Wire.read();
				receivedJsonString += c;
			}

			receivedPackets++;

			if (receivedPackets == expectedPackets)
			{
				Serial.println("Complete JSON received:");
				Serial.println(receivedJsonString);

				// Process the JSON string
				cJSON *root = cJSON_Parse(receivedJsonString.c_str());
				if (root != NULL)
				{
					// Add the JSON to the serial queue for processing
					SerialProcess::addJsonToQueue(root);
				}
				else
				{
					const char *error_ptr = cJSON_GetErrorPtr();
					if (error_ptr != NULL)
						log_i("error while parsing:%s", error_ptr);
					log_i("I2C input is null");
				}

				// Reset for the next message
				receivedJsonString = "";
				expectedPackets = 0;
				receivedPackets = 0;
			}
		}
	}
#endif
} // namespace i2c_controller
