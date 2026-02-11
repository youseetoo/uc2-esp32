#include <PinConfig.h>
#include "HomeMotor.h"
#include "../digitalin/DigitalInController.h"
#include "../config/ConfigController.h"
#include "../motor/MotorEncoderConfig.h"
#include "HardwareSerial.h"
#include "../../cJsonTool.h"
#include "Arduino.h"
#include "../../JsonKeys.h"
#include "../motor/MotorTypes.h"
#include "../motor/FocusMotor.h"
#include "../motor/MotorJsonParser.h"
#ifdef LINEAR_ENCODER_CONTROLLER
#include "../encoder/LinearEncoderController.h"
#endif
#ifdef CAN_BUS_ENABLED
#include "../can/can_controller.h"
#endif
using namespace FocusMotor;


namespace HomeMotor
{

	// Helper function to determine if an axis should use CAN in hybrid mode
	// IMPORTANT: Pin check must match FAccelStep::setupFastAccelStepper() which uses >= 0
	// A pin value of 'disabled' (-1) means no native driver, GPIO_NUM_0 (=0) IS a valid pin!
	bool shouldUseCANForAxis(int axis)
	{
#if defined(CAN_BUS_ENABLED) && defined(CAN_SEND_COMMANDS) && defined(CAN_HYBRID)
		// In hybrid mode: axes >= threshold use CAN, axes < threshold use native drivers
		// Check if this axis has a native driver configured
		// NOTE: Use >= 0 because GPIO_NUM_0 is a valid pin, -1 (disabled) means no driver
		bool hasNativeDriver = false;
		switch(axis) {
			case Stepper::A: hasNativeDriver = (pinConfig.MOTOR_A_STEP >= 0); break;
			case Stepper::X: hasNativeDriver = (pinConfig.MOTOR_X_STEP >= 0); break;
			case Stepper::Y: hasNativeDriver = (pinConfig.MOTOR_Y_STEP >= 0); break;
			case Stepper::Z: hasNativeDriver = (pinConfig.MOTOR_Z_STEP >= 0); break;
			case Stepper::B: hasNativeDriver = (pinConfig.MOTOR_B_STEP >= 0); break;
			case Stepper::C: hasNativeDriver = (pinConfig.MOTOR_C_STEP >= 0); break;
			case Stepper::D: hasNativeDriver = (pinConfig.MOTOR_D_STEP >= 0); break;
			case Stepper::E: hasNativeDriver = (pinConfig.MOTOR_E_STEP >= 0); break;
			case Stepper::F: hasNativeDriver = (pinConfig.MOTOR_F_STEP >= 0); break;
			case Stepper::G: hasNativeDriver = (pinConfig.MOTOR_G_STEP >= 0); break;
			default: hasNativeDriver = false; break;
		}
		
		// If axis >= hybrid threshold AND no native driver, use CAN
		// If axis < hybrid threshold AND has native driver, use native
		// If axis >= hybrid threshold but has native driver, use native (hardware override)
		if (hasNativeDriver) {
			return false; // Use native driver regardless of axis number
		}
		// No native driver - use CAN if axis >= threshold
		return (axis >= pinConfig.HYBRID_MOTOR_CAN_THRESHOLD);
#elif defined(CAN_BUS_ENABLED) && defined(CAN_SEND_COMMANDS) && !defined(CAN_RECEIVE_MOTOR)
		// Pure CAN master (non-hybrid): all axes use CAN
		return true;
#else
		return false; // CAN not available or this is a slave
#endif
	}

	// Helper function to convert hybrid internal axis (4,5,6,7...) to CAN axis (0,1,2,3...)
	// In hybrid mode: internal axis 4 -> CAN axis 0 -> CAN address 10 (CAN_ID_MOT_A)
	int getCANAxisForHybrid(int axis)
	{
#if defined(CAN_BUS_ENABLED) && defined(CAN_SEND_COMMANDS) && defined(CAN_HYBRID)
		if (axis >= pinConfig.HYBRID_MOTOR_CAN_THRESHOLD)
		{
			return axis - pinConfig.HYBRID_MOTOR_CAN_THRESHOLD;
		}
#endif
		return axis;
	}

	HomeData *hdata[4] = {nullptr, nullptr, nullptr, nullptr};

	HomeData **getHomeData()
	{
		return hdata;
	}

