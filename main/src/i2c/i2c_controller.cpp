// if I2C_SLAVE and I2C_MASTer defined at the same time throw an error
#if defined(I2C_SLAVE) && defined(I2C_MASTER)
#error "I2C_SLAVE and I2C_MASTER cannot be defined at the same time."
#endif

#include "i2c_controller.h"
#include "PinConfig.h"
#include "Wire.h"
#include "cJsonTool.h"
#include "JsonKeys.h"
#define I2C_SLAVE_ADDR 0x60

#ifdef MOTOR_CONTROLLER
#include "../motor/FocusMotor.h"
#endif
#ifdef DIAL_CONTROLLER
#include "../dial/DialController.h"
#endif

#include "../serial/SerialProcess.h"
#define MAX_I2C_BUFFER_SIZE 32

using namespace SerialProcess;
namespace i2c_controller
{

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

	

	void setup()
	{

// Begin I2C slave communication with the defined pins and address
#ifdef I2C_SLAVE
		// log_i("I2C Slave mode on address %i", pinConfig.I2C_ADD_SLAVE);
		Wire.begin(pinConfig.I2C_ADD_SLAVE, pinConfig.I2C_SDA, pinConfig.I2C_SCL, 100000);
		Wire.onReceive(receiveEvent);
		Wire.onRequest(requestEvent);
#endif
	}


	int act(cJSON *ob)
	{
		// This converts an incoming json string from Serial into a json object and sends it to the I2C slave under a given address
		// JSON String
		// {"task":"/i2c_act", "LED":1}

		/*

		/////////////////////
		THIS IS OLD CODE!!!!!
		/////////////////////

		*/
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
		#ifdef I2C_MASTER
		cJsonTool::setJsonInt(ls, "isI2CMaster", 1);
		#endif
		#ifdef I2C_SLAVE
		cJsonTool::setJsonInt(ls, "isI2CSlave", 1);
		#endif
		cJsonTool::setJsonInt(ls, "deviceAddress", pinConfig.I2C_ADD_SLAVE);
		cJsonTool::setJsonInt(ls, "pinSDA", pinConfig.I2C_SDA);
		cJsonTool::setJsonInt(ls, "pinSCL", pinConfig.I2C_SCL);
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
		// log_i("Receive Event");
		if (pinConfig.I2C_CONTROLLER_TYPE == I2CControllerType::mMOTOR)
		{
			parseMotorEvent(numBytes);
		}
		else if (pinConfig.I2C_CONTROLLER_TYPE == I2CControllerType::mLASER)
		{
			// Handle incoming data for the laser controller
			// parseLaserEvent(numBytes);
		}
		else if (pinConfig.I2C_CONTROLLER_TYPE == I2CControllerType::mLED)
		{
			// Handle incoming data for the LED controller
			// parseLEDEvent(numBytes);
		}
		else if (pinConfig.I2C_CONTROLLER_TYPE == I2CControllerType::mDIAL)
		{
			// Handle incoming data for the motor controller
			parseDialEvent(numBytes);
		}
		else
		{
			// Handle error: I2C controller type not supported
			log_e("Error: I2C controller type not supported.");
		}
	}

	void parseDialEvent(int numBytes)
	{
// We will receive the array of 4 positions and one intensity from the master and have to update the dial display
#ifdef DIAL_CONTROLLER
		if (numBytes == sizeof(DialController::mDialData))
		{
			//log_i("Received DialData from I2C");

			// Read the data into the mDialData struct
			Wire.readBytes((uint8_t *)&DialController::mDialData, sizeof(DialController::mDialData));

			/*
			// Now you can access the motor positions/intensity and update the dial display
			int pos_a = DialController::mDialData.pos_a;
			int pos_x = DialController::mDialData.pos_x;
			int pos_y = DialController::mDialData.pos_y;
			int pos_z = DialController::mDialData.pos_z;
			int intensity = DialController::mDialData.intensity;
			*/
			// Perform your specific actions here, such as updating the dial display.
			DialController::setDialValues(DialController::mDialData);
		}
		else
		{
			// Handle error: received data size does not match expected size
			log_e("Error: Received data size does not match DialData size.");
		}
#endif
	}

