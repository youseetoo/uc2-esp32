#include <PinConfig.h>
#include "../../config.h"
#include "FocusMotor.h"
#include "Wire.h"
#include "../wifi/WifiController.h"
#include "../../cJsonTool.h"
#include "../state/State.h"
#include "esp_debug_helpers.h"
#ifdef USE_TCA9535
#include "../i2c/tca_controller.h"
#endif
#ifdef USE_FASTACCEL
#include "FAccelStep.h"
#endif
#ifdef USE_ACCELSTEP
#include "AccelStep.h"
#endif
#ifdef STAGE_SCAN
#include "StageScan.h"
#endif
#ifdef DIAL_CONTROLLER
#include "../dial/DialController.h"
#endif
#ifdef I2C_SLAVE_MOTOR
#include "../i2c/i2c_slave_motor.h"
#endif
#ifdef I2C_MASTER
#include "../i2c/i2c_master.h"
#endif

namespace FocusMotor
{

	MotorData a_dat;
	MotorData x_dat;
	MotorData y_dat;
	MotorData z_dat;
	MotorData *data[4];

	// for A,X,Y,Z intialize the I2C addresses
	uint8_t i2c_addresses[] = {
		pinConfig.I2C_ADD_MOT_A,
		pinConfig.I2C_ADD_MOT_X,
		pinConfig.I2C_ADD_MOT_Y,
		pinConfig.I2C_ADD_MOT_Z};

	MotorData **getData()
	{
		if (data != nullptr)
			return data;
		else
		{
			MotorData *mData[4];
			return mData;
		}
	}

	void setData(int axis, MotorData *mData)
	{
		memcpy(data[axis], mData, sizeof(MotorData));
		// getData()[axis] = mData;
	}

#ifdef WIFI
	void sendUpdateToClients(void *p)
	{
		for (;;)
		{
			cJSON *root = cJSON_CreateObject();
			cJSON *stprs = cJSON_CreateArray();
			cJSON_AddItemToObject(root, key_steppers, stprs);
			int added = 0;
			for (int i = 0; i < 4; i++)
			{
				if (!data[i]->stopped)
				{
					updateData(i);
					cJSON *item = cJSON_CreateObject();
					cJSON_AddItemToArray(stprs, item);
					cJSON_AddNumberToObject(item, key_stepperid, i);
					cJSON_AddNumberToObject(item, key_position, data[i]->currentPosition);
					cJSON_AddNumberToObject(item, "isDone", data[i]->stopped);
					added++;
				}
			}
			if (added > 0)
			{
#ifdef WIFI
				WifiController::sendJsonWebSocketMsg(root);
#endif
				// print result - will that work in the case of an xTask?
				Serial.println("++");
				char *s = cJSON_PrintUnformatted(root);
				Serial.println(s);
				free(s);
				Serial.println("--");
			}
#ifdef I2C_MASTER and defined DIAL_CONTROLLER
			i2c_master::pushMotorPosToDial();
#endif
			cJSON_Delete(root);
			vTaskDelay(1000 / portTICK_PERIOD_MS);
		}
	}
#endif

	void startStepper(int axis, bool reduced = false)
	{
		// log_i("startStepper %i at speed %i and targetposition %i", axis, getData()[axis]->speed, getData()[axis]->targetPosition);
		//  ensure isStop is false
		getData()[axis]->isStop = false;
#if defined(I2C_MASTER) && defined(I2C_MOTOR)
		// Request data from the slave but only if inside i2cAddresses
		uint8_t slave_addr = i2c_master::axis2address(axis);
		if (!i2c_master::isAddressInI2CDevices(slave_addr))
		{
			getData()[axis]->stopped = true; // stop immediately, so that the return of serial gives the current position
			sendMotorPos(axis, 0);			 // this is an exception. We first get the position, then the success
		}
		else
		{
			// we need to wait for the response from the slave to be sure that the motor is running (e.g. motor needs to run before checking if it is stopped)
			MotorData *m = getData()[axis];
			i2c_master::startStepper(m, axis, reduced);
			waitForFirstRun[axis] = 1;
		}
#elif defined USE_FASTACCEL
		FAccelStep::startFastAccelStepper(axis);
#elif defined USE_ACCELSTEP
		AccelStep::startAccelStepper(axis);
#endif
		getData()[axis]->stopped = false;
	}


