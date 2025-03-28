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
#ifdef CAN_CONTROLLER
#include "../can/can_controller.h"
#endif

namespace FocusMotor
{

	MotorData a_dat;
	MotorData x_dat;
	MotorData y_dat;
	MotorData z_dat;
	MotorData b_dat; 
	MotorData c_dat; 
	MotorData d_dat;
	MotorData e_dat;
	MotorData f_dat;
	MotorData g_dat;

	MotorData *data[MOTOR_AXIS_COUNT]; // TODO!!! 

	Preferences preferences;
	int logcount;
	bool power_enable = false;
	bool isDualAxisZ = false;
	bool waitForFirstRun[] = {false, false, false, false};

	xSemaphoreHandle xMutex = xSemaphoreCreateMutex();

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
			MotorData *mData[MOTOR_AXIS_COUNT];
			return mData;
		}
	}

	void setData(int axis, MotorData *mData)
	{
		memcpy(data[axis], mData, sizeof(MotorData));
		// getData()[axis] = mData;
	}

	void startStepper(int axis, bool reduced = false)
	{
		if (1)//xSemaphoreTake(xMutex, portMAX_DELAY))
		{
#if defined(I2C_MASTER) && defined(I2C_MOTOR)
			// Request data from the slave but only if inside i2cAddresses
			getData()[axis]->isStop = false;
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
#elif defined(CAN_CONTROLLER) && !defined(CAN_SLAVE_MOTOR) // sending from master to slave
			// send the motor data to the slave
				// we need to wait for the response from the slave to be sure that the motor is running (e.g. motor needs to run before checking if it is stopped)
				MotorData *m = getData()[axis];
				int err = can_controller::startStepper(m, axis, reduced);
				if (err != 0)
				{
					// we need to return the position right away
					getData()[axis]->stopped = true; // stop immediately, so that the return of serial gives the current position
					sendMotorPos(axis, 0);			 // this is an exception. We first get the position, then the success
				}

#elif defined USE_FASTACCEL
			FAccelStep::startFastAccelStepper(axis);
#elif defined USE_ACCELSTEP
			AccelStep::startAccelStepper(axis);
#endif
			getData()[axis]->stopped = false;
		}
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

	void moveMotor(int32_t pos, int s, bool isRelative)
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
		MotorState mMotorState = i2c_master::pullMotorDataReducedDriver(axis);
		data[axis]->currentPosition = mMotorState.currentPosition;
		// data[axis]->isforever = mMotorState.isforever;
#elif defined CAN_CONTROLLER
		// FIXME: nothing to do here since the position is assigned externally?
#endif
	}

	void setup_data()
	{

		data[Stepper::A] = &a_dat;
		data[Stepper::X] = &x_dat;
		data[Stepper::Y] = &y_dat;
		data[Stepper::Z] = &z_dat;
		#if MOTOR_AXIS_COUNT > 4
		data[Stepper::B] = &b_dat;
		data[Stepper::C] = &c_dat;
		data[Stepper::D] = &d_dat;
		data[Stepper::E] = &e_dat;
		data[Stepper::F] = &f_dat;
		data[Stepper::G] = &g_dat;
		#endif 

		if (data[Stepper::A] == nullptr)
			log_e("Stepper A data NULL");
		if (data[Stepper::X] == nullptr)
			log_e("Stepper X data NULL");
		if (data[Stepper::Y] == nullptr)
			log_e("Stepper Y data NULL");
		if (data[Stepper::Z] == nullptr)
			log_e("Stepper Z data NULL");
		if (data[Stepper::B] == nullptr)
			log_e("Stepper B data NULL");
		if (data[Stepper::C] == nullptr)
			log_e("Stepper C data NULL");
		if (data[Stepper::D] == nullptr)
			log_e("Stepper D data NULL");
		if (data[Stepper::E] == nullptr)
			log_e("Stepper E data NULL");
		if (data[Stepper::F] == nullptr)
			log_e("Stepper F data NULL");
		if (data[Stepper::G] == nullptr)
			log_e("Stepper G data NULL");


		// Read dual axis from preferences if available
		const char *prefNamespace = "UC2";
		preferences.begin(prefNamespace, false);
		isDualAxisZ = preferences.getBool("dualAxZ", pinConfig.isDualAxisZ);
		preferences.end();
	}

	void fill_data()
	{
		// setup motor pins
		preferences.begin("motpos", false);
		if (pinConfig.MOTOR_A_STEP >= 0)
		{
			data[Stepper::A]->dirPin = pinConfig.MOTOR_A_DIR;
			data[Stepper::A]->stpPin = pinConfig.MOTOR_A_STEP;
			data[Stepper::A]->currentPosition = preferences.getInt(("motor" + String(Stepper::A)).c_str());
			log_i("Motor A position: %i", data[Stepper::A]->currentPosition);
		}
		if (pinConfig.MOTOR_X_STEP >= 0)
		{
			data[Stepper::X]->dirPin = pinConfig.MOTOR_X_DIR;
			data[Stepper::X]->stpPin = pinConfig.MOTOR_X_STEP;
			data[Stepper::X]->currentPosition = preferences.getInt(("motor" + String(Stepper::X)).c_str());
			log_i("Motor X position: %i", data[Stepper::X]->currentPosition);
		}
		if (pinConfig.MOTOR_Y_STEP >= 0)
		{
			data[Stepper::Y]->dirPin = pinConfig.MOTOR_Y_DIR;
			data[Stepper::Y]->stpPin = pinConfig.MOTOR_Y_STEP;
			data[Stepper::Y]->currentPosition = preferences.getInt(("motor" + String(Stepper::Y)).c_str());
			log_i("Motor Y position: %i", data[Stepper::Y]->currentPosition);
		}
		if (pinConfig.MOTOR_Z_STEP >= 0)
		{
			data[Stepper::Z]->dirPin = pinConfig.MOTOR_Z_DIR;
			data[Stepper::Z]->stpPin = pinConfig.MOTOR_Z_STEP;
			data[Stepper::Z]->currentPosition = preferences.getInt(("motor" + String(Stepper::Z)).c_str());
			log_i("Motor Z position: %i", data[Stepper::Z]->currentPosition);
		}
		if (pinConfig.MOTOR_B_STEP >= 0)
		{
			data[Stepper::B]->dirPin = pinConfig.MOTOR_B_DIR;
			data[Stepper::B]->stpPin = pinConfig.MOTOR_B_STEP;
			data[Stepper::B]->currentPosition = preferences.getInt(("motor" + String(Stepper::B)).c_str());
			log_i("Motor B position: %i", data[Stepper::B]->currentPosition);
		}
		if (pinConfig.MOTOR_C_STEP >= 0)
		{
			data[Stepper::C]->dirPin = pinConfig.MOTOR_C_DIR;
			data[Stepper::C]->stpPin = pinConfig.MOTOR_C_STEP;
			data[Stepper::C]->currentPosition = preferences.getInt(("motor" + String(Stepper::C)).c_str());
			log_i("Motor C position: %i", data[Stepper::C]->currentPosition);
		}
		if (pinConfig.MOTOR_D_STEP >= 0)
		{
			data[Stepper::D]->dirPin = pinConfig.MOTOR_D_DIR;
			data[Stepper::D]->stpPin = pinConfig.MOTOR_D_STEP;
			data[Stepper::D]->currentPosition = preferences.getInt(("motor" + String(Stepper::D)).c_str());
			log_i("Motor D position: %i", data[Stepper::D]->currentPosition);
		}
		if (pinConfig.MOTOR_E_STEP >= 0)
		{
			data[Stepper::E]->dirPin = pinConfig.MOTOR_E_DIR;
			data[Stepper::E]->stpPin = pinConfig.MOTOR_E_STEP;
			data[Stepper::E]->currentPosition = preferences.getInt(("motor" + String(Stepper::E)).c_str());
			log_i("Motor E position: %i", data[Stepper::E]->currentPosition);
		}
		if (pinConfig.MOTOR_F_STEP >= 0)
		{
			data[Stepper::F]->dirPin = pinConfig.MOTOR_F_DIR;
			data[Stepper::F]->stpPin = pinConfig.MOTOR_F_STEP;
			data[Stepper::F]->currentPosition = preferences.getInt(("motor" + String(Stepper::F)).c_str());
			log_i("Motor F position: %i", data[Stepper::F]->currentPosition);
		}
		if (pinConfig.MOTOR_G_STEP >= 0)
		{
			data[Stepper::G]->dirPin = pinConfig.MOTOR_G_DIR;
			data[Stepper::G]->stpPin = pinConfig.MOTOR_G_STEP;
			data[Stepper::G]->currentPosition = preferences.getInt(("motor" + String(Stepper::G)).c_str());
			log_i("Motor G position: %i", data[Stepper::G]->currentPosition);
		}
		preferences.end();

		// setup trigger pins
		if (pinConfig.DIGITAL_OUT_1 > 0)
			data[Stepper::X]->triggerPin = 1; // pixel^
		if (pinConfig.DIGITAL_OUT_2 > 0)
			data[Stepper::Y]->triggerPin = 2; // line^
		if (pinConfig.DIGITAL_OUT_3 > 0)
			data[Stepper::Z]->triggerPin = 3; // frame^
	}

