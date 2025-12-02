#include <PinConfig.h>
#include "../../config.h"
#include "FocusMotor.h"
#include "MotorEncoderConfig.h"
#include "Wire.h"
#include "../wifi/WifiController.h"
#include "../../cJsonTool.h"
#include "../state/State.h"
#include "../serial/SerialProcess.h"
#include "esp_debug_helpers.h"
#ifdef LINEAR_ENCODER_CONTROLLER
#include "../encoder/LinearEncoderController.h"
#endif
#ifdef ENCODER_CONTROLLER
#include "../encoder/PCNTEncoderController.h"
#endif
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
#ifdef CAN_BUS_ENABLED
#include "../can/can_controller.h"
#endif
#ifdef DIGITAL_IN_CONTROLLER
#include "../digitalin/DigitalInController.h"
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


	xSemaphoreHandle xMutex = NULL;
	xSemaphoreHandle xSerialMutex = NULL;  // Mutex for serial JSON output

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



	// Helper function to determine if an axis should use CAN in hybrid mode
	bool shouldUseCANForAxis(int axis)
	{
#if defined(CAN_BUS_ENABLED) && defined(CAN_SEND_COMMANDS)
		// In hybrid mode: axes >= threshold use CAN, axes < threshold use native drivers
		// Check if this axis has a native driver configured
		bool hasNativeDriver = false;
		switch(axis) {
			case Stepper::A: hasNativeDriver = (pinConfig.MOTOR_A_STEP > 0); break;
			case Stepper::X: hasNativeDriver = (pinConfig.MOTOR_X_STEP > 0); break;
			case Stepper::Y: hasNativeDriver = (pinConfig.MOTOR_Y_STEP > 0); break;
			case Stepper::Z: hasNativeDriver = (pinConfig.MOTOR_Z_STEP > 0); break;
			case Stepper::B: hasNativeDriver = (pinConfig.MOTOR_B_STEP > 0); break;
			case Stepper::C: hasNativeDriver = (pinConfig.MOTOR_C_STEP > 0); break;
			case Stepper::D: hasNativeDriver = (pinConfig.MOTOR_D_STEP > 0); break;
			case Stepper::E: hasNativeDriver = (pinConfig.MOTOR_E_STEP > 0); break;
			case Stepper::F: hasNativeDriver = (pinConfig.MOTOR_F_STEP > 0); break;
			case Stepper::G: hasNativeDriver = (pinConfig.MOTOR_G_STEP > 0); break;
			default: hasNativeDriver = false; break;
		}
		
		// If axis >= hybrid threshold AND no native driver, use CAN
		// If axis < hybrid threshold AND has native driver, use native
		// If axis >= hybrid threshold but has native driver, use native (hardware override)
		if (hasNativeDriver) {
			return false; // Use native driver
		}
		// No native driver - use CAN if axis >= threshold
		return (axis >= pinConfig.HYBRID_MOTOR_CAN_THRESHOLD);
#else
		return false; // CAN not available or this is a slave
#endif
	}

	void startStepper(int axis, int reduced = 0)
	{
		/*
		reduced organizes the ammount of data that is being transmitted (E.g. via CAN Bus)
		reduced: 0 => All data transfered MotorData
		reduced: 1 => MotorDataReduced is transfered
		reducsed: 2 => single Value udpates  
		*/
		
		// Check if hard limit is triggered - don't start motor until homing clears it
		if (getData()[axis]->hardLimitTriggered)
		{
			log_w("Cannot start motor on axis %d - hard limit triggered! Homing required.", axis);
			getData()[axis]->stopped = true;
			sendMotorPos(axis, 0, -3); // Send with special qid to indicate error state
			return;
		}
		
		if (xSemaphoreTake(xMutex, portMAX_DELAY)) // TODO: Check if this is causing the issue with the queue
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
#elif defined(CAN_BUS_ENABLED) && defined(CAN_SEND_COMMANDS) && !defined(CAN_RECEIVE_MOTOR)
			// HYBRID MODE SUPPORT: Check if this axis should use CAN or native driver
			if (shouldUseCANForAxis(axis))
			{
				// Route to CAN satellite
				log_i("Hybrid mode: Routing axis %d to CAN at address %d", axis, can_controller::axis2id(axis-4));
				MotorData *m = getData()[axis-4];
				int err = can_controller::startStepper(m, axis-4, reduced);
				if (err != 0)
				{
					getData()[axis]->stopped = true;
					sendMotorPos(axis, 0);
				}
			}
			else
			{
				// Use native driver (FastAccelStepper or AccelStepper)
				log_i("Hybrid mode: Routing axis %d to native driver", axis);
#if defined(USE_FASTACCEL)
				waitForFirstRun[axis] = 1;
				FAccelStep::startFastAccelStepper(axis);
#elif defined(USE_ACCELSTEP)
				AccelStep::startAccelStepper(axis);
#else
				log_w("No native motor driver available for axis %d in hybrid mode", axis);
				getData()[axis]->stopped = true;
				sendMotorPos(axis, 0);
#endif
			}
#elif defined(CAN_BUS_ENABLED) && !defined(CAN_RECEIVE_MOTOR)
			// Pure CAN master mode (non-hybrid) - all motors via CAN
			MotorData *m = getData()[axis];
			int err = can_controller::startStepper(m, axis, reduced);
			if (err != 0)
			{
				getData()[axis]->stopped = true;
				sendMotorPos(axis, 0);
			}

#elif defined USE_FASTACCEL
			waitForFirstRun[axis] = 1; // TODO: This is probably a weird workaround to skip the first check in the loop() if the motor is actually running - otherwise It'll stop immediately
			FAccelStep::startFastAccelStepper(axis);
#elif defined USE_ACCELSTEP
			AccelStep::startAccelStepper(axis);
#endif
			getData()[axis]->stopped = false;
			xSemaphoreGive(xMutex);
		}
	}

	void toggleStepper(Stepper s, bool isStop, int reduced)
	{
		/*
		reduced:
			0 => Full MotorDAta
			1 => MotorDataReduced
			2 => Single Value Updates
		*/
		if (isStop)
		{
			stopStepper(s);
		}
		else
		{
			// Check if encoder-based motion is enabled for this stepper
			if (getData()[s]->encoderBasedMotion) {
				log_i("Starting encoder-based precise motion for stepper %d", s);
				#ifdef LINEAR_ENCODER_CONTROLLER
				// Use LinearEncoderController for precise motion
				startEncoderBasedMotion(s); 
				#else
				log_w("Encoder-based motion requested but LINEAR_ENCODER_CONTROLLER not defined");
				startStepper(s, reduced);
				#endif
			} else {
				startStepper(s, reduced); // TODO: Need dual axis?
			}
		}
	}

	void moveMotor(int32_t pos, int32_t speed, int s, bool isRelative)
	{
		// move motor
		// blocking = true => wait until motor is done
		// blocking = false => fire and forget
		data[s]->targetPosition = pos;
		data[s]->isforever = false;
		data[s]->absolutePosition = !isRelative;
		data[s]->isStop = false;
		data[s]->stopped = false;
		data[s]->speed = speed;
		data[s]->acceleration = 1000;
		data[s]->isaccelerated = 1;
		log_i("Moving motor %i to position %i with speed %i, isRelative: %i", s, pos, speed, isRelative);
		startStepper(s, 1); // reduced = 1 means that we only send the motor data reduced to the slave, so that it can handle it faster
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
#if defined(CAN_BUS_ENABLED) && defined(CAN_SEND_COMMANDS) && !defined(CAN_RECEIVE_MOTOR)
		// HYBRID MODE SUPPORT: Check if this axis uses CAN or native driver
		if (shouldUseCANForAxis(axis))
		{
			// CAN axes: position is assigned externally via CAN messages
			// Nothing to do here as the slave pushes updates
		}
		else
		{
			// Native axes: update from local driver
#if defined(USE_FASTACCEL)
			FAccelStep::updateData(axis);
#elif defined(USE_ACCELSTEP)
			AccelStep::updateData(axis);
#endif
		}
#elif defined USE_FASTACCEL
		FAccelStep::updateData(axis);
#elif defined USE_ACCELSTEP
		AccelStep::updateData(axis);
#elif defined I2C_MASTER
		MotorState mMotorState = i2c_master::pullMotorDataReducedDriver(axis);
		data[axis]->currentPosition = mMotorState.currentPosition;
		// data[axis]->isforever = mMotorState.isforever;
#elif defined CAN_BUS_ENABLED
		// FIXME: nothing to do here since the position is assigned externally?
#endif
	}

	long getCurrentMotorPosition(int axis)
	{
		// Get real-time motor position directly from stepper library
#if defined(CAN_BUS_ENABLED) && defined(CAN_SEND_COMMANDS) && !defined(CAN_RECEIVE_MOTOR)
		// HYBRID MODE SUPPORT: Check if this axis uses CAN or native driver
		if (shouldUseCANForAxis(axis))
		{
			// CAN axes: use cached position
			return data[axis]->currentPosition;
		}
		else
		{
			// Native axes: get from local driver
#if defined(USE_FASTACCEL)
			return FAccelStep::getCurrentPosition(static_cast<Stepper>(axis));
#elif defined(USE_ACCELSTEP)
			return data[axis]->currentPosition;
#else
			return data[axis]->currentPosition;
#endif
		}
#elif defined USE_FASTACCEL
		return FAccelStep::getCurrentPosition(static_cast<Stepper>(axis));
#elif defined USE_ACCELSTEP
		// For AccelStep, we need to rely on cached position
		return data[axis]->currentPosition;
#elif defined I2C_MASTER
		// For I2C, we need to rely on cached position
		return data[axis]->currentPosition;
#elif defined CAN_BUS_ENABLED
		// For CAN, we need to rely on cached position
		return data[axis]->currentPosition;
#else
		return data[axis]->currentPosition;
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
		preferences.begin("UC2", false);
		if (pinConfig.MOTOR_A_STEP >= 0)
		{
		data[Stepper::A]->dirPin = pinConfig.MOTOR_A_DIR;
		data[Stepper::A]->stpPin = pinConfig.MOTOR_A_STEP;
		data[Stepper::A]->currentPosition = preferences.getInt(("motor" + String(Stepper::A)).c_str());
		data[Stepper::A]->minPos = preferences.getInt(("min" + String(Stepper::A)).c_str());
		data[Stepper::A]->maxPos = preferences.getInt(("max" + String(Stepper::A)).c_str());
		data[Stepper::A]->softLimitEnabled = preferences.getBool(("isen" + String(Stepper::A)).c_str(),false);
		data[Stepper::A]->directionPinInverted = preferences.getInt("motainvert", false);
		data[Stepper::A]->joystickDirectionInverted = preferences.getBool(("joyDir" + String(Stepper::A)).c_str(), false);
		data[Stepper::A]->hardLimitEnabled = preferences.getBool(("hlEn" + String(Stepper::A)).c_str(), true); // Enabled by default
		data[Stepper::A]->hardLimitPolarity = preferences.getBool(("hlPol" + String(Stepper::A)).c_str(), false); // NO by default
		isActivated[Stepper::A] = true;
			log_i("Motor A position: %i", data[Stepper::A]->currentPosition);
		}
		if (pinConfig.MOTOR_X_STEP >= 0)
		{
			data[Stepper::X]->dirPin = pinConfig.MOTOR_X_DIR;
			data[Stepper::X]->stpPin = pinConfig.MOTOR_X_STEP;
			data[Stepper::X]->currentPosition = preferences.getInt(("motor" + String(Stepper::X)).c_str());
			data[Stepper::X]->minPos = preferences.getInt(("min" + String(Stepper::X)).c_str());
		data[Stepper::X]->maxPos = preferences.getInt(("max" + String(Stepper::X)).c_str());
		data[Stepper::X]->softLimitEnabled = preferences.getBool(("isen" + String(Stepper::X)).c_str(),false);
		data[Stepper::X]->directionPinInverted = preferences.getInt("motxinv", false);
		data[Stepper::X]->joystickDirectionInverted = preferences.getBool(("joyDir" + String(Stepper::X)).c_str(), false);
		data[Stepper::X]->hardLimitEnabled = preferences.getBool(("hlEn" + String(Stepper::X)).c_str(), true); // Enabled by default
		data[Stepper::X]->hardLimitPolarity = preferences.getBool(("hlPol" + String(Stepper::X)).c_str(), false); // NO by default
		isActivated[Stepper::X] = true;
			log_i("Motor X position: %i", data[Stepper::X]->currentPosition);
		}
		if (pinConfig.MOTOR_Y_STEP >= 0)
		{
			data[Stepper::Y]->dirPin = pinConfig.MOTOR_Y_DIR;
			data[Stepper::Y]->stpPin = pinConfig.MOTOR_Y_STEP;
			data[Stepper::Y]->currentPosition = preferences.getInt(("motor" + String(Stepper::Y)).c_str());
			data[Stepper::Y]->minPos = preferences.getInt(("min" + String(Stepper::Y)).c_str());
		data[Stepper::Y]->maxPos = preferences.getInt(("max" + String(Stepper::Y)).c_str());
		data[Stepper::Y]->softLimitEnabled = preferences.getBool(("isen" + String(Stepper::Y)).c_str(),false);
		data[Stepper::Y]->directionPinInverted = preferences.getInt("motyinv", false);
		data[Stepper::Y]->joystickDirectionInverted = preferences.getBool(("joyDir" + String(Stepper::Y)).c_str(), false);
		data[Stepper::Y]->hardLimitEnabled = preferences.getBool(("hlEn" + String(Stepper::Y)).c_str(), true); // Enabled by default
		data[Stepper::Y]->hardLimitPolarity = preferences.getBool(("hlPol" + String(Stepper::Y)).c_str(), false); // NO by default
		isActivated[Stepper::Y] = true;
			log_i("Motor Y position: %i", data[Stepper::Y]->currentPosition);
		}
		if (pinConfig.MOTOR_Z_STEP >= 0)
		{
			data[Stepper::Z]->dirPin = pinConfig.MOTOR_Z_DIR;
			data[Stepper::Z]->stpPin = pinConfig.MOTOR_Z_STEP;
			data[Stepper::Z]->currentPosition = preferences.getInt(("motor" + String(Stepper::Z)).c_str());
			data[Stepper::Z]->minPos = preferences.getInt(("min" + String(Stepper::Z)).c_str());
		data[Stepper::Z]->maxPos = preferences.getInt(("max" + String(Stepper::Z)).c_str());
		data[Stepper::Z]->softLimitEnabled = preferences.getBool(("isen" + String(Stepper::Z)).c_str(),false);
		data[Stepper::Z]->directionPinInverted = preferences.getInt("motzinv", false);
		data[Stepper::Z]->joystickDirectionInverted = preferences.getBool(("joyDir" + String(Stepper::Z)).c_str(), false);
		data[Stepper::Z]->hardLimitEnabled = preferences.getBool(("hlEn" + String(Stepper::Z)).c_str(), true); // Enabled by default
		data[Stepper::Z]->hardLimitPolarity = preferences.getBool(("hlPol" + String(Stepper::Z)).c_str(), false); // NO by default
		isActivated[Stepper::Z] = true;
			log_i("Motor Z position: %i", data[Stepper::Z]->currentPosition);
		}
		if (pinConfig.MOTOR_B_STEP >= 0)
		{
			data[Stepper::B]->dirPin = pinConfig.MOTOR_B_DIR;
			data[Stepper::B]->stpPin = pinConfig.MOTOR_B_STEP;
			data[Stepper::B]->currentPosition = preferences.getInt(("motor" + String(Stepper::B)).c_str());
			data[Stepper::B]->minPos = preferences.getInt(("min" + String(Stepper::B)).c_str());
			data[Stepper::B]->maxPos = preferences.getInt(("max" + String(Stepper::B)).c_str());
			data[Stepper::B]->softLimitEnabled = preferences.getBool(("isen" + String(Stepper::B)).c_str(),false);
			log_i("Motor B position: %i", data[Stepper::B]->currentPosition);
		}
		if (pinConfig.MOTOR_C_STEP >= 0)
		{
			data[Stepper::C]->dirPin = pinConfig.MOTOR_C_DIR;
			data[Stepper::C]->stpPin = pinConfig.MOTOR_C_STEP;
			data[Stepper::C]->currentPosition = preferences.getInt(("motor" + String(Stepper::C)).c_str());
			data[Stepper::C]->minPos = preferences.getInt(("min" + String(Stepper::C)).c_str());
			data[Stepper::C]->maxPos = preferences.getInt(("max" + String(Stepper::C)).c_str());
			data[Stepper::C]->softLimitEnabled = preferences.getBool(("isen" + String(Stepper::C)).c_str(),false);
			log_i("Motor C position: %i", data[Stepper::C]->currentPosition);
		}
		if (pinConfig.MOTOR_D_STEP >= 0)
		{
			data[Stepper::D]->dirPin = pinConfig.MOTOR_D_DIR;
			data[Stepper::D]->stpPin = pinConfig.MOTOR_D_STEP;
			data[Stepper::D]->currentPosition = preferences.getInt(("motor" + String(Stepper::D)).c_str());
			data[Stepper::D]->minPos = preferences.getInt(("min" + String(Stepper::D)).c_str());
			data[Stepper::D]->maxPos = preferences.getInt(("max" + String(Stepper::D)).c_str());
			data[Stepper::D]->softLimitEnabled = preferences.getBool(("isen" + String(Stepper::D)).c_str(),false);
			log_i("Motor D position: %i", data[Stepper::D]->currentPosition);
		}
		if (pinConfig.MOTOR_E_STEP >= 0)
		{
			data[Stepper::E]->dirPin = pinConfig.MOTOR_E_DIR;
			data[Stepper::E]->stpPin = pinConfig.MOTOR_E_STEP;
			data[Stepper::E]->currentPosition = preferences.getInt(("motor" + String(Stepper::E)).c_str());
			data[Stepper::E]->minPos = preferences.getInt(("min" + String(Stepper::E)).c_str());
			data[Stepper::E]->maxPos = preferences.getInt(("max" + String(Stepper::E)).c_str());
			data[Stepper::E]->softLimitEnabled = preferences.getBool(("isen" + String(Stepper::E)).c_str(),false);
			log_i("Motor E position: %i", data[Stepper::E]->currentPosition);
		}
		if (pinConfig.MOTOR_F_STEP >= 0)
		{
			data[Stepper::F]->dirPin = pinConfig.MOTOR_F_DIR;
			data[Stepper::F]->stpPin = pinConfig.MOTOR_F_STEP;
			data[Stepper::F]->currentPosition = preferences.getInt(("motor" + String(Stepper::F)).c_str());
			data[Stepper::F]->minPos = preferences.getInt(("min" + String(Stepper::F)).c_str());
			data[Stepper::F]->maxPos = preferences.getInt(("max" + String(Stepper::F)).c_str());
			data[Stepper::F]->softLimitEnabled = preferences.getBool(("isen" + String(Stepper::F)).c_str(),false);
			log_i("Motor F position: %i", data[Stepper::F]->currentPosition);
		}
		if (pinConfig.MOTOR_G_STEP >= 0)
		{
			data[Stepper::G]->dirPin = pinConfig.MOTOR_G_DIR;
			data[Stepper::G]->stpPin = pinConfig.MOTOR_G_STEP;
			data[Stepper::G]->currentPosition = preferences.getInt(("motor" + String(Stepper::G)).c_str());
			data[Stepper::G]->minPos = preferences.getInt(("min" + String(Stepper::G)).c_str());
			data[Stepper::G]->maxPos = preferences.getInt(("max" + String(Stepper::G)).c_str());
			data[Stepper::G]->softLimitEnabled = preferences.getBool(("isen" + String(Stepper::G)).c_str(),false);
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
			if (isActivated[iMotor])
			{
				// need to activate the motor's dir pin eventually
				// This also updates the dial's positions
				// only test those motors that are activated
				Stepper s = static_cast<Stepper>(iMotor);
				data[s]->absolutePosition = false;
				data[s]->targetPosition = -1;
				startStepper(iMotor, 0);
				delay(10);
				stopStepper(iMotor);
				data[s]->targetPosition = 1;
				startStepper(iMotor, 0);
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
			if (isActivated[iMotor])
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
			moveMotor(1, 1000, iMotor, true); // wake up motor
			isActivated[iMotor] = true;
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
		// Initialize mutexes FIRST, before any other operations
		if (xMutex == NULL)
			xMutex = xSemaphoreCreateMutex();
		if (xSerialMutex == NULL)
			xSerialMutex = xSemaphoreCreateMutex();
		
		// Common setup for all
		setup_data();
		fill_data();
		
		// Initialize motor-encoder conversion configuration
		MotorEncoderConfig::setup();

#ifdef I2C_MASTER
		setup_i2c_motor();
#endif

#if (defined(CAN_BUS_ENABLED) && !defined(CAN_RECEIVE_MOTOR))
		// stop all motors on startup
		for (int i = 0; i < MOTOR_AXIS_COUNT; i++)
		{
			isActivated[i] = true;
			stopStepper(i); // TODO: do it twice - wrong state on slave/master side? Weird!! //FIXME: why?
			delay(40);
			// TODO: This apparently does not suffice to wake up the motor and get the current position from CAN satellites
			// move motor by 0 to wake it up and get the current position
		}
		
		// Send motor settings (soft limits, acceleration, etc.) to all CAN slaves
		for (int i = 0; i < MOTOR_AXIS_COUNT; i++)
		{
			MotorSettings settings = can_controller::extractMotorSettings(*getData()[i]);
			can_controller::sendMotorSettingsToCANDriver(settings, i);
			delay(20); // Allow time for CAN transmission
		}
		log_i("Motor settings sent to all CAN slaves during setup");
#endif

#ifdef CAN_RECEIVE_MOTOR
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

	void setSoftLimits(int axis, int32_t minPos, int32_t maxPos, bool isEnabled)
	{
		// set soft limits
		getData()[axis]->softLimitEnabled = isEnabled;
		getData()[axis]->minPos = minPos;
		getData()[axis]->maxPos = maxPos;
		log_i("Set soft limits on axis %d: min=%ld, max=%ld", axis, (long)minPos, (long)maxPos);
	}

	void setHardLimit(int axis, bool enabled, bool polarity)
	{
		// Set hard limit configuration for a motor axis
		// enabled: if true, motor will emergency stop when endstop is hit during normal operation
		// polarity: 0 = normally open (NO), 1 = normally closed (NC)
		if (axis < 0 || axis >= MOTOR_AXIS_COUNT) return;
		
		getData()[axis]->hardLimitEnabled = enabled;
		getData()[axis]->hardLimitPolarity = polarity;
		log_i("Set hard limit on axis %d: enabled=%d, polarity=%d (0=NO, 1=NC)", axis, enabled, polarity);
		
		// Save to preferences
		preferences.begin("UC2", false);
		preferences.putBool(("hlEn" + String(axis)).c_str(), enabled);
		preferences.putBool(("hlPol" + String(axis)).c_str(), polarity);
		preferences.end();
		
#if defined(CAN_BUS_ENABLED) && !defined(CAN_RECEIVE_MOTOR)
		// Master: Notify CAN slaves about the hard limit settings
		MotorSettings settings = can_controller::extractMotorSettings(*getData()[axis]);
		can_controller::sendMotorSettingsToCANDriver(settings, axis);
#endif
	}

	void clearHardLimitTriggered(int axis)
	{
		if (axis < 0 || axis >= MOTOR_AXIS_COUNT) return;
		getData()[axis]->hardLimitTriggered = false;
		log_i("Cleared hard limit triggered flag for axis %d", axis);
	}

	void checkHardLimits()
	{
#if defined(CAN_RECEIVE_MOTOR) && defined(MOTOR_CONTROLLER) && defined(DIGITAL_IN_CONTROLLER)
		// Hard limit checks are ONLY performed on CAN slave/satellite motor controllers
		// The slave has direct access to the endstop and controls the motor directly
		// Master controllers receive motor state updates from slaves via CAN
		
		// On CAN slaves, we only have one motor (REMOTE_MOTOR_AXIS_ID) connected to DIGITAL_IN_1
		Stepper mStepper = static_cast<Stepper>(pinConfig.REMOTE_MOTOR_AXIS_ID);
		
		if (isActivated[mStepper] && getData()[mStepper]->hardLimitEnabled && 
		    !getData()[mStepper]->isHoming)// && !getData()[mStepper]->hardLimitTriggered)
		{
			bool endstopState = DigitalInController::getDigitalVal(1); // Slaves use DIGITAL_IN_1
			bool hardLimitPolarity = getData()[mStepper]->hardLimitPolarity;
			
			// Trigger if endstop state indicates limit is hit
			// For NO (polarity=0): trigger when endstopState=1 (pressed)
			// For NC (polarity=1): trigger when endstopState=0 (opened)
			if (endstopState != hardLimitPolarity && isRunning(mStepper))
			{
				if (!getData()[mStepper]->hardLimitTriggered){
					log_e("HARD LIMIT TRIGGERED on slave motor! Endstop state: %d, Polarity: %d", endstopState, hardLimitPolarity);
					setPosition(mStepper, 999999); // Set to special value to indicate error state
					sendMotorPos(mStepper, 0, -3); // Send with special qid to indicate error
				}
				stopStepper(mStepper);
				getData()[mStepper]->hardLimitTriggered = true;
			}
		}
#elif defined(MOTOR_CONTROLLER) && defined(DIGITAL_IN_CONTROLLER) && !defined(CAN_BUS_ENABLED)
		// Non-CAN configurations (local motor drivers with direct endstop access)
		// Check hard limits for X, Y, Z axes (digital inputs 1, 2, 3)
		// Only check if motor is running and not in homing mode
		
		// Axis X uses digital input 1
		if (isActivated[Stepper::X] && getData()[Stepper::X]->hardLimitEnabled && 
		    !getData()[Stepper::X]->isHoming && !getData()[Stepper::X]->hardLimitTriggered)
		{
			bool endstopState = DigitalInController::getDigitalVal(1);
			bool hardLimitPolarity = getData()[Stepper::X]->hardLimitPolarity;
			
			// Trigger if endstop state matches the polarity
			// For NO (polarity=0): trigger when endstopState=1 (pressed)
			// For NC (polarity=1): trigger when endstopState=0 (opened)
			if (endstopState != hardLimitPolarity && isRunning(Stepper::X))
			{
				log_e("HARD LIMIT TRIGGERED on axis X! Endstop state: %d, Polarity: %d", endstopState, hardLimitPolarity);
				stopStepper(Stepper::X);
				getData()[Stepper::X]->hardLimitTriggered = true;
				setPosition(Stepper::X, 999999); // Set to special value to indicate error state
				sendMotorPos(Stepper::X, 0, -3); // Send with special qid to indicate error
			}
		}
		
		// Axis Y uses digital input 2
		if (isActivated[Stepper::Y] && getData()[Stepper::Y]->hardLimitEnabled && 
		    !getData()[Stepper::Y]->isHoming && !getData()[Stepper::Y]->hardLimitTriggered)
		{
			bool endstopState = DigitalInController::getDigitalVal(2);
			bool hardLimitPolarity = getData()[Stepper::Y]->hardLimitPolarity;
			
			if (endstopState != hardLimitPolarity && isRunning(Stepper::Y))
			{
				log_e("HARD LIMIT TRIGGERED on axis Y! Endstop state: %d, Polarity: %d", endstopState, hardLimitPolarity);
				stopStepper(Stepper::Y);
				getData()[Stepper::Y]->hardLimitTriggered = true;
				setPosition(Stepper::Y, 999999);
				sendMotorPos(Stepper::Y, 0, -3);
			}
		}
		
		// Axis Z uses digital input 3
		if (isActivated[Stepper::Z] && getData()[Stepper::Z]->hardLimitEnabled && 
		    !getData()[Stepper::Z]->isHoming && !getData()[Stepper::Z]->hardLimitTriggered)
		{
			bool endstopState = DigitalInController::getDigitalVal(3);
			bool hardLimitPolarity = getData()[Stepper::Z]->hardLimitPolarity;
			
			if (endstopState != hardLimitPolarity && isRunning(Stepper::Z))
			{
				log_e("HARD LIMIT TRIGGERED on axis Z! Endstop state: %d, Polarity: %d", endstopState, hardLimitPolarity);
				stopStepper(Stepper::Z);
				getData()[Stepper::Z]->hardLimitTriggered = true;
				setPosition(Stepper::Z, 999999);
				sendMotorPos(Stepper::Z, 0, -3);
				
				// If dual axis Z is enabled, also stop A
				if (isDualAxisZ)
				{
					stopStepper(Stepper::A);
					getData()[Stepper::A]->hardLimitTriggered = true;
					setPosition(Stepper::A, 999999);
					sendMotorPos(Stepper::A, 0, -3);
				}
			}
		}
#endif
		// CAN_BUS_ENABLED masters don't check hard limits locally - slaves handle this
		// and report back via CAN when a hard limit is triggered
	}

	bool isEncoderBasedMotionEnabled(int axis)
	{
		if (axis < 0 || axis >= MOTOR_AXIS_COUNT) return false;
		return getData()[axis]->encoderBasedMotion;
	}

	void setEncoderBasedMotion(int axis, bool enabled)
	{
		if (axis < 0 || axis >= MOTOR_AXIS_COUNT) return;
		getData()[axis]->encoderBasedMotion = enabled;
		log_i("Encoder-based motion for axis %d: %s", axis, enabled ? "enabled" : "disabled");
		
#ifdef CAN_BUS_ENABLED
		// Notify CAN slaves if we are a CAN master
		can_controller::sendEncoderBasedMotionToCanDriver(axis, enabled);
#endif
	}

	void startEncoderBasedMotion(int axis)
	{
		#ifdef LINEAR_ENCODER_CONTROLLER
		// Create a JSON object to call LinearEncoderController moveP function
		// Convert motor parameters to encoder-based motion parameters
		MotorData* motorData = getData()[axis];
		
		// Use global conversion factor for step-to-encoder units
		float conversionFactor = MotorEncoderConfig::getStepsToEncoderUnits();
		
		// Convert step position to encoder units (micrometers)
		float encoderPosition = motorData->targetPosition * conversionFactor;
		// Convert step speed to encoder units  
		float encoderSpeed = abs(motorData->speed) * conversionFactor;
		
		// Create JSON for LinearEncoderController moveP command
		// Structure: {"task": "/linearencoder_act", "moveP": {"steppers": [...]}}
		cJSON* movePreciseJson = cJSON_CreateObject();
		cJSON* moveP = cJSON_CreateObject();
		cJSON* steppers = cJSON_CreateArray();
		cJSON* stepper = cJSON_CreateObject();
		
		// Add task identifier
		cJSON_AddStringToObject(movePreciseJson, "task", "/linearencoder_act");
		
		// Build stepper object
		cJSON_AddNumberToObject(stepper, "stepperid", axis);
		cJSON_AddNumberToObject(stepper, "position", (int)encoderPosition);
		cJSON_AddNumberToObject(stepper, "isabs", motorData->absolutePosition ? 1 : 0);
		cJSON_AddNumberToObject(stepper, "speed", (int)encoderSpeed);
		
		// Set default PID values if not already configured
		cJSON_AddNumberToObject(stepper, "cp", 20.0);  // Proportional gain // TODO: Maybe we should store / read these values from preferences and make them setable via act()
		cJSON_AddNumberToObject(stepper, "ci", 1.0);   // Integral gain  
		cJSON_AddNumberToObject(stepper, "cd", 5.0);   // Derivative gain
		
		// Nest properly: steppers array -> moveP object -> root object
		cJSON_AddItemToArray(steppers, stepper);
		cJSON_AddItemToObject(moveP, "steppers", steppers);
		cJSON_AddItemToObject(movePreciseJson, "moveP", moveP);
		
		log_i("Starting encoder-based motion for axis %d: step_pos=%ld -> encoder_pos=%f µm (factor=%f)", 
		      axis, (long)motorData->targetPosition, encoderPosition, conversionFactor);
		log_i("Motor speed: step_speed=%ld -> encoder_speed=%f µm/s", 
		      (long)motorData->speed, encoderSpeed);
		
		// Start encoder accuracy tracking for this move
		#ifdef ENCODER_CONTROLLER
		PCNTEncoderController::startEncoderTracking(axis, motorData->targetPosition);
		#endif
		
			  // print the json for debugging
			  char* jsonString = cJSON_Print(movePreciseJson);
			  if (jsonString) {
				  log_d("Encoder-based motion JSON: %s", jsonString);
				  cJSON_free(jsonString);
				}
				// Clean up JSON object
		// Call LinearEncoderController act function with moveP command
		LinearEncoderController::act(movePreciseJson);
				cJSON_Delete(movePreciseJson);
		#else
		log_w("LINEAR_ENCODER_CONTROLLER not available for encoder-based motion");
		#endif
	}

    static unsigned long lastSendTime = 0; // holds the last time positions were sent
    const unsigned long interval = 2000;   // 2-second interval

	void loop()
	{
		// Check hard limits ONCE per loop iteration (not per motor)
		// Hard limits are only checked on slaves or non-CAN configurations
		#if (!defined(CAN_BUS_ENABLED) || defined(CAN_RECEIVE_MOTOR))
		checkHardLimits();
		#endif
		
		for (int i = 0; i < MOTOR_AXIS_COUNT; i++)
		{
			#if (!defined(CAN_BUS_ENABLED) || defined(CAN_RECEIVE_MOTOR)) // if we are the master, we don't check this in the loop as the slave will push it asynchronously
			// checks if a stepper is still running
			// seems like the i2c needs a moment to start the motor (i.e. act is async and loop is continously running, maybe faster than the motor can start)
			if (waitForFirstRun[i])
			{
				waitForFirstRun[i] = 0;
				continue;
			}
			// If soft limits are enabled, decide whether to stop
			if (getData()[i]->softLimitEnabled)
			{
				log_i("Soft limits enabled for motor %d", i);
				int32_t pos = getData()[i]->currentPosition;
				int32_t minPos = getData()[i]->minPos;
				int32_t maxPos = getData()[i]->maxPos;
				int32_t spd = getData()[i]->speed; // can be negative or positive

				// If below minPos
				if (pos < minPos)
				{
					// Only stop if continuing further negative
					// i.e. speed < 0 => going more negative
					if (spd < 0)
					{
						log_i("Motor %d outside min limit & moving further negative => STOP", i);
						stopStepper(i);
					}
					// else if speed > 0 => let it keep running to move back inside the range
				}

				

				// If above maxPos
				else if (pos > maxPos)
				{
					// Only stop if continuing further positive
					// i.e. speed > 0 => going more positive
					if (spd > 0)
					{
						log_i("Motor %d outside max limit & moving further positive => STOP", i);
						stopStepper(i);
					}
					// else if speed < 0 => let it keep running to move back inside the range
				}
			}
			if (isActivated[i] and false)
				log_i("Stop Motor %i in loop, isRunning %i, data[i]->stopped %i, data[i]-speed %i, position %i", i, isRunning(i), data[i]->stopped, getData()[i]->speed, getData()[i]->currentPosition);
			if (isActivated[i] && !isRunning(i) && !data[i]->stopped && !data[i]->isforever)
			{
				// If the motor is not running, we stop it, report the position and save the position
				// This is the ordinary case if the motor is not connected via I2C/CAN
				// log_d("Sending motor pos %i", i);
				log_i("Stop Motor (2) %i in loop, mIsRunning %i, data[i]->stopped %i", i, isRunning(i), !data[i]->stopped);
				stopStepper(i);
			}

			
			#endif
			if (false and isActivated[i] && isRunning(i)){ // TODO: This is nice, but maybe not very efficient - only for updating the position in the gui periodically
				// indicate that the motor is running by sending the position periodically
				// Check if it's time to send motor positions
				unsigned long now = millis();
				if (now - lastSendTime >= interval)
				{
					lastSendTime = now;
					// Send motor positions for all motors
					for (int i = 0; i < MOTOR_AXIS_COUNT; i++)
					{
						if (isActivated[i])
						{
							sendMotorPos(i, 0, -2);
						}
					}
				}
			}
		}
	}

	bool isRunning(int i)
	{
		bool mIsRunning = false;
#if defined(CAN_BUS_ENABLED) && defined(CAN_SEND_COMMANDS) && !defined(CAN_RECEIVE_MOTOR)
		// HYBRID MODE SUPPORT: Check if this axis uses CAN or native driver
		if (shouldUseCANForAxis(i))
		{
			// CAN axes: get status from CAN controller
			mIsRunning = can_controller::isMotorRunning(i);
		}
		else
		{
			// Native axes: get status from local driver
#if defined(USE_FASTACCEL)
			mIsRunning = FAccelStep::isRunning(i);
#elif defined(USE_ACCELSTEP)
			mIsRunning = AccelStep::isRunning(i);
#endif
		}
#elif defined USE_FASTACCEL
		mIsRunning = FAccelStep::isRunning(i);
#elif defined USE_ACCELSTEP
		mIsRunning = AccelStep::isRunning(i);
#elif defined I2C_MASTER
		// Request data from the slave but only if inside i2cAddresses
		MotorState mData = i2c_master::getMotorState(i);
		mIsRunning = mData.isRunning;
#elif defined CAN_BUS_ENABLED
		// Slave will push this information to the master via CAN asynchrously
		mIsRunning = can_controller::isMotorRunning(i);
#endif
		return mIsRunning;
	}

	// returns json {"steppers":[...]} as qid
	void sendMotorPos(int i, int arraypos, int qid)
	{
		// Safety check: ensure mutex is initialized
		if (xSerialMutex == NULL)
		{
			log_e("Serial mutex not initialized - cannot send motor position");
			return;
		}
		
		// Protect entire function to prevent concurrent access to cJSON objects and serial output
		// This prevents heap corruption when multiple threads try to send motor positions simultaneously
		if (!xSemaphoreTake(xSerialMutex, pdMS_TO_TICKS(1000)))
		{
			log_w("Failed to acquire serial mutex for sendMotorPos");
			return;
		}

		// update current position of the motor depending on the interface
		updateData(i);

		cJSON *root = cJSON_CreateObject();
		if (root == NULL)
		{
			xSemaphoreGive(xSerialMutex);
			return; // Handle allocation failure
		}

		cJSON *stprs = cJSON_CreateArray();
		if (stprs == NULL)
		{
			cJSON_Delete(root);
			xSemaphoreGive(xSerialMutex);
			return; // Handle allocation failure
		}
		cJSON_AddItemToObject(root, key_steppers, stprs);
		if (qid == -1)
			qid = data[i]->qid;
		cJSON_AddNumberToObject(root, "qid", qid);

		cJSON *item = cJSON_CreateObject();
		if (item == NULL)
		{
			cJSON_Delete(root);
			xSemaphoreGive(xSerialMutex);
			return; // Handle allocation failure
		}
		cJSON_AddItemToArray(stprs, item);
		cJSON_AddNumberToObject(item, key_stepperid, i);
		cJSON_AddNumberToObject(item, key_position, data[i]->currentPosition);
		cJSON_AddNumberToObject(item, "isDone", data[i]->stopped);

		// Add encoder position if encoder-based motion is enabled
#ifdef LINEAR_ENCODER_CONTROLLER
		if (i==1) {
			// Get current encoder position in micrometers
			float encoderPos = PCNTEncoderController::getCurrentPosition(i);
			cJSON_AddNumberToObject(item, "encoderPosition", encoderPos);
			
			// Also add raw encoder count for debugging
			int64_t encoderCount = PCNTEncoderController::getEncoderCount(i);
			cJSON_AddNumberToObject(item, "encoderCount", (int)encoderCount);
		}
#endif

		// also save in preferences
		preferences.begin("UC2", false);
		preferences.putInt(("motor" + String(i)).c_str(), data[i]->currentPosition);
		preferences.end();


		arraypos++;

#ifdef WIFI
		WifiController::sendJsonWebSocketMsg(root);
#endif
#ifdef defined I2C_MASTER &&defined DIAL_CONTROLLER
		i2c_master::pushMotorPosToDial();
#endif

		// CRITICAL: Serialize to string BEFORE releasing mutex to prevent doc from being freed
		// This ensures atomic operation: create JSON -> serialize -> delete
		char *jsonString = cJSON_PrintUnformatted(root);
		
		// Check if serialization was successful before proceeding
		if (jsonString == NULL)
		{
			log_e("Failed to serialize motor position JSON for motor %d", i);
			cJSON_Delete(root);
			xSemaphoreGive(xSerialMutex);
			return;
		}
		
		// Delete cJSON object immediately after serialization
		cJSON_Delete(root);
		root = NULL;
		
		// Release mutex AFTER JSON operations are complete
		xSemaphoreGive(xSerialMutex);
		
		// Now send the pre-serialized string through the safe output queue
		SerialProcess::safeSendJsonString(jsonString);
		
		// Free the serialized string - cJSON_PrintUnformatted allocates with malloc
		free(jsonString);
		jsonString = NULL;

#ifdef CAN_RECEIVE_MOTOR
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

#if defined(CAN_BUS_ENABLED) && defined(CAN_SEND_COMMANDS) && !defined(CAN_RECEIVE_MOTOR)
		// HYBRID MODE SUPPORT: Check if this axis should use CAN or native driver
		if (shouldUseCANForAxis(i))
		{
			// Stop via CAN
			log_i("Hybrid mode: Stopping axis %d via CAN", i);
			getData()[i]->isforever = false;
			getData()[i]->speed = 0;
			getData()[i]->stopped = true;
			getData()[i]->isStop = true;
			Stepper s = static_cast<Stepper>(i);
			can_controller::stopStepper(s);
		}
		else
		{
			// Stop native driver
			log_i("Hybrid mode: Stopping axis %d via native driver", i);
#if defined(USE_FASTACCEL)
			FAccelStep::stopFastAccelStepper(i);
#elif defined(USE_ACCELSTEP)
			AccelStep::stopAccelStepper(i);
#else
			log_w("No native motor driver available for axis %d in hybrid mode", i);
#endif
		}
#elif defined USE_FASTACCEL
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
#elif defined CAN_BUS_ENABLED && !defined CAN_RECEIVE_MOTOR
		getData()[i]->isforever = false;
		getData()[i]->speed = 0;
		getData()[i]->stopped = true;
		getData()[i]->isStop = true;
		Stepper s = static_cast<Stepper>(i);
		can_controller::stopStepper(s);
		
#endif
log_i("stopStepper Focus Motor %i, stopped: %i", i, data[i]->stopped);
// only send motor data if it was running before
	sendMotorPos(i, 0); // rather here or at the end? M5Dial needs the position ASAP

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