	void toggleStepper(Stepper s, bool isStop, bool reduced)
	{
		if (isStop)
		{
			stopStepper(s);
		}
		else
		{
			startStepper(s, reduced); // TODO: Need dual axis?
		}
	}

	void moveMotor(long pos, int s, bool isRelative)
	{
		// move motor
		// blocking = true => wait until motor is done
		// blocking = false => fire and forget
		data[s]->targetPosition = pos;
		data[s]->isforever = false;
		data[s]->absolutePosition = !isRelative;
		data[s]->isStop = false;
		data[s]->stopped = false;
		data[s]->speed = 2000;
		data[s]->acceleration = 1000;
		data[s]->isaccelerated = 1;
		startStepper(s, false);
	}

	void enable(bool en)
	{
#ifdef USE_FASTACCEL
		FAccelStep::Enable(en);
#elif defined USE_ACCELSTEP
		AccelStep::Enable(en);
#endif
	}

	void setAutoEnable(bool enable)
	{
#ifdef USE_FASTACCEL
		FAccelStep::setAutoEnable(enable);
#endif
	}

	void setEnable(bool enable)
	{
#ifdef USE_FASTACCEL
		FAccelStep::Enable(enable);
#elif defined USE_ACCELSTEP
		AccelStep::Enable(enable);
#endif
	}

    void setDualAxisZ(bool dual)
    {
		isDualAxisZ = dual;
    }

    bool getDualAxisZ()
    {
        return isDualAxisZ;
    }

    void updateData(int axis)
	{
// Request the current position from the slave motors depending on the interface
#ifdef USE_FASTACCEL
		FAccelStep::updateData(axis);
#elif defined USE_ACCELSTEP
		AccelStep::updateData(axis);
#elif defined I2C_MASTER
		MotorState mMotorState = i2c_master::pullMotorDataI2CDriver(axis);
		data[axis]->currentPosition = mMotorState.currentPosition;
		// data[axis]->isforever = mMotorState.isforever;
#endif
	}