	/*
	Handle REST calls to the HomeMotor module
	*/
	int act(cJSON *doc)
	{
		// {"task": "/home_act", "home": {"steppers": [{"stepperid":0, "home_timeout":10000, "home_speed":5000, "home_maxspeed":10000, "home_direction":1, "home_endstoppolarity":0", "home_e"}]}, "qid":1234}
		// {"task": "/home_act", "home": {"steppers": [{"stepperid":1, "home_timeout":10000, "home_speed":5000, "home_maxspeed":10000, "home_direction":1, "home_endstoppolarity":0", "precise":1}]}, "qid":1234}
		log_i("home_act_fct");
		// print the json
		char *out = cJSON_PrintUnformatted(doc);
		log_i("HomeMotor act %s", out);
		int qid = cJsonTool::getJsonInt(doc, "qid");

		// parse the home data and start homing
		// parseHomeData() calls startHome() which handles ALL routing:
		// - Native driver: creates FreeRTOS homing task
		// - CAN (hybrid): sends to CAN slave for remote axes, local task for native axes
		// - CAN (pure master): sends to CAN slave
		// Do NOT call runStepper() or sendHomeDataToCANDriver() here - that would
		// double-start the motor and override the homing task's motor configuration!
		uint8_t axis = parseHomeData(doc);

		return qid;
	}

	int parseHomeData(cJSON *doc)
	{
#ifdef HOME_MOTOR
		// get the home object that contains multiple axis and parameters
		cJSON *home = cJSON_GetObjectItem(doc, key_home);
		Stepper axis = static_cast<Stepper>(-1);
#ifdef MOTOR_CONTROLLER
		if (home != NULL)
		{
			cJSON *stprs = cJSON_GetObjectItem(home, key_steppers);
			log_i("HomeMotor act %s", stprs);
			if (stprs != NULL)
			{

				cJSON *stp = NULL;
				cJSON_ArrayForEach(stp, stprs)
				{
					log_i("HomeMotor act %s", stp);

					axis = static_cast<Stepper>(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint);
					int homeTimeout = cJsonTool::getJsonInt(stp, key_home_timeout);
					int homeSpeed = cJsonTool::getJsonInt(stp, key_home_speed);
					int homeMaxspeed = cJsonTool::getJsonInt(stp, key_home_maxspeed);
					int homeDirection = cJsonTool::getJsonInt(stp, key_home_direction);
					int homeEndStopPolarity = cJsonTool::getJsonInt(stp, key_home_endstoppolarity);
					int homeEndOffset = cJsonTool::getJsonInt(stp, key_home_endoffset);
					int qid = cJsonTool::getJsonInt(doc, "qid");
					
					// Check for encoder-based homing (precise=1 or enc=1 for backward compatibility)
					bool useEncoderHoming = MotorJsonParser::isEncoderPrecisionRequested(stp);
					
					if (useEncoderHoming) {
						log_i("Starting encoder-based homing for axis %d", axis);
						#ifdef LINEAR_ENCODER_CONTROLLER
						// Use stall-based homing with encoder feedback
						// Speed sign determines direction
						int homingSpeed = homeSpeed * homeDirection;
						
						log_i("Starting encoder-based homing for axis %d: speed=%d", axis, homingSpeed);
						
						// Call simplified homeAxis function directly
						LinearEncoderController::homeAxis(homingSpeed, axis);
						#else
						log_w("Encoder-based homing requested but LINEAR_ENCODER_CONTROLLER not available");
						// Fall back to regular homing
						startHome(axis, homeTimeout, homeSpeed, homeMaxspeed, homeDirection, homeEndStopPolarity, homeEndOffset, qid);
						#endif
					} else {
						// Standard endstop-based homing
						startHome(axis, homeTimeout, homeSpeed, homeMaxspeed, homeDirection, homeEndStopPolarity, homeEndOffset, qid);
					}
				}
			}
		}
#endif // MOTOR_CONTROLLER
#else 
int axis = 0;
#endif // HOME_CONTROLLER
		return axis;
	}

