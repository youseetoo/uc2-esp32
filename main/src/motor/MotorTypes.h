#pragma once


const int32_t MAX_VELOCITY_A = 20000;
const int32_t MAX_ACCELERATION_A = 60000;
const int32_t DEFAULT_ACCELERATION = 40000;

#pragma pack(push,1)
struct MotorState {
	int32_t currentPosition = 0;
	bool isRunning = 0;
	uint8_t axis = 0;
	//bool isForever = 0;
};
#pragma pack(pop)


#pragma pack(push,1)
struct MotorDataReduced
{
	// a stripped down version of MotorData to be sent over I2C
	int32_t targetPosition = 0;
	int32_t speed = 0;
	bool isforever = false;
	bool absolutePosition = false;
	bool isStop = false;
}__attribute__((packed));
#pragma pack(pop)


#pragma pack(push,1)
struct MotorData
{
	bool directionPinInverted = false;
	int32_t speed = 0;
	int32_t maxspeed = 200000;
	int32_t acceleration = 0;
	int32_t targetPosition = 0;
	int32_t currentPosition = 0;
	int isforever = false;
	bool isaccelerated = false;
	// running relative or aboslute position! gets ignored when isforever is true
	bool absolutePosition = false; 	
	bool isEnable = true; // keeping motor on after job is completed?
	int qid = -1;
	bool isStop = false; // stop motor or not

	//flag that indicate if a motor is realy availible and not just a motor wiht no function.
	//on earlier implementation, motors with no pin where nullptrs but now all motors gets initialized
	//and its needed to show only true initialized motors inside webui and android app.
	bool isActivated = 0;
	//internal state used by fast/accel stepper, maybe it shoulde get renamed to isRunning^^ but that need also to invert all true false values
	bool stopped = true;
	bool endstop_hit = false;

	// for triggering frame or lineclock
	bool isTriggered = false; // state if we send a pulse
	int32_t offsetTrigger = 0;	// offset in steps
	int32_t triggerPeriod = -1; // give a pulse every n steps
	int triggerPin = -1;	 // pin to trigger (0,1,2 - depends on pinConfig)
	int dirPin = -1;
	int stpPin = -1;
	
}__attribute__((packed));
#pragma pack(pop)


// Common minimal struct
#pragma pack(push,1)
struct MotorDataValueUpdate {
  uint16_t offset;   // e.g. offsetof(MotorData, isforever)
  int32_t  value;    // We'll interpret as bool on the other side
};
#pragma pack(pop)



enum Stepper
{
	A =0,
	X,
	Y,
	Z, 
	B, 
	C,
	D,
	E,
	F,
	G
};