	void setup()
	{
		data[Stepper::A] = &a_dat;
		data[Stepper::X] = &x_dat;
		data[Stepper::Y] = &y_dat;
		data[Stepper::Z] = &z_dat;
		if (data[Stepper::A] == nullptr)
			log_e("Stepper A data NULL");
		if (data[Stepper::X] == nullptr)
			log_e("Stepper X data NULL");
		if (data[Stepper::Y] == nullptr)
			log_e("Stepper Y data NULL");
		if (data[Stepper::Z] == nullptr)
			log_e("Stepper Z data NULL");

		// Read dual axis from preferences if available
		Preferences preferences;
		const char *prefNamespace = "UC2";
		preferences.begin(prefNamespace, false);
		isDualAxisZ = preferences.getBool("dualAxZ", pinConfig.isDualAxisZ);
		preferences.end();

		// setup motor pins
		log_i("Setting Up Motor A,X,Y,Z");
#ifdef USE_FASTACCEL || USE_ACCELSTEP
		preferences.begin("motpos", false);
		if (pinConfig.MOTOR_A_STEP >= 0)
		{
			data[Stepper::A]->dirPin = pinConfig.MOTOR_A_DIR;
			data[Stepper::A]->stpPin = pinConfig.MOTOR_A_STEP;
			data[Stepper::A]->currentPosition = preferences.getLong(("motor" + String(Stepper::A)).c_str());
			log_i("Motor A position: %i", data[Stepper::A]->currentPosition);
		}
		if (pinConfig.MOTOR_X_STEP >= 0)
		{
			data[Stepper::X]->dirPin = pinConfig.MOTOR_X_DIR;
			data[Stepper::X]->stpPin = pinConfig.MOTOR_X_STEP;
			data[Stepper::X]->currentPosition = preferences.getLong(("motor" + String(Stepper::X)).c_str());
			log_i("Motor X position: %i", data[Stepper::X]->currentPosition);
		}
		if (pinConfig.MOTOR_Y_STEP >= 0)
		{
			data[Stepper::Y]->dirPin = pinConfig.MOTOR_Y_DIR;
			data[Stepper::Y]->stpPin = pinConfig.MOTOR_Y_STEP;
			data[Stepper::Y]->currentPosition = preferences.getLong(("motor" + String(Stepper::Y)).c_str());
			log_i("Motor Y position: %i", data[Stepper::Y]->currentPosition);
		}
		if (pinConfig.MOTOR_Z_STEP >= 0)
		{
			data[Stepper::Z]->dirPin = pinConfig.MOTOR_Z_DIR;
			data[Stepper::Z]->stpPin = pinConfig.MOTOR_Z_STEP;
			data[Stepper::Z]->currentPosition = preferences.getLong(("motor" + String(Stepper::Z)).c_str());
			log_i("Motor Z position: %i", data[Stepper::Z]->currentPosition);
		}
		preferences.end();

		// setup trigger pins
		if (pinConfig.DIGITAL_OUT_1 > 0)
			data[Stepper::X]->triggerPin = 1; // pixel^
		if (pinConfig.DIGITAL_OUT_2 > 0)
			data[Stepper::Y]->triggerPin = 2; // line^
		if (pinConfig.DIGITAL_OUT_3 > 0)
			data[Stepper::Z]->triggerPin = 3; // frame^

#endif

#ifdef USE_FASTACCEL
#ifdef USE_TCA9535
		log_i("Setting external pin for FastAccelStepper");
		FAccelStep::setExternalCallForPin(tca_controller::setExternalPin);
#endif
		FAccelStep::setupFastAccelStepper();
#elif defined USE_ACCELSTEP
#ifdef USE_TCA9535
		AccelStep::setExternalCallForPin(tca_controller::setExternalPin);
#endif
		AccelStep::setupAccelStepper();
#endif
#ifdef USE_FASTACCEL || USE_ACCELSTEP
#ifdef TCA9535
		for (int iMotor = 0; iMotor < 4; iMotor++)
		{
			// need to activate the motor's dir pin eventually
			// This also updates the dial's positions
			Stepper s = static_cast<Stepper>(iMotor);
			data[s]->absolutePosition = false;
			data[s]->targetPosition = -1;
			startStepper(iMotor, true);
			delay(10);
			stopStepper(iMotor);
			data[s]->targetPosition = 1;
			startStepper(iMotor, true);
			delay(10);
			stopStepper(iMotor);
		}
#else
		// send motor positions
		for (int iMotor = 0; iMotor < 4; iMotor++)
		{
			if (data[iMotor]->isActivated)
			{
				sendMotorPos(iMotor, 0);
			}
		}
#endif // TCA9535

#endif
#ifdef I2C_MASTER and defined I2C_MOTOR
		// send stop signal to all motors and update motor positions
		for (int iMotor = 0; iMotor < 4; iMotor++)
		{
			moveMotor(1, iMotor, true); // wake up motor
			data[iMotor]->isActivated = true;
			stopStepper(iMotor);
			MotorState mMotorState = i2c_master::pullMotorDataI2CDriver(iMotor);
			data[iMotor]->currentPosition = mMotorState.currentPosition;
		}
#endif
#if defined DIAL_CONTROLLER && defined I2C_MASTER
		// send motor positions to dial
		i2c_master::pushMotorPosToDial();
#endif
#ifdef WIFI
		// TODO: This causes the heap to overload?
		// log_i("Creating Task sendUpdateToClients");
		// xTaskCreate(sendUpdateToClients, "sendUpdateToWSClients", pinConfig.MOTOR_TASK_UPDATEWEBSOCKET_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY, NULL);
#endif
	}