	// CNC-style homing task that runs in background
	// Phase 1: Fast to endstop
	// Phase 2: Retract fixed distance
	// Phase 3: Slow approach to endstop for precision
	void homingTaskFunction(void *parameter) {
		int axis = (int)(intptr_t)parameter;
		log_i("[Homing Task] Started for axis %d", axis);
		
		HomeData *hd = hdata[axis];
		MotorData *md = FocusMotor::getData()[axis];
		
		// Get endstop input mapping
		int endstopInput = 0;
		if (axis == Stepper::X) endstopInput = 1;
		else if (axis == Stepper::Y) endstopInput = 2;
		else if (axis == Stepper::Z) endstopInput = 3;
		else if (axis == Stepper::A) endstopInput = 4; // Dual Z uses same endstop as Z
		
		uint32_t phaseStartTime = millis();
		
		while (hd->homeIsActive) {
			// Check for timeout
			if (millis() - hd->homeTimeStarted > hd->homeTimeout) {
				log_e("[Homing Task] Axis %d timeout in phase %d", axis, hd->homingPhase);
				FocusMotor::stopStepper(axis);
				hd->homeIsActive = false;
				md->isHoming = false;  // Release global lock on timeout
				sendHomeDone(axis);
				break;
			}
			
			int endstopState = DigitalInController::getDigitalVal(endstopInput);
			bool endstopTriggered = (endstopState == hd->homeEndStopPolarity);
			
			switch (hd->homingPhase) {
				case 0: {  // Phase 0: Release endstop (if already triggered at start)
					log_i("[Homing Task] Axis %d Phase 0: Releasing endstop (moving opposite direction)", axis);
					// CRITICAL: Clear hard limit first to allow motor start
					FocusMotor::clearHardLimitTriggered(axis);
					
					// Move away from endstop in opposite direction
					md->isforever = true;
					md->targetPosition = 0;
					md->absolutePosition = false;
					md->speed = -hd->homeDirection * abs(hd->homeSpeed);
					md->maxspeed = abs(hd->homeSpeed);
					md->isEnable = 1;
					md->isaccelerated = 0;
					md->acceleration = MAX_ACCELERATION_A;
					md->isStop = 0;
					md->stopped = false;
					
					log_i("[Homing Task] Axis %d Phase 0: Starting motor speed=%d", axis, md->speed);
					FocusMotor::startStepper(axis, 0);
					
					// Retry loop: wait for motor to start, retry if needed
					bool phase0Started = false;
					for (int i = 0; i < 5; i++) {
						vTaskDelay(pdMS_TO_TICKS(50));
						if (FocusMotor::isRunning(axis)) {
							log_i("[Homing Task] Axis %d Phase 0: Motor running (attempt %d)", axis, i+1);
							phase0Started = true;
							break;
						}
						log_w("[Homing Task] Axis %d Phase 0: Motor not running, retry %d/5", axis, i+1);
						md->stopped = false;
						FocusMotor::startStepper(axis, 0);
					}
					
					if (phase0Started) {
						hd->homingPhase = 8;  // Wait for endstop release
						phaseStartTime = millis();
					} else {
						log_e("[Homing Task] Axis %d Phase 0: Motor start failed, aborting", axis);
						hd->homeIsActive = false;
					}
					break;
				}
				
				case 1: {  // Phase 1: Fast approach to endstop
					log_i("[Homing Task] Axis %d Phase 1: Fast approach", axis);
					
					// Set motor parameters for fast approach
					md->isforever = true;
					md->speed = hd->homeDirection * abs(hd->homeSpeed);
					md->maxspeed = abs(hd->homeSpeed);
					md->isEnable = 1;
					md->isaccelerated = 0;
					md->acceleration = MAX_ACCELERATION_A;
					md->isStop = 0;
					md->stopped = false;
					md->isHoming = true;
					FocusMotor::clearHardLimitTriggered(axis);
					
					log_i("[Homing Task] Axis %d Phase 1: Starting motor speed=%d", axis, md->speed);
					FocusMotor::startStepper(axis, 0);
					
					// Retry loop: same robust pattern as Phase 0
					bool phase1Started = false;
					for (int i = 0; i < 5; i++) {
						vTaskDelay(pdMS_TO_TICKS(50));
						if (FocusMotor::isRunning(axis)) {
							log_i("[Homing Task] Axis %d Phase 1: Motor running (attempt %d)", axis, i+1);
							phase1Started = true;
							break;
						}
						log_w("[Homing Task] Axis %d Phase 1: Motor not running, retry %d/5", axis, i+1);
						md->stopped = false;
						FocusMotor::startStepper(axis, 0);
					}
					
					if (phase1Started) {
						hd->homingPhase = 2;  // Wait for endstop
						phaseStartTime = millis();
					} else {
						log_e("[Homing Task] Axis %d Phase 1: Motor start failed, aborting", axis);
						hd->homeIsActive = false;
					}
					break;
				}
				
				case 2: {  // Phase 2: Wait for endstop trigger
					//Serial.println("2");
					if (endstopTriggered) {
						// Record position where endstop was hit
						hd->homeFirstHitPosition = md->currentPosition;
						log_i("[Homing Task] Phase 2: Axis %d endstop triggered at position %d", axis, hd->homeFirstHitPosition);
						
						// Stop motor
						FocusMotor::stopStepper(axis);
						vTaskDelay(pdMS_TO_TICKS(100));  // Let motor settle
						
						hd->homingPhase = 3;  // Move to retract phase
						phaseStartTime = millis();
					}
					// Continue waiting for endstop
					break;
				}
				
				case 3: {  // Phase 3: Retract fixed distance
					//Serial.println("3");
					// Move away from endstop by fixed distance
					md->isforever = false;
					md->targetPosition = -hd->homeDirection * hd->homeRetractDistance;  // Opposite direction // TODO: This does not switch direction it seems 
					md->absolutePosition = false;  // Relative move
					md->speed = abs(hd->homeSpeed);
					md->maxspeed = abs(hd->homeSpeed);
					md->isEnable = 1;
					md->isaccelerated = 1;
					md->acceleration = MAX_ACCELERATION_A;
					md->isStop = 0;
					md->stopped = false;
					FocusMotor::startStepper(axis, 0);
					log_i("[Homing Task] Axis %d Phase 3: Retracting %d steps", axis, md->targetPosition);
					
					
					hd->homingPhase = 4;  // Move to waiting for retract completion
					phaseStartTime = millis();
					break;
				}
				
				case 4: {  // Phase 4: Wait for retract to complete
					//Serial.println("4");
					// Wait at least 100ms before accepting !isRunning as "completed"
					// Prevents race where isRunning() returns false before motor actually starts
					if ((millis() - phaseStartTime > 100) && !FocusMotor::isRunning(axis)) {
						log_i("[Homing Task] Phase 4: Axis %d retract complete, starting slow approach", axis);
						vTaskDelay(pdMS_TO_TICKS(100));  // Brief pause
						hd->homingPhase = 5;
						phaseStartTime = millis();
					}
					break;
				}
				
				case 5: {  // Phase 5: Slow approach to endstop
					//Serial.println("5");
					// Move slowly back toward endstop
					md->isforever = true;
					md->speed = hd->homeDirection * abs(hd->homeSpeed/4);
					md->maxspeed = abs(hd->homeSpeed/4);  // maxspeed must always be positive
					md->isEnable = 1;
					md->isaccelerated = 0;
					md->acceleration = MAX_ACCELERATION_A;
					md->isStop = 0;
					md->stopped = false;
					FocusMotor::startStepper(axis, 0);
					log_i("[Homing Task] Axis %d Phase 5: Slow approach (speed=%d)", axis, md->speed);
					
					hd->homingPhase = 6;  // Move to waiting for final trigger
					phaseStartTime = millis();
					break;
				}
				
				case 6: {  // Phase 6: Wait for final endstop trigger
					//Serial.println("6");
					if (endstopTriggered) {
						
						// Stop motor
						FocusMotor::stopStepper(axis);
						vTaskDelay(pdMS_TO_TICKS(200));  // Let motor settle
						
						// Set position to zero at home
						FocusMotor::setPosition(static_cast<Stepper>(axis), 0);
						
						md->isforever = false;
						
						hd->homingPhase = 7;  // Move to completion
						phaseStartTime = millis();
						log_i("[Homing Task] Phase 6: Axis %d final position reached, stopping", axis);
					}
					break;
				}
				
				case 7: {  // Phase 7: Check if final offset move is needed
					log_i("[Homing Task] Phase 7: Axis %d homing complete", axis);
					
					// Check if we need to move to final position with offset
					if (hd->homeEndOffset != 0) {
						log_i("[Homing Task] Phase 7: Axis %d moving to final offset %d", axis, hd->homeEndOffset);
						hd->homingPhase = 11;  // Move to Phase 11: final positioning
						phaseStartTime = millis();
					} else {
						// No offset, complete immediately
						log_i("[Homing Task] Phase 7: Axis %d no offset, completing", axis);
						hd->homingPhase = 12;  // Move to final completion phase
						phaseStartTime = millis();
					}
					break;
				}
				
				case 11: {  // Phase 11: Move to final position with offset
					log_i("[Homing Task] Phase 11: Axis %d moving %d steps from home", axis, hd->homeEndOffset);
					
					// Move to final position relative to home (0)
					md->isforever = false;
					md->targetPosition = hd->homeEndOffset;
					md->absolutePosition = false;  // Relative move from current position (0)
					md->speed = abs(hd->homeSpeed);
					md->maxspeed = abs(hd->homeSpeed);
					md->isEnable = 1;
					md->isaccelerated = 1;
					md->acceleration = MAX_ACCELERATION_A;
					md->isStop = 0;
					md->stopped = false;
					FocusMotor::startStepper(axis, 0);
					
					hd->homingPhase = 12;  // Wait for final move completion
					phaseStartTime = millis();
					break;
				}
				
				case 12: {  // Phase 12: Wait for final offset move to complete
					// Wait at least 100ms before accepting !isRunning as completed
					if ((millis() - phaseStartTime > 100) && !FocusMotor::isRunning(axis)) {
						log_i("[Homing Task] Phase 12: Axis %d final position reached", axis);
						
						// Send position update
						FocusMotor::sendMotorPos(axis, 0);
						
						// Send completion message
						sendHomeDone(axis);
						
						// COMPREHENSIVE cleanup: reset ALL motor state flags that homing modified.
						// This is critical because subsequent motor commands (especially via
						// MotorDataReduced/CAN) may not set these fields, and stale values
						// from homing will block motor start.
						hd->homeIsActive = false;
						hd->homingPhase = 0;
						md->isHoming = false;
						md->hardLimitTriggered = false;
						md->isforever = false;
						md->isStop = false;
						md->stopped = true;  // Motor is stopped after homing
						md->isEnable = true;
						
						log_i("[Homing Task] Phase 12: Axis %d cleanup done", axis);
					}
					break;
				}
				
case 8: {  // Phase 8: Wait for endstop to be released (for Phase 0 only)
					//Serial.println("8");
						if (!endstopTriggered) {
							log_i("[Homing Task] Axis %d endstop released, moving safety distance", axis);
							
							// Stop motor
							FocusMotor::stopStepper(axis);
							vTaskDelay(pdMS_TO_TICKS(100));
							
							hd->homingPhase = 9;  // Move to safety distance phase
							phaseStartTime = millis();
						}
						// Keep moving until endstop is released
						break;
					}
					
					case 9: {  // Phase 9: Move additional safety distance after endstop release
						//Serial.println("9");
						log_i("[Homing Task] Axis %d Phase 9: Moving safety distance (2000 steps)", axis);
						// Move additional 2000 steps away for safety
						md->isforever = false;
						// Additional safety distance in the opposite direction of homing (same direction as retract)
						md->targetPosition = -hd->homeDirection * 2000;  
						md->absolutePosition = false;  // Relative move
						md->speed = abs(hd->homeSpeed);
						md->maxspeed = abs(hd->homeSpeed);
						md->isEnable = 1;
						md->isaccelerated = 1;
						md->acceleration = MAX_ACCELERATION_A;
						md->isStop = 0;
						md->stopped = false;
						FocusMotor::startStepper(axis, 0);

						
						hd->homingPhase = 10;  // Wait for safety move completion
						phaseStartTime = millis();
						break;
					}
					
					case 10: {  // Phase 10: Wait for safety distance move to complete
						// Wait at least 100ms before accepting !isRunning as "completed"
						//Serial.println("10");
						if ((millis() - phaseStartTime > 100) && !FocusMotor::isRunning(axis)) {
							log_i("[Homing Task] Axis %d safety distance complete, starting normal homing", axis);
							vTaskDelay(pdMS_TO_TICKS(100));
							hd->homingPhase = 1;  // Now start normal homing sequence
							phaseStartTime = millis();
						}
						break;
					}
					
					default:
					log_e("[Homing Task] Axis %d unknown phase %d", axis, hd->homingPhase);
					hd->homeIsActive = false;
					md->isHoming = false;
					break;
			}
			
			// Small delay to prevent task hogging CPU
			vTaskDelay(pdMS_TO_TICKS(10));
		}
		
		// Task cleanup - ensure all flags are cleared before exiting
		// This allows new homing commands to start after this task exits
		hd->homeIsActive = false;
		md->isHoming = false;  // CRITICAL: Release global lock
		homingTaskHandles[axis] = nullptr;
		log_i("[Homing Task] Axis %d task exiting and deleted", axis);
		vTaskDelete(nullptr);  // Delete self
	}

