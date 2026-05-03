#include <PinConfig.h>
#include "../../config.h"
#include "FocusMotor.h"
#include "MotorEncoderConfig.h"
#include "Wire.h"
#include "../wifi/WifiController.h"
#include "../../cJsonTool.h"
#include "../state/State.h"
#include "../serial/SerialProcess.h"
#include "../qid/QidRegistry.h"
#include "esp_debug_helpers.h"
#ifdef LINEAR_ENCODER_CONTROLLER
#include "../encoder/LinearEncoderController.h"
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
#ifdef I2C_MASTER
#include "../i2c/i2c_master.h"
#endif
#ifdef CAN_BUS_ENABLED
#include "../can/can_transport.h"
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
	// Grace-period counter: skip this many loop iterations after startStepper()
	// before the stop-condition is allowed to fire.  Gives the stepper time to
	// start actually running (especially important for CAN-dispatched moves).
	int waitForFirstRun[] = {0, 0, 0, 0};

	xSemaphoreHandle xMutex = NULL;
	xSemaphoreHandle xSerialMutex = NULL; // Mutex for serial JSON output

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

	// Compute the requested motion direction for the current command parameters.
	// Returns +1, -1, or 0 (unknown / stop / no motion).
	static int8_t computeRequestedDir(int axis)
	{
		MotorData *md = getData()[axis];
		if (md->isStop)
			return 0;
		if (md->isforever)
		{
			if (md->speed > 0) return 1;
			if (md->speed < 0) return -1;
			return 0;
		}
		if (md->absolutePosition)
		{
			int32_t delta = md->targetPosition - md->currentPosition;
			if (delta > 0) return 1;
			if (delta < 0) return -1;
			return 0;
		}
		// relative move
		if (md->targetPosition > 0) return 1;
		if (md->targetPosition < 0) return -1;
		return 0;
	}

	// Returns false if motion in that direction is locked out by a previous
	// hard-limit hit. requestedDir is +1, -1, or 0.
	static bool directionAllowed(int axis, int8_t requestedDir)
	{
		MotorData *md = getData()[axis];
		if (md->hardLimitLockoutDir == 0)
			return true;
		if (requestedDir == 0)
			return true; // let stops / zero-speed updates through
		return requestedDir != md->hardLimitLockoutDir;
	}

	void startStepper(int axis, int reduced = 0)
	{
		/*
		reduced organizes the ammount of data that is being transmitted (E.g. via CAN Bus)
		reduced: 0 => All data transfered MotorData
		reduced: 1 => MotorDataReduced is transfered
		reducsed: 2 => single Value udpates
	*/

		// Directional hard-limit lockout. Homing legitimately drives into the endstop,
		// so it always bypasses the guard.
		int8_t requestedDir = computeRequestedDir(axis);
		if (!getData()[axis]->isHoming && !directionAllowed(axis, requestedDir))
		{
			log_w("Axis %d: motion in dir %d blocked by hard-limit lockout (lockoutDir=%d)",
				  axis, (int)requestedDir, (int)getData()[axis]->hardLimitLockoutDir);
			getData()[axis]->stopped = true;
			sendMotorPos(axis, 0, -3); // error report
#ifdef CAN_RECEIVE_MOTOR
			if (getData()[axis]->qid > 0)
				can_controller::sendQidReportToMaster(getData()[axis]->qid, 1);
#else
			QidRegistry::reportActionError(getData()[axis]->qid);
#endif
			return;
		}
		if (requestedDir != 0)
			getData()[axis]->lastCommandedDir = requestedDir;

		// Use timeout instead of portMAX_DELAY to prevent PS4 controller from blocking indefinitely
		// when serial commands are being processed. 50ms is enough for most operations while
		// allowing gamepad to retry quickly if mutex is busy.
		if (xSemaphoreTake(xMutex, pdMS_TO_TICKS(50)))
		{
#if defined(USE_FASTACCEL)
			waitForFirstRun[axis] = 10;
			FAccelStep::startFastAccelStepper(axis);
#elif defined(USE_ACCELSTEP)
			AccelStep::startAccelStepper(axis);
#endif
			getData()[axis]->stopped = false;
			xSemaphoreGive(xMutex);
		}
		else
		{
			log_w("startStepper: Could not obtain mutex to start motor on axis %d", axis);
			// Optionally, set an error state or retry logic here
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
			if (getData()[s]->encoderBasedMotion)
			{
				log_i("Starting encoder-based precise motion for stepper %d", s);
#ifdef LINEAR_ENCODER_CONTROLLER
				// Use LinearEncoderController for precise motion
				startEncoderBasedMotion(s);
#else
				log_w("Encoder-based motion requested but LINEAR_ENCODER_CONTROLLER not defined");
				startStepper(s, reduced);
#endif
			}
			else
			{
				startStepper(s, reduced); 
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



	void updateData(int axis)
	{
#if defined(USE_FASTACCEL)
		FAccelStep::updateData(axis);
#elif defined(USE_ACCELSTEP)
		AccelStep::updateData(axis);
#elif defined(I2C_MASTER)
		MotorState mMotorState = i2c_master::pullMotorDataReducedDriver(axis);
		data[axis]->currentPosition = mMotorState.currentPosition;
#endif
	}

	long getCurrentMotorPosition(int axis)
	{
#if defined(USE_FASTACCEL)
		return FAccelStep::getCurrentPosition(static_cast<Stepper>(axis));
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
#if MOTOR_AXIS_COUNT > 4
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
#endif


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
			data[Stepper::A]->directionPinInverted = preferences.getInt("motainvert", false);
			data[Stepper::A]->joystickDirectionInverted = preferences.getBool(("joyDir" + String(Stepper::A)).c_str(), false);
			data[Stepper::A]->hardLimitEnabled = preferences.getBool(("hlEn" + String(Stepper::A)).c_str(), false);	  // Disabled by default
			data[Stepper::A]->hardLimitPolarity = preferences.getBool(("hlPol" + String(Stepper::A)).c_str(), false); // NO by default
			isActivated[Stepper::A] = true;
			log_i("Motor A position: %i", data[Stepper::A]->currentPosition);
		}
		if (pinConfig.MOTOR_X_STEP >= 0)
		{
			data[Stepper::X]->dirPin = pinConfig.MOTOR_X_DIR;
			data[Stepper::X]->stpPin = pinConfig.MOTOR_X_STEP;
			data[Stepper::X]->currentPosition = preferences.getInt(("motor" + String(Stepper::X)).c_str());
			data[Stepper::X]->directionPinInverted = preferences.getInt("motxinv", false);
			data[Stepper::X]->joystickDirectionInverted = preferences.getBool(("joyDir" + String(Stepper::X)).c_str(), false);
			data[Stepper::X]->hardLimitEnabled = preferences.getBool(("hlEn" + String(Stepper::X)).c_str(), false);	  // Disabled by default
			data[Stepper::X]->hardLimitPolarity = preferences.getBool(("hlPol" + String(Stepper::X)).c_str(), false); // NO by default
			isActivated[Stepper::X] = true;
			log_i("Motor X position: %i", data[Stepper::X]->currentPosition);
		}
		if (pinConfig.MOTOR_Y_STEP >= 0)
		{
			data[Stepper::Y]->dirPin = pinConfig.MOTOR_Y_DIR;
			data[Stepper::Y]->stpPin = pinConfig.MOTOR_Y_STEP;
			data[Stepper::Y]->currentPosition = preferences.getInt(("motor" + String(Stepper::Y)).c_str());
			data[Stepper::Y]->directionPinInverted = preferences.getInt("motyinv", false);
			data[Stepper::Y]->joystickDirectionInverted = preferences.getBool(("joyDir" + String(Stepper::Y)).c_str(), false);
			data[Stepper::Y]->hardLimitEnabled = preferences.getBool(("hlEn" + String(Stepper::Y)).c_str(), false);	  // Disabled by default
			data[Stepper::Y]->hardLimitPolarity = preferences.getBool(("hlPol" + String(Stepper::Y)).c_str(), false); // NO by default
			isActivated[Stepper::Y] = true;
			log_i("Motor Y position: %i", data[Stepper::Y]->currentPosition);
		}
		if (pinConfig.MOTOR_Z_STEP >= 0)
		{
			data[Stepper::Z]->dirPin = pinConfig.MOTOR_Z_DIR;
			data[Stepper::Z]->stpPin = pinConfig.MOTOR_Z_STEP;
			data[Stepper::Z]->currentPosition = preferences.getInt(("motor" + String(Stepper::Z)).c_str());
			data[Stepper::Z]->directionPinInverted = preferences.getInt("motzinv", false);
			data[Stepper::Z]->joystickDirectionInverted = preferences.getBool(("joyDir" + String(Stepper::Z)).c_str(), false);
			data[Stepper::Z]->hardLimitEnabled = preferences.getBool(("hlEn" + String(Stepper::Z)).c_str(), false);	  // Disabled by default
			data[Stepper::Z]->hardLimitPolarity = preferences.getBool(("hlPol" + String(Stepper::Z)).c_str(), false); // NO by default
			isActivated[Stepper::Z] = true;
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
#endif

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

	void setHardLimit(int axis, bool enabled, bool polarity)
	{
		// Set hard limit configuration for a motor axis
		// enabled: if true, motor will emergency stop when endstop is hit during normal operation
		// polarity: 0 = normally open (NO), 1 = normally closed (NC)
		if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
			return;

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
		if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
			return;
		getData()[axis]->hardLimitTriggered = false;
		log_i("Cleared hard limit triggered flag for axis %d", axis);
	}

	// Per-axis hard-limit handling: trip detection (rising edge) and lockout-clear
	// when the endstop is physically released and the motor is idle.
	static void evaluateHardLimitForAxis(int axis, int digitalInputIdx)
	{
		MotorData *md = getData()[axis];
		if (!isActivated[axis] || !md->hardLimitEnabled || md->isHoming)
			return;

		bool endstopState = DigitalInController::getDigitalVal(digitalInputIdx);
		bool polarity = md->hardLimitPolarity;
		// Pressed when endstopState != polarity
		// (NO/polarity=0: pressed=1; NC/polarity=1: pressed=0)
		bool pressed = (endstopState != polarity);

		// --- Trip detection (rising edge while motor is moving) ---
		if (pressed && isRunning(axis) && !md->hardLimitTriggered)
		{
			// Determine direction of the motion that just hit the endstop.
			int8_t dir = md->lastCommandedDir;
			if (dir == 0)
			{
				if (md->speed > 0) dir = 1;
				else if (md->speed < 0) dir = -1;
				else
				{
					int32_t delta = md->targetPosition - md->currentPosition;
					if (delta > 0) dir = 1;
					else if (delta < 0) dir = -1;
				}
			}
			log_e("HARD LIMIT TRIGGERED on axis %d (endstop=%d, polarity=%d, dir=%d)",
				  axis, endstopState, polarity, (int)dir);
			md->hardLimitLockoutDir = dir;
			md->hardLimitTriggered = true;
			stopStepper(axis);
			sendMotorPos(axis, 0, -3); // one-shot error report
			return;
		}

		// --- Lockout clear when endstop is released and motor is idle ---
		if (!pressed && md->hardLimitLockoutDir != 0 && !isRunning(axis))
		{
			log_i("Hard-limit lockout cleared on axis %d (endstop released)", axis);
			md->hardLimitLockoutDir = 0;
			md->hardLimitTriggered = false;
		}
	}

	void checkHardLimits()
	{
#if defined(CAN_RECEIVE_MOTOR) && defined(MOTOR_CONTROLLER) && defined(DIGITAL_IN_CONTROLLER)
		// CAN slave / satellite: single motor on REMOTE_MOTOR_AXIS_ID, endstop on DIGITAL_IN_1
		Stepper mStepper = static_cast<Stepper>(pinConfig.REMOTE_MOTOR_AXIS_ID);
		evaluateHardLimitForAxis(mStepper, 1);
#elif defined(MOTOR_CONTROLLER) && defined(DIGITAL_IN_CONTROLLER) && !defined(CAN_BUS_ENABLED)
		// Non-CAN: X/Y/Z mapped to digital inputs 1/2/3
		evaluateHardLimitForAxis(Stepper::X, 1);
		evaluateHardLimitForAxis(Stepper::Y, 2);
		evaluateHardLimitForAxis(Stepper::Z, 3);
#endif
		// CAN_BUS_ENABLED masters don't check hard limits locally - slaves handle this
		// and report back via CAN when a hard limit is triggered
	}

	bool isEncoderBasedMotionEnabled(int axis)
	{
		if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
			return false;
		return getData()[axis]->encoderBasedMotion;
	}

	void setEncoderBasedMotion(int axis, bool enabled)
	{
		if (axis < 0 || axis >= MOTOR_AXIS_COUNT)
			return;
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
		// All values are in pure encoder counts now - no conversion!
		MotorData *motorData = getData()[axis];

		// Position and speed are passed directly in encoder counts
		// No conversion factor needed - host computes encoder counts directly
		int32_t encoderPosition = motorData->targetPosition;
		int32_t encoderSpeed = abs(motorData->speed);

		// Create JSON for LinearEncoderController moveP command
		// Structure: {"task": "/linearencoder_act", "moveP": {"steppers": [...]}}
		cJSON *movePreciseJson = cJSON_CreateObject();
		cJSON *moveP = cJSON_CreateObject();
		cJSON *steppers = cJSON_CreateArray();
		cJSON *stepper = cJSON_CreateObject();

		// Add task identifier
		cJSON_AddStringToObject(movePreciseJson, "task", "/linearencoder_act");

		// Build stepper object - pure encoder counts
		cJSON_AddNumberToObject(stepper, "stepperid", axis);
		cJSON_AddNumberToObject(stepper, "position", encoderPosition);
		cJSON_AddNumberToObject(stepper, "isabs", motorData->absolutePosition ? 1 : 0);
		cJSON_AddNumberToObject(stepper, "speed", encoderSpeed);

		// Set default PID values if not already configured
		cJSON_AddNumberToObject(stepper, "cp", 20.0); // Proportional gain
		cJSON_AddNumberToObject(stepper, "ci", 1.0);  // Integral gain
		cJSON_AddNumberToObject(stepper, "cd", 5.0);  // Derivative gain

		// Nest properly: steppers array -> moveP object -> root object
		cJSON_AddItemToArray(steppers, stepper);
		cJSON_AddItemToObject(moveP, "steppers", steppers);
		cJSON_AddItemToObject(movePreciseJson, "moveP", moveP);

		log_i("Starting encoder-based motion for axis %d: position=%d counts (abs=%d)",
			  axis, encoderPosition, motorData->absolutePosition ? 1 : 0);

// Start encoder accuracy tracking for this move
#ifdef LINEAR_ENCODER_CONTROLLER
		PCNTEncoderController::startEncoderTracking(axis, motorData->targetPosition);
#endif

		// print the json for debugging
		char *jsonString = cJSON_Print(movePreciseJson);
		if (jsonString)
		{
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
		// Check for QID timeouts
		QidRegistry::tickTimeout();

// Check hard limits ONCE per loop iteration (not per motor)
// Hard limits are only checked on slaves or non-CAN configurations
#if (!defined(CAN_BUS_ENABLED) || defined(CAN_RECEIVE_MOTOR))
		checkHardLimits();
#endif

		for (int i = 0; i < MOTOR_AXIS_COUNT; i++)
		{
#if defined(USE_FASTACCEL) || defined(USE_ACCELSTEP)
			// Skip axes that were never activated (no step pin configured)
			if (!isActivated[i])
				continue;

			// checks if a stepper is still running
			// seems like the i2c needs a moment to start the motor (i.e. act is async and loop is continously running, maybe faster than the motor can start)
			if (waitForFirstRun[i])
			{
				waitForFirstRun[i]--;
				continue;
			}
			if (isActivated[i] && !isRunning(i) && !data[i]->stopped && !data[i]->isforever && !data[i]->isHoming)
			{
				// If the motor is not running, we stop it, report the position and save the position
				// This is the ordinary case if the motor is not connected via I2C/CAN
				// Skip during homing - the homing task manages motor lifecycle
				log_d("Stop Motor (2) %i in loop, mIsRunning %i, data[i]->stopped %i", i, isRunning(i), !data[i]->stopped);
				stopStepper(i);
			}

#endif
		}
	}

	bool isRunning(int i)
	{
		bool mIsRunning = false;
		// Fallback: local stepper only (CAN routing handled by DeviceRouter)
#if defined(USE_FASTACCEL)
		mIsRunning = FAccelStep::isRunning(i);
#elif defined(USE_ACCELSTEP)
		mIsRunning = AccelStep::isRunning(i);
#elif defined(I2C_MASTER)
		MotorState mData = i2c_master::getMotorState(i);
		mIsRunning = mData.isRunning;
#endif
		return mIsRunning;
	}

	// returns json {"steppers":[...]} as qid
	void sendMotorPos(int i, int arraypos, int qid)
	{
		// Always update and persist position first, independent of serial mutex
		updateData(i);
		preferences.begin("UC2", false);
		preferences.putInt(("motor" + String(i)).c_str(), data[i]->currentPosition);
		preferences.end();

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

		// update current position of the motor depending on the interface (already done above, refresh for JSON)
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
		if (i == 1)
		{
			// Get current encoder position (raw counts - no conversion!)
			bool encoderDirection = LinearEncoderController::getEncoderData(i)->encoderDirection;
			int64_t encoderCount = PCNTEncoderController::getEncoderCount(i);
			// int32_t currentPos = edata[s]->encoderDirection ? -rawPos : rawPos;
			// get encoderDirection

			cJSON_AddNumberToObject(item, "encoderCount", encoderCount ? (int)encoderCount : -(int)encoderCount);
		}
#endif

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
		// Stop local stepper (CAN routing handled by DeviceRouter)
#if defined(USE_FASTACCEL)
		FAccelStep::stopFastAccelStepper(i);
#elif defined(USE_ACCELSTEP)
		AccelStep::stopAccelStepper(i);
#elif defined(I2C_MASTER)
		getData()[i]->isforever = false;
		getData()[i]->speed = 0;
		getData()[i]->stopped = true;
		getData()[i]->isStop = true;
		MotorData *m = getData()[i];
		i2c_master::stopStepper(m, i);
#endif
		log_i("stopStepper Focus Motor %i, stopped: %i", i, data[i]->stopped);
		// only send motor data if it was running before
		sendMotorPos(i, 0); // rather here or at the end? M5Dial needs the position ASAP
		// Report QID completion
#ifdef CAN_RECEIVE_MOTOR
		// On CAN slave: send QID report to master via dedicated CAN message
		if (data[i]->qid > 0)
			can_controller::sendQidReportToMaster(data[i]->qid, 0);
#else
		QidRegistry::reportActionDone(data[i]->qid);
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
