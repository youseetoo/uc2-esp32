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
/*#ifdef CAN_CONTROLLER
			can_controller::sendMotorSingleValue(s, offsetof(MotorData, speed), (int)motorSpeed);
			can_controller::sendMotorSingleValue(s, offsetof(MotorData, isforever), true);
			can_controller::sendMotorSingleValue(s, offsetof(MotorData, isStop), false);
#else*/
//#endif
			FocusMotor::startStepper(s, 1);

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

		// X-Direction
		handleAxis(x, Stepper::X);

		// Y-direction
		handleAxis(y, Stepper::Y);

		// Z-direction
		if (!aIsRunning)
			handleAxis(z, Stepper::Z);

		// A-direction
		if (!zIsRunning)
			handleAxis(a, Stepper::A);
		
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
}