	void startHome(int axis, int homeTimeout, int homeSpeed, int homeMaxspeed, int homeDirection, int homeEndStopPolarity, int homeEndOffset, int qid)
	{
		// CRITICAL: Check if homing is already running BEFORE touching any state
		// Use getData()[axis]->isHoming as the single source of truth
		// This prevents race conditions when multiple homing commands arrive quickly
		#if defined(USE_ACCELSTEP) || defined(USE_FASTACCEL)
		if (getData()[axis]->isHoming) {
			log_w("Homing already active for axis %d, ignoring new command", axis);
			return;
		}
		#endif

		// Store variables in preferences for later use per axis 
		preferences.begin("home", false);
		preferences.putInt(("to_" + String(axis)).c_str(), homeTimeout);
		preferences.putInt(("hs_" + String(axis)).c_str(), homeSpeed);
		preferences.putInt(("hms_" + String(axis)).c_str(), homeMaxspeed);
		preferences.putInt(("hd_" + String(axis)).c_str(), homeDirection);
		preferences.putInt(("hep_" + String(axis)).c_str(), homeEndStopPolarity);
		preferences.putInt(("heo_" + String(axis)).c_str(), homeEndOffset);
		preferences.end();

		// Set the home data
		hdata[axis]->homeTimeout = homeTimeout;
		hdata[axis]->homeSpeed = homeSpeed;
		hdata[axis]->homeMaxspeed = homeMaxspeed;
		hdata[axis]->homeEndStopPolarity = homeEndStopPolarity;
		hdata[axis]->homeEndOffset = homeEndOffset;
		hdata[axis]->qid = qid;

		// Normalize direction to 1 or -1
		if (homeDirection >= 0)
		{
			hdata[axis]->homeDirection = 1;
		}
		else
		{
			hdata[axis]->homeDirection = -1;
		}
		// Normalize endstop polarity to 0 or 1
		if (hdata[axis]->homeEndStopPolarity > 0)
		{
			hdata[axis]->homeEndStopPolarity = 1;
		}
		else
		{
			hdata[axis]->homeEndStopPolarity = 0;
		}
		log_i("Start home for axis %i with timeout %i, speed %i, maxspeed %i, direction %i, endstop polarity %i", axis, homeTimeout, homeSpeed, homeMaxspeed, homeDirection, homeEndStopPolarity);
		
		// Set isHoming flag IMMEDIATELY to prevent concurrent homing attempts
		// This is the single source of truth - hdata[axis]->homeIsActive is for task control only
		getData()[axis]->isHoming = true;
		hdata[axis]->homeTimeStarted = millis();
		
		// Check initial endstop state to determine starting mode
		// If endstop is already pressed, start in release phase (Phase 0)
#if defined MOTOR_CONTROLLER && defined DIGITAL_IN_CONTROLLER
		int currentEndstopState = 0;
		if (axis == Stepper::X) currentEndstopState = DigitalInController::getDigitalVal(1);
		else if (axis == Stepper::Y) currentEndstopState = DigitalInController::getDigitalVal(2);
		else if (axis == Stepper::Z) currentEndstopState = DigitalInController::getDigitalVal(3);
		
		// Check if endstop is already triggered
		bool endstopAlreadyTriggered = (currentEndstopState == hdata[axis]->homeEndStopPolarity);
		if (endstopAlreadyTriggered) {
			log_i("Axis %i endstop already active, starting with release phase", axis);
			hdata[axis]->homingPhase = 0;  // Phase 0: Release endstop first
		} else {
			hdata[axis]->homingPhase = 1;  // Phase 1: Normal fast to endstop
		}
#else
		hdata[axis]->homingPhase = 1;  // Start with phase 1: fast to endstop
#endif
		
		// Determine whether to use native driver or CAN based on hybrid mode
		bool useCANForHoming = shouldUseCANForAxis(axis);
		
		if (useCANForHoming)
		{
#if defined(CAN_BUS_ENABLED) && !defined(CAN_RECEIVE_MOTOR)
			// In hybrid mode, send home data to CAN slave
			int canAxis = getCANAxisForHybrid(axis);
			log_i("Homing axis %d via CAN (CAN axis: %d)", axis, canAxis);
			can_controller::sendHomeDataToCANDriver(*hdata[axis], canAxis);
			// Clear isHoming on master - the slave manages its own homing state.
			// Without this, isHoming stays true forever on the master (no local
			// homing task to clear it), blocking all future homing commands.
			getData()[axis]->isHoming = false;
#endif
		}
		else
		{
#if defined(USE_ACCELSTEP) || defined(USE_FASTACCEL)
			// Use native driver with new CNC-style task-based homing
			
			// Stop any existing homing task for this axis - should not happen due to early check above
			// but handle it safely just in case
			if (homingTaskHandles[axis] != nullptr) {
				log_e("Homing task still exists for axis %d - this should not happen! Emergency cleanup.", axis);
				
				// Signal task to exit gracefully via homeIsActive flag
				// (isHoming is already set above to prevent new commands)
				hdata[axis]->homeIsActive = false;
				
				// Stop the motor to release mutex if held
				FocusMotor::stopStepper(axis);
				
				// Wait for task to actually exit (check handle becomes NULL)
				// Task sets homingTaskHandles[axis]=nullptr before vTaskDelete(nullptr)
				uint32_t waitStart = millis();
				while (homingTaskHandles[axis] != nullptr && (millis() - waitStart < 500)) {
					vTaskDelay(pdMS_TO_TICKS(10));
				}
				
				// If task still exists after 500ms, force delete (last resort)
				if (homingTaskHandles[axis] != nullptr) {
					log_e("Task did not exit gracefully, force deleting for axis %d", axis);
					vTaskDelete(homingTaskHandles[axis]);
					homingTaskHandles[axis] = nullptr;
				}
				
				// Reset all homing state
				getData()[axis]->hardLimitTriggered = false;
				
				// Give system time to fully clean up
				vTaskDelay(pdMS_TO_TICKS(100));
			}
			
			// Create new homing task
			log_i("Creating new homing task for axis %d", axis);
			char taskName[32];
			hdata[axis]->homeIsActive = true;  // Enable task loop
			snprintf(taskName, sizeof(taskName), "Homing_Axis_%d", axis);
			xTaskCreate(
				homingTaskFunction,      // Task function
				taskName,                  // Task name
				4096,                      // Stack size (bytes)
				(void*)(intptr_t)axis,    // Parameter: axis number
				5,                         // Priority (medium)
				&homingTaskHandles[axis]  // Task handle
			);
			
			if (homingTaskHandles[axis] == nullptr) {
				log_e("Failed to create homing task for axis %d", axis);
				hdata[axis]->homeIsActive = false;
				getData()[axis]->isHoming = false;  // Release lock on failure
			}
#endif
		}
	}