	void parseMotorEvent(int numBytes)
	{
#ifdef MOTOR_CONTROLLER
		// incoming command from I2C master will be converted to a motor action
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
			// FocusMotor::setData(pinConfig.I2C_MOTOR_AXIS, &receivedMotorData);
			FocusMotor::getData()[mStepper]->qid = receivedMotorData.qid;
			FocusMotor::getData()[mStepper]->isEnable = receivedMotorData.isEnable;
			FocusMotor::getData()[mStepper]->targetPosition = receivedMotorData.targetPosition;
			FocusMotor::getData()[mStepper]->absolutePosition = receivedMotorData.absolutePosition;
			FocusMotor::getData()[mStepper]->speed = receivedMotorData.speed;
			FocusMotor::getData()[mStepper]->acceleration = receivedMotorData.acceleration;
			FocusMotor::getData()[mStepper]->isforever = receivedMotorData.isforever;
			FocusMotor::getData()[mStepper]->isStop = receivedMotorData.isStop;
			// prevent the motor from getting stuck
			if (FocusMotor::getData()[mStepper]->acceleration <= 0)
			{
				FocusMotor::getData()[mStepper]->acceleration = MAX_ACCELERATION_A;
			}
			if (FocusMotor::getData()[mStepper]->speed <= 0)
			{
				FocusMotor::getData()[mStepper]->speed = 1000;
			}

			FocusMotor::toggleStepper(mStepper, FocusMotor::getData()[mStepper]->isStop);
			log_i("Received MotorData from I2C");
			log_i("MotorData:");
			log_i("  qid: %i", (int)FocusMotor::getData()[mStepper]->qid);
			log_i("  isEnable: %i", (bool)FocusMotor::getData()[mStepper]->isEnable);
			log_i("  targetPosition: %i", (int)FocusMotor::getData()[mStepper]->targetPosition);
			log_i("  absolutePosition: %i", (bool)FocusMotor::getData()[mStepper]->absolutePosition);
			log_i("  speed: %i", (int)FocusMotor::getData()[mStepper]->speed);
			log_i("  acceleration: %i", (bool)FocusMotor::getData()[mStepper]->acceleration);
			log_i("  isforever: %i", (bool)FocusMotor::getData()[mStepper]->isforever);
			log_i("  isEnable: %i", (bool)FocusMotor::getData()[mStepper]->isEnable);
			log_i("  isStop: %i", (bool)FocusMotor::getData()[mStepper]->isStop);

			// Now `receivedMotorData` contains the deserialized data
			// You can process `receivedMotorData` as needed
			// bool isStop = receivedMotorData.isStop;
		}
		else
		{
			// Handle error: received data size does not match expected size
			log_e("Error: Received data size does not match MotorData size.");
		}
#endif
	}

	void requestEvent()
	{
// The master request data from the slave
// !THIS IS ONLY EXECUTED IN I2C SLAVE MODE!
#ifdef I2C_SLAVE
		// for the motor we would need to send the current position and the state of isRunning
		if (pinConfig.I2C_CONTROLLER_TYPE == I2CControllerType::mMOTOR)
		{
#ifdef MOTOR_CONTROLLER
			// The master request data from the slave
			MotorState motorState;
			bool isRunning = !FocusMotor::getData()[pinConfig.I2C_MOTOR_AXIS]->stopped;
			long currentPosition = FocusMotor::getData()[pinConfig.I2C_MOTOR_AXIS]->currentPosition;
			motorState.currentPosition = currentPosition;
			motorState.isRunning = isRunning;
			// Serial.println("motor is running: " + String(motorState.isRunning));
			Wire.write((uint8_t *)&motorState, sizeof(MotorState));
#else
			Wire.write(0);
#endif
		}
		else if (pinConfig.I2C_CONTROLLER_TYPE == I2CControllerType::mDIAL)
		{
#ifdef DIAL_CONTROLLER
			// The master request data from the slave
			DialData dialDataMotor;

			// @KillerInk is there a smarter way to do this?
			// make a copy of the current dial data
			dialDataMotor = DialController::getDialValues();
			Wire.write((uint8_t *)&dialDataMotor, sizeof(DialData));
			// WARNING!! The log_i causes confusion in the I2C communication, but the values are correct
			// log_i("DialData sent to I2C master: %i, %i, %i, %i", dialDataMotor.pos_abs[0], dialDataMotor.pos_abs[1], dialDataMotor.pos_abs[2], dialDataMotor.pos_abs[3]);

#endif
		}
		else
		{
			// Handle error: I2C controller type not supported
			log_e("Error: I2C controller type not supported.");
			Wire.write(0);
		}
#endif
	}

} // namespace i2c_controller
