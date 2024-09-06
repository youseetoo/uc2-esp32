#include "i2c_controller.h"
#include "PinConfig.h"
#include "Wire.h"
#include "cJsonTool.h"
#include "JsonKeys.h"

#include "../motor/FocusMotor.h"

#include "../serial/SerialProcess.h"
#define MAX_I2C_BUFFER_SIZE 32

using namespace SerialProcess;
namespace i2c_controller
{
#ifdef USE_I2C

	void i2c_scan()
	{
		// Ensure we start with no stored addresses
		numDevices = 0;

		byte error, address;

		log_i("Scanning...");

		for (address = 1; address < 127; address++)
		{
			// Begin I2C transmission to check if a device acknowledges at this address
			Wire.beginTransmission(address);
			error = Wire.endTransmission();

			if (error == 0)
			{
				log_i("I2C device found at address 0x%02X  !", address);
				// Store the address if the device is found and space is available in the array
				if (numDevices < MAX_I2C_DEVICES)
				{
					i2cAddresses[numDevices] = address;
					numDevices++;
				}
				else
				{
					log_i("Maximum number of I2C devices reached. Increase MAX_I2C_DEVICES if needed.");
					break; // Exit the loop if we've reached the maximum storage capacity
				}
			}
			else if (error == 4)
			{
				log_e("Unknown device found at address 0x%02X  !", address);
			}
		}

		if (numDevices == 0)
			log_i("No I2C devices found\n");
		else
		{
			log_i("Found %d I2C devices.", numDevices);
			log_i("Addresses of detected devices:");
			for (int i = 0; i < numDevices; i++)
			{
				if (i2cAddresses[i] < 16)
				{
					log_i("0x0%X", i2cAddresses[i]);
				}
				else
				{
					log_i("0x%X", i2cAddresses[i]);
				}
			}
		}
	}

	bool isAddressInI2CDevices(byte addressToCheck)
	{
		for (int i = 0; i < numDevices; i++) // Iterate through the array
		{
			if (i2cAddresses[i] == addressToCheck) // Check if the current element matches the address
			{
				return true; // Address found in the array
			}
		}
		return false; // Address not found
	}

	void setup()
	{
		// Begin I2C slave communication with the defined pins and address
		if (pinConfig.IS_I2C_SLAVE and pinConfig.I2C_ADD_SLAVE > 0) // this address has to be loaded á la 0,1,2,3 => A,X,Y,Z
		{
			log_i("I2C Slave mode on address %i", pinConfig.I2C_ADD_SLAVE);
			Wire.begin(pinConfig.I2C_ADD_SLAVE, pinConfig.I2C_SDA, pinConfig.I2C_SCL, 100000);
			Wire.onReceive(receiveEvent);
			Wire.onRequest(requestEvent);
		}
		else if (pinConfig.IS_I2C_MASTER)
		{
			// TODO: We need a receiver for the Master too
			i2c_scan();
		}
	}

	void loop()
	{
		// nothing to do here
		if (i2cRescanTick > 10000 and pinConfig.IS_I2C_MASTER)
		{
			log_i("Rescan I2C");
			i2c_scan();
			i2cRescanTick = 0;
		}
		i2cRescanTick++;
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

		if (pinConfig.I2C_CONTROLLER_TYPE == I2CControllerType::mMOTOR)
		{
			uint8_t slave_addr = pinConfig.I2C_ADD_MOT_X;
			sendJsonString(jsonString, slave_addr);
		}
		// TODO: we would need to wait for some repsonse - or better have a reading queue for the I2C devices to send back data to the serial
		return qid;
	}

	cJSON *get(cJSON *ob)
	{
		// scan I2C for all devices
		// {"task":"/i2c_get"}
		if (!(pinConfig.I2C_ADD_SLAVE >= 0))
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
		// Master and Slave
		log_i("Receive Event");
		if (pinConfig.I2C_CONTROLLER_TYPE == I2CControllerType::mMOTOR)
		{
			if (numBytes == sizeof(MotorData))
			{
				MotorData receivedMotorData;
				uint8_t *dataPtr = (uint8_t *)&receivedMotorData;
				for (int i = 0; i < numBytes; i++)
				{
					dataPtr[i] = Wire.read();
				}
				// assign the received data to the motor to MotorData *data[4];
				Stepper mStepper = static_cast<Stepper>(pinConfig.I2C_MOTOR_AXIS);
				FocusMotor::setData(pinConfig.I2C_MOTOR_AXIS, &receivedMotorData);
				log_i("Received MotorData from I2C");
				log_i("MotorData:");
				log_i("  currentPosition: %i", receivedMotorData.currentPosition);
				log_i("  targetPosition: %i", receivedMotorData.targetPosition);
				log_i("  stopped: %i", receivedMotorData.stopped);
				// Now `receivedMotorData` contains the deserialized data
				// You can process `receivedMotorData` as needed
				bool isStop = receivedMotorData.isStop;
				FocusMotor::toggleStepper(mStepper, isStop);
			}
			else
			{
				// Handle error: received data size does not match expected size
				log_e("Error: Received data size does not match MotorData size.");
			}
		}
		else
		{
			// Handle error: I2C controller type not supported
			log_e("Error: I2C controller type not supported.");
		}
	}