	void stopHome(int axis)
	{
		// Stop homing process for specified axis
		log_i("[stopHome] Stopping homing for axis %d", axis);
		
		if (axis < 0 || axis >= 4) {
			log_e("[stopHome] Invalid axis %d", axis);
			return;
		}
		
		if (!getData()[axis]->isHoming && !hdata[axis]->homeIsActive) {
			log_w("[stopHome] Axis %d not currently homing", axis);
			return;
		}
		
		// Signal the homing task to stop by clearing homeIsActive
		hdata[axis]->homeIsActive = false;
		
		// Stop motor immediately
		FocusMotor::stopStepper(axis);
		
		// Clear homing flags
		getData()[axis]->isHoming = false;
		getData()[axis]->hardLimitTriggered = false;
		hdata[axis]->homingPhase = 0;
		
		// Wait for task to terminate (max 200ms)
		unsigned long waitStart = millis();
		while (homingTaskHandles[axis] != nullptr && (millis() - waitStart < 200)) {
			vTaskDelay(pdMS_TO_TICKS(10));
		}
		
		log_i("[stopHome] Axis %d homing stopped", axis);
	}

	void runStepper(int s)
	{
		// Determine motor direction based on homing mode
		FocusMotor::clearHardLimitTriggered(s);

		// Configure and start the motor
		FocusMotor::getData()[s]->isforever = true;
		FocusMotor::getData()[s]->speed = hdata[s]->homeSpeed;
		FocusMotor::getData()[s]->maxspeed = abs(hdata[s]->homeSpeed);  // maxspeed must always be positive
		FocusMotor::getData()[s]->isEnable = 1;
		FocusMotor::getData()[s]->isaccelerated = 0;
		FocusMotor::getData()[s]->acceleration = MAX_ACCELERATION_A;
		FocusMotor::getData()[s]->isStop = 0;
		FocusMotor::getData()[s]->stopped = false;
		FocusMotor::startStepper(s, 0);
		
		delay(50); // give the motor some time to start
		log_i("Start stepper based on home data: Axis: %i, homeTimeout: %i, homeSpeed: %i, homeMaxSpeed: %i, homeDirection:%i, homeTimeStarted:%i, homeEndStopPolarity %i",
			  s, hdata[s]->homeTimeout, hdata[s]->homeDirection * hdata[s]->homeSpeed, hdata[s]->homeDirection * hdata[s]->homeSpeed,
			  hdata[s]->homeDirection, hdata[s]->homeTimeStarted, hdata[s]->homeEndStopPolarity);
	}

