#include "MotorGamePad.h"
#include "FocusMotor.h"
#include "MotorTypes.h"
#ifdef CAN_CONTROLLER
#include "../can/can_controller.h"
#endif

// sample pio project e.g. https://github.com/inventonater/flashbike-matrix/blob/master/scratch/tetris/tetris-matrix-s3.cpp

namespace MotorGamePad
{
constexpr int   kOffset         = 1025;      // joystick dead-zone
constexpr float kAlpha          = 0.40f;     // linear range boundary
constexpr float kMaxSpeed       = 2000.0f;   // base max speed
constexpr float kOneOver32768f  = 1.0f / 32768.0f;


	static inline void stopAxis(int ax)
	{
#ifdef CAN_CONTROLLER
		can_controller::sendMotorSingleValue(ax, offsetof(MotorData, speed), 0);
		can_controller::sendMotorSingleValue(ax, offsetof(MotorData, isStop), true);
#else
		FocusMotor::stopStepper(ax);
#endif
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
		if (ax == Stepper::A)
		return;
		// dead-zone ────────────────────────────────────────────────────────────
		if (std::abs(value) <= kOffset)
		{
			if (axisRunning[ax])
				stopAxis(ax);
			return;
		}

		// Z⇄A mutual exclusion ────────────────────────────────────────────────
		if (0)
		{ // TODO: Problem might be that they cancel each other out if there is not return from CAN
			if (ax == Stepper::Z && axisRunning[Stepper::A])
				stopAxis(Stepper::A);
			if (ax == Stepper::A && axisRunning[Stepper::Z])
				stopAxis(Stepper::Z);
		}
		// speed computation ───────────────────────────────────────────────────
		float speed = curve(value) * kMaxSpeed;

		// per-axis scaling
		speed *= (ax == Stepper::Z || (ax == Stepper::A && FocusMotor::getDualAxisZ()))
					 ? pinConfig.JOYSTICK_SPEED_MULTIPLIER_Z
					 : pinConfig.JOYSTICK_SPEED_MULTIPLIER;

		startAxis(ax, static_cast<int>(speed));

		log_i("Motor %d: raw=%d  speed=%f", ax, value, speed);
	}
	void xyza_changed_event(int x, int y, int z, int a)
	{
		// log_i("xyza_changed_event x:%d y:%i z:%i a:%i", x,y,z,a);

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