	void loop()
	{
		// checks if a stepper is still running
		for (int i = 0; i < 4; i++)
		{
			#ifdef I2C_MASTER
			// seems like the i2c needs a moment to start the motor (i.e. act is async and loop is continously running, maybe faster than the motor can start)
			if(waitForFirstRun[i]){
				waitForFirstRun[i] = 0;
				continue;
			}
			#endif
			bool mIsRunning = false;
			// we check if the motor was defined
			if (getData()[i]->isActivated)
			{
				mIsRunning = isRunning(i);
			}

			// log_i("Stop Motor %i in loop, isRunning %i, data[i]->stopped %i, data[i]-speed %i, position %i", i, mIsRunning, data[i]->stopped, getData()[i]->speed, getData()[i]->currentPosition);
			if (!mIsRunning && !data[i]->stopped && !data[i]->isforever)
			{
				// If the motor is not running, we stop it, report the position and save the position
				// This is the ordinary case if the motor is not connected via I2C
				// log_d("Sending motor pos %i", i);
				log_i("Stop Motor (2) %i in loop, mIsRunning %i, data[i]->stopped %i", i, mIsRunning, data[i]->stopped);
				stopStepper(i);
				preferences.begin("motpos", false);
				preferences.putLong(("motor" + String(i)).c_str(), data[i]->currentPosition);
				preferences.end();
			}
		}
	}

	bool isRunning(int i)
	{
		bool mIsRunning = false;
#ifdef USE_FASTACCEL
		mIsRunning = FAccelStep::isRunning(i);
#elif defined USE_ACCELSTEP
		mIsRunning = AccelStep::isRunning(i);
#elif defined I2C_MASTER
		// Request data from the slave but only if inside i2cAddresses
		MotorState mData = i2c_master::getMotorState(i);
		mIsRunning = mData.isRunning;
#endif
		return mIsRunning;
	}

	// returns json {"steppers":[...]} as qid
	void sendMotorPos(int i, int arraypos)
	{
		// update current position of the motor depending on the interface
		updateData(i);

		cJSON *root = cJSON_CreateObject();
		if (root == NULL)
			return; // Handle allocation failure

		cJSON *stprs = cJSON_CreateArray();
		if (stprs == NULL)
		{
			cJSON_Delete(root);
			return; // Handle allocation failure
		}
		cJSON_AddItemToObject(root, key_steppers, stprs);
		cJSON_AddNumberToObject(root, "qid", data[i]->qid);

		cJSON *item = cJSON_CreateObject();
		if (item == NULL)
		{
			cJSON_Delete(root);
			return; // Handle allocation failure
		}
		cJSON_AddItemToArray(stprs, item);
		cJSON_AddNumberToObject(item, key_stepperid, i);
		cJSON_AddNumberToObject(item, key_position, data[i]->currentPosition);
		cJSON_AddNumberToObject(item, "isDone", data[i]->stopped);
		arraypos++;

#ifdef WIFI
		WifiController::sendJsonWebSocketMsg(root);
#endif
#ifdef I2C_MASTER and defined DIAL_CONTROLLER
		i2c_master::pushMotorPosToDial();
#endif

		// Print result - will that work in the case of an xTask?
		Serial.println("++");
		char *s = cJSON_PrintUnformatted(root);
		if (s != NULL)
		{
			Serial.println(s);
			free(s);
		}
		Serial.println("--");

		cJSON_Delete(root); // Free the root object, which also frees all nested objects
	}

	void stopStepper(int i)
	{

#if defined(I2C_MASTER) && defined(I2C_MOTOR)
		// Request data from the slave but only if inside i2cAddresses
		// esp_backtrace_print(10);
		log_i("stopStepper I2C_MASTER Focus Motor %i", i);
		uint8_t slave_addr = i2c_master::axis2address(i);
		if (!i2c_master::isAddressInI2CDevices(slave_addr))
		{
			// we need to wait for the response from the slave to be sure that the motor is running (e.g. motor needs to run before checking if it is stopped)
			getData()[i]->stopped = true; // stop immediately, so that the return of serial gives the current position
			sendMotorPos(i, 0);			  // this is an exception. We first get the position, then the success
		}
#endif

		log_i("stopStepper Focus Motor %i", i);
		// only send motor data if it was running before
		if (!data[i]->stopped)
			sendMotorPos(i, 0); // rather here or at the end? M5Dial needs the position ASAP
#ifdef USE_FASTACCEL
		FAccelStep::stopFastAccelStepper(i);
#elif defined USE_ACCELSTEP
		AccelStep::stopAccelStepper(i);
#elif defined I2C_MASTER
		getData()[i]->isforever = false;
		getData()[i]->speed = 0;
		getData()[i]->stopped = true;
		getData()[i]->isStop = true;
		MotorData *m = getData()[i];
		i2c_master::stopStepper(m, i);
#endif
	}

