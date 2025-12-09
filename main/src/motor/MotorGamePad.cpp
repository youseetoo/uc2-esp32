#include "MotorGamePad.h"
#include "FocusMotor.h"
#include "MotorTypes.h"
#include "../objective/ObjectiveController.h"


// sample pio project e.g. https://github.com/inventonater/flashbike-matrix/blob/master/scratch/tetris/tetris-matrix-s3.cpp

namespace MotorGamePad
{
constexpr int   kOffset         = 1025;      // joystick dead-zone
constexpr float kAlpha          = 0.40f;     // linear range boundary
constexpr float kMaxSpeed       = 2000.0f;   // base max speed
constexpr float kOneOver32768f  = 1.0f / 32768.0f;

// State variables for fine/coarse mode and button edge detection
static bool isFineMode = false;
static float joystickScaleFactor = 1.0f;
static unsigned long lastOptionsButtonTime = 0; // Track last button press time
static bool optionsButtonWasPressed = false; // Track if button was recently pressed
static const unsigned long BUTTON_DEBOUNCE_TIME = 300; // ms to wait between toggles

// Joystick offset calibration on startup
static bool isCalibrated = false;
static int16_t joystickOffsets[4] = {0, 0, 0, 0}; // X, Y, Z, A

// Auto-stop on inactivity
static const unsigned long INACTIVITY_TIMEOUT = 10000; // ms of no change before stopping motor
static int16_t lastAxisValues[4] = {0, 0, 0, 0}; // Track last values for each axis
static unsigned long lastAxisChangeTime[4] = {0, 0, 0, 0}; // Track last change time for each axis


	static inline void stopAxis(int ax)
	{
		FocusMotor::stopStepper(ax);
		axisRunning[ax] = false;
	}

	static inline void startAxis(int ax, int speed)
	{
		auto *d = FocusMotor::getData()[ax];
		d->speed = speed;
		d->isforever = true;
		d->acceleration = MAX_ACCELERATION_A;

		FocusMotor::startStepper(ax, 1);
		axisRunning[ax] = true;
	}

	static float curve(int16_t raw)
	{
		const int16_t centred = raw - (raw >= 0 ? kOffset : -kOffset); // symmetric offset
		const float x = static_cast<float>(centred) * kOneOver32768f;
		const float absX = std::fabs(x);
		const float signX = (x >= 0.f) ? 1.f : -1.f;

		if (absX <= kAlpha)
			return x; // linear part

		const float diff = absX - kAlpha; // excess over α
		const float scale = 1.f - kAlpha;
		const float quad = (diff * diff) / (scale * scale); // 0…1
		return signX * (kAlpha + quad * (1.f - kAlpha));	// α…1, C1-continuous
	}

	inline void handleAxis(int16_t value, int ax)
	{
		#ifdef CAN_SEND_COMMANDS 
		if (ax == Stepper::A)
		return;
		#endif
		
		// Apply offset calibration
		value -= joystickOffsets[ax];
		
		// Apply direction inversion if configured
		if (FocusMotor::getData()[ax]->joystickDirectionInverted) {
			value = -value;
		}
		
		// Check for inactivity - if value hasn't changed, check timeout
		unsigned long currentTime = millis();
		if (value == lastAxisValues[ax])
		{
			// Value unchanged - check if timeout exceeded
			if (axisRunning[ax] && (currentTime - lastAxisChangeTime[ax] > INACTIVITY_TIMEOUT))
			{
				stopAxis(ax);
				log_d("Motor %d auto-stopped due to inactivity", ax);
			}
		}
		else
		{
			// Value changed - update tracking
			lastAxisValues[ax] = value;
			lastAxisChangeTime[ax] = currentTime;
		}
		
		// dead-zone ────────────────────────────────────────────────────────────
		if (std::abs(value) <= kOffset)
		{
			if (axisRunning[ax])
				stopAxis(ax);
			return;
		}

		// Z⇄A mutual exclusion ────────────────────────────────────────────────
		#ifndef CAN_SEND_COMMANDS 
		{ // TODO: Problem might be that they cancel each other out if there is not return from CAN
			if (ax == Stepper::Z && axisRunning[Stepper::A])
				stopAxis(Stepper::A);
			if (ax == Stepper::A && axisRunning[Stepper::Z])
				stopAxis(Stepper::Z);
		}
		#endif
		// speed computation ───────────────────────────────────────────────────
		float speed = curve(value) * kMaxSpeed;

		// per-axis scaling
		speed *= (ax == Stepper::Z || (ax == Stepper::A && FocusMotor::getDualAxisZ()))
					 ? pinConfig.JOYSTICK_SPEED_MULTIPLIER_Z
					 : pinConfig.JOYSTICK_SPEED_MULTIPLIER;

		// Apply fine/coarse mode scaling from MotorGamePad
		speed *= getJoystickScaleFactor();

		startAxis(ax, static_cast<int>(speed));

		log_i("Motor %d: raw=%d  speed=%f  scale=%.1f  mode=%s", 
			  ax, value, speed, getJoystickScaleFactor(),
			  isInFineMode() ? "FINE" : "COARSE");
	}