	cJSON *get(cJSON *ob)
	{
		log_i("home_get_fct");
		cJSON *doc = cJSON_CreateObject();
#ifdef MOTOR_CONTROLLER
		cJSON *home = cJSON_CreateObject();
		cJSON_AddItemToObject(doc, key_home, home);
		cJSON *arr = cJSON_CreateArray();
		cJSON_AddItemToObject(home, key_steppers, arr);

		// add the home data to the json

		for (int i = 0; i < 4; i++)
		{
			cJSON *arritem = cJSON_CreateObject();
			cJSON_AddItemToArray(arr, arritem);
			cJsonTool::setJsonInt(arritem, key_home_timeout, hdata[i]->homeTimeout);
			cJsonTool::setJsonInt(arritem, key_home_speed, hdata[i]->homeSpeed);
			cJsonTool::setJsonInt(arritem, key_home_maxspeed, hdata[i]->homeMaxspeed);
			cJsonTool::setJsonInt(arritem, key_home_direction, hdata[i]->homeDirection);
			cJsonTool::setJsonInt(arritem, key_home_timestarted, hdata[i]->homeTimeStarted);
			cJsonTool::setJsonInt(arritem, key_home_isactive, hdata[i]->homeIsActive);
		}

		log_i("home_get_fct done");
#endif
		return doc;
	}

