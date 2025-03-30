#include "MotorGamePad.h"
#include "FocusMotor.h"
#include "MotorTypes.h"
#ifdef CAN_CONTROLLER
#include "../can/can_controller.h"
#endif


namespace MotorGamePad
{
    bool joystick_drive_X = false;
	bool joystick_drive_Y = false;
	bool joystick_drive_Z = false;
	bool joystick_drive_A = false;
	int offset_val = 1025;

	void handleAxis(int value, int s)
	{

		if (s == Stepper::Z or (s == Stepper::A && FocusMotor::getDualAxisZ()))
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
			FocusMotor::getData()[s]->speed = value;
			FocusMotor::getData()[s]->isforever = true;
			FocusMotor::getData()[s]->acceleration = MAX_ACCELERATION_A;
			//log_i("Start motor from BT %i with speed %i", s, getData()[s]->speed);
			#ifdef CAN_CONTROLLER 
			can_controller::sendMotorSingleValue(s, offsetof(MotorData, speed), value);
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
		//log_i("xyza_changed_event x:%d y:%i z:%i a:%i", x,y,z,a);
		// Only allow motion in one direction at a time
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