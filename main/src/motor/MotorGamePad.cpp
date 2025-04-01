#include "MotorGamePad.h"
#include "FocusMotor.h"
#include "MotorTypes.h"
#ifdef CAN_CONTROLLER
#include "../can/can_controller.h"
#endif

// sample pio project e.g. https://github.com/inventonater/flashbike-matrix/blob/master/scratch/tetris/tetris-matrix-s3.cpp

namespace MotorGamePad
{
	bool joystick_drive_X = false;
	bool joystick_drive_Y = false;
	bool joystick_drive_Z = false;
	bool joystick_drive_A = false;
	
	int offset_val = 1025; // offset value for joystick

	void handleAxis(int value, int s)
	{

		if (abs(value) > offset_val)
		{
			// we want to treat the value non-linearly such that for small values it behaves linear but for larger values it behaves non-linear (e.g. quadratic)

			// 1) Normalise value
			float maxVal = pow(2, 15);		  // 32768
			float x = (float)(value-offset_val) / maxVal;

			// 2) Piecewise-Function: linear until ±alpha, then quadratic
			float alpha = 0.4f; // up to ±alpha linear, then quadratic
			float signX = (x >= 0.0f) ? 1.0f : -1.0f;
			float absX = fabs(x);

			float out;
			if (absX <= alpha)
			{
				// linear
				out = x * alpha; // scaled from 0..alpha
			}
			else
			{
				// quadratic 
				float diff = absX - alpha;					// Anteil über alpha
				float scale = 1.0f - alpha;					// verfügbarer Rest bis 1
				float quad = diff * diff / (scale * scale); // normalisiertes Quadrat
				out = alpha + quad * (1.0f - alpha);		// skaliert von alpha..1
				out = signX * out;
			}

			// 3) converseion to motor speed
			float maxSpeed = 2000.0f;
			float motorSpeed = out * maxSpeed;

			// 4) Auf Z-Achse ggf. anders skalieren
			if (s == Stepper::Z or (s == Stepper::A && FocusMotor::getDualAxisZ()))
			{
				motorSpeed *= pinConfig.JOYSTICK_SPEED_MULTIPLIER_Z;
			}
			else
			{
				motorSpeed *= pinConfig.JOYSTICK_SPEED_MULTIPLIER;
			}

			log_i("Motor %i: %i -> %f", s, value, motorSpeed);
			// move_x
			FocusMotor::getData()[s]->speed = (int)motorSpeed;
			FocusMotor::getData()[s]->isforever = true;
			FocusMotor::getData()[s]->acceleration = MAX_ACCELERATION_A;
// log_i("Start motor from BT %i with speed %i", s, getData()[s]->speed);
#ifdef CAN_CONTROLLER
			can_controller::sendMotorSingleValue(s, offsetof(MotorData, speed), (int)motorSpeed);
			can_controller::sendMotorSingleValue(s, offsetof(MotorData, isforever), true);
			can_controller::sendMotorSingleValue(s, offsetof(MotorData, isStop), false);
#else
			FocusMotor::startStepper(s, true);
#endif

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
			FocusMotor::getData()[s]->speed = 0;
			FocusMotor::getData()[s]->isforever = false;
			if (s == Stepper::X and joystick_drive_X)
			{
#ifdef CAN_CONTROLLER
				can_controller::sendMotorSingleValue(s, offsetof(MotorData, speed), 0);
				can_controller::sendMotorSingleValue(s, offsetof(MotorData, isStop), true);
#else
				FocusMotor::stopStepper(s);
				FocusMotor::stopStepper(s);
#endif

				joystick_drive_X = false;
			}
			if (s == Stepper::Y and joystick_drive_Y)
			{
#ifdef CAN_CONTROLLER
				can_controller::sendMotorSingleValue(s, offsetof(MotorData, speed), 0);
				can_controller::sendMotorSingleValue(s, offsetof(MotorData, isStop), true);
#else
				FocusMotor::stopStepper(s);
				FocusMotor::stopStepper(s);
#endif
				joystick_drive_Y = false;
			}
			if (s == Stepper::Z and joystick_drive_Z)
			{
#ifdef CAN_CONTROLLER
				can_controller::sendMotorSingleValue(s, offsetof(MotorData, speed), 0);
				can_controller::sendMotorSingleValue(s, offsetof(MotorData, isStop), true);
#else
				FocusMotor::stopStepper(s);
				FocusMotor::stopStepper(s);
#endif
				joystick_drive_Z = false;
			}
			if (s == Stepper::A and joystick_drive_A)
			{
#ifdef CAN_CONTROLLER
				can_controller::sendMotorSingleValue(s, offsetof(MotorData, speed), 0);
				can_controller::sendMotorSingleValue(s, offsetof(MotorData, isStop), true);
#else
				FocusMotor::stopStepper(s);
				FocusMotor::stopStepper(s);
#endif
				joystick_drive_A = false;
			}
		}
	}

	void xyza_changed_event(int x, int y, int z, int a)
	{
		// log_i("xyza_changed_event x:%d y:%i z:%i a:%i", x,y,z,a);
		//  Only allow motion in one direction at a time
		bool zIsRunning = FocusMotor::getData()[Stepper::Z]->isforever;
		bool aIsRunning = FocusMotor::getData()[Stepper::A]->isforever;

		if (!aIsRunning || FocusMotor::getDualAxisZ())
		{ // Z-direction
			handleAxis(z, Stepper::Z);
			if (FocusMotor::getDualAxisZ())
			{ // Z-Direction
				handleAxis(z, Stepper::A);
			}
		}
		if (abs(z) < offset_val)
		{
			// force stop in case it's trapped
			handleAxis(0, Stepper::Z);
		}
		if (abs(z) < offset_val && FocusMotor::getDualAxisZ())
		{
			// force stop in case it's trapped
			handleAxis(0, Stepper::A);
		}

		if (!zIsRunning && !FocusMotor::getDualAxisZ())
		{ // A-direction
			handleAxis(a, Stepper::A);
		}

		// X-Direction
		handleAxis(x, Stepper::X);

		// Y-direction
		handleAxis(y, Stepper::Y);
	}
}