	// home done returns
	//{"home":{...}}
	void sendHomeDone(int axis)
	{
#ifdef MOTOR_CONTROLLER
		// send home done to client
		cJSON *json = cJSON_CreateObject();
		cJSON *home = cJSON_CreateObject();
		cJSON_AddItemToObject(json, key_home, home);
		cJSON *steppers = cJSON_CreateObject();
		cJSON_AddItemToObject(home, key_steppers, steppers);
		cJSON *axs = cJSON_CreateNumber(axis);
		cJSON *done = cJSON_CreateNumber(true);
		cJSON *pos = cJSON_CreateNumber(FocusMotor::getData()[axis]->currentPosition);
		cJSON_AddItemToObject(steppers, "axis", axs);
		cJSON_AddItemToObject(steppers, "pos", pos);
		cJSON_AddItemToObject(steppers, "isDone", done);
		cJSON_AddItemToObject(json, keyQueueID, cJSON_CreateNumber(hdata[axis]->qid));
		cJsonTool::setJsonInt(json, keyQueueID, hdata[axis]->qid);
		//Serial.println("++");
		char *ret = cJSON_PrintUnformatted(json);
		cJSON_Delete(json);
		//Serial.println(ret);
		free(ret);
		//Serial.println("--");
#endif
#if defined(CAN_BUS_ENABLED) && defined(CAN_RECEIVE_MOTOR)
		// send home state to master
		HomeState homeState;
		homeState.isHoming = false;
		homeState.isHomed = true;
		homeState.currentPosition = FocusMotor::getData()[axis]->currentPosition;
		can_controller::sendHomeStateToMaster(homeState);
#endif
	}