	void xyza_changed_event(int x, int y, int z, int a)
	{
		// log_i("xyza_changed_event x:%d y:%i z:%i a:%i", x,y,z,a);

		// Calibrate on first call - capture initial joystick position as offset
		if (!isCalibrated)
		{
			joystickOffsets[Stepper::X] = x;
			joystickOffsets[Stepper::Y] = y;
			joystickOffsets[Stepper::Z] = z;
			joystickOffsets[Stepper::A] = a;
			isCalibrated = true;
			
			log_i("Joystick calibrated with offsets: X=%d, Y=%d, Z=%d, A=%d", x, y, z, a);
			return; // Skip first iteration to avoid initial movement
		}

		// X-Direction
		handleAxis(x, Stepper::X);

		// Y-direction
		handleAxis(y, Stepper::Y);

		// Z-direction
		//if (!FocusMotor::getData()[Stepper::A]->isforever;)
		handleAxis(z, Stepper::Z);

		// A-direction
		//if (!FocusMotor::getData()[Stepper::Z]->isforever;)
		handleAxis(a, Stepper::A);
	}

	void options_changed_event(int pressed)
	{
		// Convert to boolean for clearer logic
		bool isPressed = (pressed != 0);
		unsigned long currentTime = millis();
		
		log_i("options_changed_event: pressed=%d, isPressed=%s, time=%lu", 
			  pressed, isPressed ? "true" : "false", currentTime);
		
		// Only trigger toggle if button is pressed AND enough time has passed since last toggle
		if (isPressed && (currentTime - lastOptionsButtonTime > BUTTON_DEBOUNCE_TIME))
		{
			// Toggle between fine and coarse mode
			isFineMode = !isFineMode;
			joystickScaleFactor = isFineMode ? 0.1f : 1.0f;
			
			log_i("Joystick mode switched to %s (scale factor: %.1f)", 
				  isFineMode ? "FINE" : "COARSE", joystickScaleFactor);
			
			// Update the time of last toggle
			lastOptionsButtonTime = currentTime;
		}
		else if (isPressed)
		{
			log_d("Button press ignored - too soon after last toggle (debounce)");
		}
	}


	void singlestep_event(int left, int right, bool r1, bool r2, bool l1, bool l2)
	{
		// log_i("singlestep_event left:%d right:%i r1:%i r2:%i l1:%i l2:%i", left,right,r1,r2,l1,l2);
		// for r1/l1 move z-axis by 10 steps
		// for r2/l2 move z-axis by 100 steps
		if (r1)
		{
			FocusMotor::getData()[Stepper::Z]->isforever = false;
			FocusMotor::getData()[Stepper::Z]->speed = 20000;
			FocusMotor::getData()[Stepper::Z]->acceleration = MAX_ACCELERATION_A;
			FocusMotor::getData()[Stepper::Z]->targetPosition = 1;
			FocusMotor::getData()[Stepper::Z]->absolutePosition = false;
			FocusMotor::startStepper(Stepper::Z, 1);
		}
		if (r2)
		{
			FocusMotor::getData()[Stepper::Z]->isforever = false;
			FocusMotor::getData()[Stepper::Z]->speed = 20000;
			FocusMotor::getData()[Stepper::Z]->acceleration = MAX_ACCELERATION_A;
			FocusMotor::getData()[Stepper::Z]->targetPosition = 10;
			FocusMotor::getData()[Stepper::Z]->absolutePosition = false;
			FocusMotor::startStepper(Stepper::Z, 1);
		}
		if (l1)
		{
			FocusMotor::getData()[Stepper::Z]->isforever = false;
			FocusMotor::getData()[Stepper::Z]->speed = 20000;
			FocusMotor::getData()[Stepper::Z]->acceleration = MAX_ACCELERATION_A;
			FocusMotor::getData()[Stepper::Z]->targetPosition = -1;
			FocusMotor::getData()[Stepper::Z]->absolutePosition = false;
			FocusMotor::startStepper(Stepper::Z, 1);
		}
		if (l2)
		{
			FocusMotor::getData()[Stepper::Z]->isforever = false;
			FocusMotor::getData()[Stepper::Z]->speed = 20000;
			FocusMotor::getData()[Stepper::Z]->acceleration = MAX_ACCELERATION_A;
			FocusMotor::getData()[Stepper::Z]->targetPosition = -10;
			FocusMotor::getData()[Stepper::Z]->absolutePosition = false;
			FocusMotor::startStepper(Stepper::Z, 1);
		}
	}

		float getJoystickScaleFactor()
	{
		return joystickScaleFactor;
	}

	bool isInFineMode()
	{
		return isFineMode;
	}

	void resetCalibration()
	{
		isCalibrated = false;
		joystickOffsets[0] = joystickOffsets[1] = joystickOffsets[2] = joystickOffsets[3] = 0;
		log_i("Joystick calibration reset - will recalibrate on next input");
	}

}