	long getPosition(Stepper s)
	{
		log_i("Getting motor %i position", s);
		updateData(s);
		return getData()[s]->currentPosition;
	}

	void setPosition(Stepper s, int pos)
	{
#ifdef USE_FASTACCEL
		FAccelStep::setPosition(s, pos);
#elif defined USE_ACCELSTEP
		AccelStep::setPosition(s, pos);
#endif
		getData()[s]->currentPosition = pos;
	}

	void move(Stepper s, int steps, bool blocking)
	{
#ifdef USE_FASTACCEL
		FAccelStep::move(s, steps, blocking);
#endif
	}

	bool joystick_drive_X = false;
	bool joystick_drive_Y = false;
	bool joystick_drive_Z = false;
	bool joystick_drive_A = false;
	int offset_val = 1025;

	void handleAxis(int value, int s)
	{

		if (s == Stepper::Z or (s == Stepper::A and isDualAxisZ))
		{
			// divide  to slow down
			value = (float)((int)value / (float)pinConfig.JOYSTICK_SPEED_MULTIPLIER_Z);
		}
		else
		{
			// divide  to slow down
			value = (float)((int)value / (float)pinConfig.JOYSTICK_SPEED_MULTIPLIER);
		}

		int offset_val_scaled = offset_val / pinConfig.JOYSTICK_SPEED_MULTIPLIER;
		if (s == Stepper::Z)
		{
			offset_val_scaled = offset_val / pinConfig.JOYSTICK_SPEED_MULTIPLIER_Z;
		}
		else if(offset_val_scaled < offset_val)
			offset_val_scaled = offset_val;
		if (value >= offset_val_scaled || value <= -offset_val_scaled)
		{
			// move_x
			getData()[s]->speed = value;
			getData()[s]->isforever = true;
			getData()[s]->acceleration = MAX_ACCELERATION_A;
			//log_i("Start motor from BT %i with speed %i", s, getData()[s]->speed);
			FocusMotor::startStepper(s, true);
			if (s == Stepper::X)
				joystick_drive_X = true;
			if (s == Stepper::Y)
				joystick_drive_Y = true;
			if (s == Stepper::Z)
				joystick_drive_Z = true;
			else if (s == Stepper::A)
				joystick_drive_A = true;
		}
		else if (joystick_drive_X || joystick_drive_Y || joystick_drive_Z || joystick_drive_A)
		{
			getData()[s]->speed = 0;
			getData()[s]->isforever = false;
			if (s == Stepper::X and joystick_drive_X)
			{
				FocusMotor::stopStepper(s);
				joystick_drive_X = false;
			}
			if (s == Stepper::Y and joystick_drive_Y)
			{
				FocusMotor::stopStepper(s);
				joystick_drive_Y = false;
			}
			if (s == Stepper::Z and joystick_drive_Z)
			{
				FocusMotor::stopStepper(s);
				joystick_drive_Z = false;
			}
			if (s == Stepper::A and joystick_drive_A)
			{
				FocusMotor::stopStepper(s);
				joystick_drive_A = false;
			}
		}
	}

	void xyza_changed_event(int x, int y, int z, int a)
	{
		//log_i("xyza_changed_event x:%d y:%i z:%i a:%i", x,y,z,a);
		// Only allow motion in one direction at a time
		bool zIsRunning = getData()[Stepper::Z]->isforever;
		bool aIsRunning = getData()[Stepper::A]->isforever;

		if (!aIsRunning || isDualAxisZ)
		{ // Z-direction
			handleAxis(z, Stepper::Z);
			if (isDualAxisZ)
			{ // Z-Direction
				handleAxis(z, Stepper::A);
			}
		}
		if (abs(z) < offset_val)
		{
			// force stop in case it's trapped
			handleAxis(0, Stepper::Z);
		}
		if (abs(z) < offset_val && isDualAxisZ)
		{
			// force stop in case it's trapped
			handleAxis(0, Stepper::A);
		}

		if (!zIsRunning && !isDualAxisZ)
		{ // A-direction
			handleAxis(a, Stepper::A);
		}

		// X-Direction
		handleAxis(x, Stepper::X);

		// Y-direction
		handleAxis(y, Stepper::Y);
	}
}