#ifdef USE_TCA9535
	void testTca()
	{

		for (int iMotor = 0; iMotor < MOTOR_AXIS_COUNT; iMotor++)
		{
			// need to activate the motor's dir pin eventually
			// This also updates the dial's positions
			// only test those motors that are activated
			if (data[iMotor]->isActivated)
			{
				// need to activate the motor's dir pin eventually
				// This also updates the dial's positions
				// only test those motors that are activated
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
		}
	}
#else
	void sendMotorPosition()
	{
		// send motor positions
		for (int iMotor = 0; iMotor < MOTOR_AXIS_COUNT; iMotor++)
		{
			if (data[iMotor]->isActivated)
			{
				sendMotorPos(iMotor, 0);
			}
		}
	}
#endif // TCA9535

#ifdef I2C_MASTER
	void setup_i2c_motor()
	{
#ifdef I2C_MOTOR
		// send stop signal to all motors and update motor positions
		for (int iMotor = 0; iMotor < MOTOR_AXIS_COUNT; iMotor++)
		{
			moveMotor(1, iMotor, true); // wake up motor
			data[iMotor]->isActivated = true;
			stopStepper(iMotor);
			MotorState mMotorState = i2c_master::pullMotorDataReducedDriver(iMotor);
			data[iMotor]->currentPosition = mMotorState.currentPosition;
		}
#endif
#if defined DIAL_CONTROLLER
		// send motor positions to dial
		i2c_master::pushMotorPosToDial();
#endif
	}
#endif
	void setup()
	{
		// Common setup for all
		setup_data();
		fill_data();


#ifdef I2C_MASTER
		setup_i2c_motor();
#endif

#if (defined(CAN_CONTROLLER) && !defined(CAN_SLAVE_MOTOR))
// stop all motors on startup
		for (int i = 0; i < MOTOR_AXIS_COUNT; i++)
		{
			stopStepper(i);delay(10);
			stopStepper(i);// do it twice - wrong state on slave/master side? Weird!! //FIXME: why?
		}
#endif

#ifdef CAN_SLAVE_MOTOR
	// send current position to master
	can_controller::sendMotorStateToMaster();
#endif 

#ifdef USE_FASTACCEL
#ifdef USE_TCA9535
		log_i("Setting external pin for FastAccelStepper");
		FAccelStep::setExternalCallForPin(tca_controller::setExternalPin);
#endif
		FAccelStep::setupFastAccelStepper();
#ifdef USE_TCA9535
		testTca();
#else
		sendMotorPosition();
#endif
#endif

#ifdef USE_ACCELSTEP
#ifdef USE_TCA9535
		AccelStep::setExternalCallForPin(tca_controller::setExternalPin);
#endif
		AccelStep::setupAccelStepper();
#ifdef USE_TCA9535
		testTca();
#else
		sendMotorPosition();
#endif
#endif

	}

	void setSoftLimits(int axis, int32_t minPos, int32_t maxPos)
    {
#ifdef USE_FASTACCEL
        FAccelStep::setSoftLimits(axis, minPos, maxPos);
        log_i("Set soft limits on axis %d: min=%ld, max=%ld", axis, (long)minPos, (long)maxPos);
#endif
    }



	void loop()
	{
#if (!defined(CAN_CONTROLLER) || defined(CAN_SLAVE_MOTOR)) // if we are the master, we don't check this in the loop as the slave will push it asynchronously
		// checks if a stepper is still running
		for (int i = 0; i < MOTOR_AXIS_COUNT; i++)
		{
#ifdef I2C_MASTER
			// seems like the i2c needs a moment to start the motor (i.e. act is async and loop is continously running, maybe faster than the motor can start)
			if (waitForFirstRun[i])
			{
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
				preferences.putInt(("motor" + String(i)).c_str(), data[i]->currentPosition);
				preferences.end();
			}
		}
#endif
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
#elif defined CAN_CONTROLLER
		// Slave will push this information to the master via CAN asynchrously
		mIsRunning = can_controller::isMotorRunning(i);
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
#ifdef defined I2C_MASTER &&defined DIAL_CONTROLLER
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
#ifdef CAN_SLAVE_MOTOR
		// We push the current state to the master to inform it that we are running and about the current position
		can_controller::sendMotorStateToMaster();
#endif
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

		log_i("stopStepper Focus Motor %i, stopped: %i", i, data[i]->stopped);
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
#elif defined CAN_CONTROLLER && !defined CAN_SLAVE_MOTOR
		getData()[i]->isforever = false;
		getData()[i]->speed = 0;
		getData()[i]->stopped = true;
		getData()[i]->isStop = true;
		Stepper s = static_cast<Stepper>(i);
		can_controller::stopStepper(s);
#endif
	}

	uint32_t getPosition(Stepper s)
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
}