	void checkAndProcessHome(Stepper s, int digitalin_val)
	{
#ifdef MOTOR_CONTROLLER
#if defined(CAN_BUS_ENABLED) && !defined(CAN_RECEIVE_MOTOR)
		// For CAN master, we receive push messages, only monitor timeout
		if (hdata[s]->homeIsActive and hdata[s]->homeTimeStarted + hdata[s]->homeTimeout < millis())
		{
			log_i("Home Motor %i timeout", s);
			sendHomeDone(s);
			hdata[s]->homeIsActive = false;
			getData()[s]->isHoming = false;  // Clear homing flag
			getData()[s]->hardLimitTriggered = false;  // Clear hard limit triggered flag after successful homing
		}
#else
		// For native drivers with USE_ACCELSTEP/USE_FASTACCEL, homing now runs in FreeRTOS task
		// This function is no longer needed for native drivers - task handles everything
		// Keep this empty block for compatibility
#endif
#endif
	}
	/*
		get called repeatedly, dont block this
	*/
	void loop()
	{

		// this will be called everytime, so we need to make this optional with a switch
		// get motor and switch instances

// expecting digitalin1 handling endstep for stepper X, digital2 stepper Y, digital3 stepper Z
//  0=A , 1=X, 2=Y , 3=Z
#if defined MOTOR_CONTROLLER && defined DIGITAL_IN_CONTROLLER
		checkAndProcessHome(Stepper::X, DigitalInController::getDigitalVal(1));
		checkAndProcessHome(Stepper::Y, DigitalInController::getDigitalVal(2));
		checkAndProcessHome(Stepper::Z, DigitalInController::getDigitalVal(3));
#endif
	}

	/*
	not needed all stuff get setup inside motor and digitalin, but must get implemented
	*/
	void setup()
	{
		log_i("HomeMotor setup");
		for (int i = 0; i < 4; i++)
		{
			hdata[i] = new HomeData();
		}
	}
}