	void requestEvent()
	{
		// The master request data from the slave

		// for the motor we would need to send the current position and the state of isRunning
		if (pinConfig.I2C_CONTROLLER_TYPE == I2CControllerType::mMOTOR)
		{
			// The master request data from the slave
			MotorState motorState;

			// log_i("Sending MotorState to I2C, currentPosition: %i, isRunning: %i", (int)focusMotorData[pinConfig.I2C_MOTOR_AXIS]->currentPosition, (bool)!focusMotorData[pinConfig.I2C_MOTOR_AXIS]->stopped);
			
			bool isRunning = !FocusMotor::getData()[pinConfig.I2C_MOTOR_AXIS]->stopped;
			long currentPosition = FocusMotor::getData()[pinConfig.I2C_MOTOR_AXIS]->currentPosition;
			Serial.println("motor is running: " + String(isRunning));
			/*motorState.currentPosition = currentPosition;
			motorState.isRunning = isRunning;
			Serial.println("motor is running: " + String(motorState.isRunning));
			Wire.write((uint8_t *)&motorState, sizeof(MotorState));
			*/
		}
		else
		{
			// Handle error: I2C controller type not supported
			log_e("Error: I2C controller type not supported.");
			Wire.write(0);
		}
	}

#endif
} // namespace i2c_controller

/*

READ OUT REGISTERS:


#include <Wire.h>

#define SLAVE_ADDRESS 0x08

// Define the number of registers and the data they hold
#define NUM_REGISTERS 5

// Register array to hold different types of data
volatile int registers[NUM_REGISTERS] = {0, 1, 2, 3, 4}; // Example initial values
volatile int currentRegister = 0; // Variable to keep track of which register to respond with

// Function to handle when the master requests data
void requestEvent() {
	// Send the current register's data to the master
	Wire.write((byte*)&registers[currentRegister], sizeof(registers[currentRegister]));
}

// Function to handle when the master sends data to the slave
void receiveEvent(int numBytes) {
	if (numBytes >= 1) {
		// First byte indicates which register is being addressed
		currentRegister = Wire.read();
		if (currentRegister < NUM_REGISTERS && numBytes == 2) {
			// If there is a second byte, it's the data to write into the register
			registers[currentRegister] = Wire.read();
		}
	}
}

void setup() {
	Wire.begin(SLAVE_ADDRESS); // Join I2C bus as slave with address 0x08
	Wire.onRequest(requestEvent); // Register the request event
	Wire.onReceive(receiveEvent); // Register the receive event

	Serial.begin(115200);
}

void loop() {
	// For demonstration, increment register 0 value every second
	registers[0]++;
	delay(1000);
	Serial.print("Register 0: ");
	Serial.println(registers[0]);
}


#include <Wire.h>

#define SLAVE_ADDRESS 0x08

int requestedRegister = 0;
int receivedData = 0;

void requestRegister(int registerNumber) {
	// Begin transmission to the slave
	Wire.beginTransmission(SLAVE_ADDRESS);
	Wire.write(registerNumber); // Send the register number to the slave
	Wire.endTransmission(false); // End transmission, but keep the connection alive

	// Request the data from the slave (expecting an int size)
	Wire.requestFrom(SLAVE_ADDRESS, sizeof(receivedData));

	if (Wire.available() >= sizeof(receivedData)) {
		receivedData = Wire.read(); // Read the data into receivedData
	}

	// Print the received data to the Serial Monitor
	Serial.print("Data from register ");
	Serial.print(registerNumber);
	Serial.print(": ");
	Serial.println(receivedData);
}

void setup() {
	Wire.begin(); // Join I2C bus as master
	Serial.begin(115200);
}

void loop() {
	// Request data from register 0
	requestRegister(0);
	delay(1000); // Wait before requesting again

	// Optionally, request data from other registers
	requestedRegister = (requestedRegister + 1) % 5; // Cycle through registers 0 to 4
	requestRegister(requestedRegister);
	delay(1000);
}


*/