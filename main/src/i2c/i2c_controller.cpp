#include "i2c_controller.h"
#include "PinConfig.h"
#include "Wire.h"
#include "cJsonTool.h"
#include "JsonKeys.h"

#define SLAVE_ADDRESS 0x40 // I2C address of the ESP32

#define SLAVE_ADDRESS 0x40
#define MAX_I2C_BUFFER_SIZE 32

namespace i2c_controller
{
#ifdef USE_I2C
	void i2c_scan()
	{
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
		// scan I2C for all devices
		//	i2c_scan();
		if (0)// (pinConfig.I2C_DEV_ADDR >= 0)
		{
			Wire.begin(pinConfig.I2C_DEV_ADDR, pinConfig.I2C_SDA, pinConfig.I2C_SCL, 100000);
			Wire.onReceive(receiveEvent);
		}
	}

	void loop()
	{
		// nothing to do here
	}

	int act(cJSON *ob)
	{
		// JSON String
		// {"task":"/i2c_act", "LED":1}
		int qid = cJsonTool::getJsonInt(ob, "qid");

		Wire.begin(pinConfig.I2C_SDA, pinConfig.I2C_SCL);
		String jsonString = "{\"task\":\"/ledarr_act\", \"led\":{\"LEDArrMode\":8, \"led_array\":[{\"id\":1, \"r\":255, \"g\":255, \"b\":255},{\"id\":2, \"r\":255, \"g\":255, \"b\":255},{\"id\":3, \"r\":255, \"g\":255, \"b\":255},{\"id\":4, \"r\":255, \"g\":255, \"b\":255},{\"id\":5, \"r\":255, \"g\":255, \"b\":255},{\"id\":6, \"r\":255, \"g\":255, \"b\":255},{\"id\":7, \"r\":255, \"g\":255, \"b\":255},{\"id\":8, \"r\":255, \"g\":255, \"b\":255},{\"id\":9, \"r\":255, \"g\":255, \"b\":255}]}}";
		uint8_t slave_addr = SLAVE_ADDRESS;
		sendJsonString(jsonString, slave_addr);
		/*
		// assign values
		int LASERid = 0;
		LASERid = cJsonTool::getJsonInt(ob, "LASERid");
		// debugging
		log_i("LaserID  %i", LASERid);
		*/
		return qid;
	}

	cJSON *get(cJSON *ob)
	{
		cJSON *j = cJSON_CreateObject();
		cJSON *ls = cJSON_CreateObject();
		cJSON_AddItemToObject(j, "i2c", ls);
		cJsonTool::setJsonInt(ls, "Info", 1);
		return j;
	}

	void sendJsonString(String jsonString, uint8_t slave_addr)
	{
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
		String receivedJsonString = "";
		int expectedPackets = 0;
		int receivedPackets = 0;

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
			}
		}
	}
#endif
} // namespace i2c_controller
