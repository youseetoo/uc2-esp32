#pragma once


const int32_t MAX_VELOCITY_A = 20000;
const int32_t MAX_ACCELERATION_A = 600000;
const int32_t DEFAULT_ACCELERATION = 100000;

#pragma pack(push,1)
struct MotorState {
	int32_t currentPosition = 0;
	bool isRunning = 0;
	uint8_t axis = 0;
	//bool isForever = 0;
};
#pragma pack(pop)


#pragma pack(push,1)
struct MotorAction
{
	// Runtime motor commands - sent frequently via CAN/I2C
	int32_t targetPosition = 0;
	int32_t speed = 0;
	bool isforever = false;
	bool absolutePosition = false;
	bool isStop = false;
}__attribute__((packed));
#pragma pack(pop)

// Legacy alias for backwards compatibility
typedef MotorAction MotorDataReduced;


#pragma pack(push,1)
struct MotorSettings
{
	// Motor configuration settings - sent once or rarely
	bool directionPinInverted = false;
	bool joystickDirectionInverted = false;
	bool isaccelerated = false;
	bool isEnable = true;
	int32_t maxspeed = 200000;
	int32_t acceleration = 0;
	
	// Trigger settings
	bool isTriggered = false;
	int32_t offsetTrigger = 0;
	int32_t triggerPeriod = -1;
	int triggerPin = -1;
	
	// Pin configuration (can be removed if handled via PinConfig)
	int dirPin = -1;
	int stpPin = -1;
	
	// Soft limits
	uint32_t maxPos = 0;
	uint32_t minPos = 0;
	bool softLimitEnabled = false;
	
	// Advanced features
	bool encoderBasedMotion = false;
}__attribute__((packed));
#pragma pack(pop)



#pragma pack(push,1)
struct MotorData
{
	// Runtime action fields
	int32_t speed = 0;
	int32_t targetPosition = 0;
	int32_t currentPosition = 0;
	int isforever = false;
	bool absolutePosition = false;
	bool isStop = false;
	int qid = -1;
	
	// Internal state
	bool stopped = true;
	bool endstop_hit = false;
	bool isHoming = false; // homing in progress - ignore soft limits
	
	// Settings - these should eventually be moved to MotorSettings
	bool directionPinInverted = false;
	bool joystickDirectionInverted = false;
	bool isaccelerated = false;
	bool isEnable = true;
	int32_t maxspeed = 200000;
	int32_t acceleration = 0;
	bool isTriggered = false;
	int32_t offsetTrigger = 0;
	int32_t triggerPeriod = -1;
	int triggerPin = -1;
	int dirPin = -1;
	int stpPin = -1;
	uint32_t maxPos = 0;
	uint32_t minPos = 0;
	bool softLimitEnabled = false;
	bool encoderBasedMotion = false;
	
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
