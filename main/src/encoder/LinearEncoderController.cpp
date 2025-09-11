#include "LinearEncoderController.h"
#include "../digitalin/DigitalInController.h"
#include "../config/ConfigController.h"
#include "../motor/MotorEncoderConfig.h"
#include "HardwareSerial.h"
#include <Preferences.h>
#ifdef MOTOR_CONTROLLER
#include "../motor/FocusMotor.h"
#endif

#include "InterruptController.h"
#include "PCNTEncoderController.h"
#include "JsonKeys.h"

using namespace FocusMotor;
namespace LinearEncoderController
{

    // function pointer to motordata from different possible targets
    MotorData **(*getData)();
    // function pointer to start a stepper
    void (*startStepper)(int, int);

    std::array<LinearEncoderData *, 4> edata;
    // std::array<AS5311 *, 4> encoders;
    std::array<AS5311AB, 4> encoders;

    // TODO: Probalby, we should move the interrupt based encoder handling to a separate class and unify the interface with PCNTEncoderController
    
    void processEncoderEvent(uint8_t pin)
    {
        if ((pin == pinConfig.ENC_X_B))
        { // X-A changed
            bool pinA = digitalRead(pinConfig.ENC_X_A);
            bool pinB = digitalRead(pinConfig.ENC_X_B);
            if ((pinA == pinB) ^ edata[1]->encoderDirection)
                edata[1]->stepCount++; // Update integer steps first for accuracy
            else
                edata[1]->stepCount--;
        }
        else if (pin == pinConfig.ENC_X_A)
        { // X-B changed
            bool pinA = digitalRead(pinConfig.ENC_X_A);
            bool pinB = digitalRead(pinConfig.ENC_X_B);

            if ((pinA != pinB) ^ edata[1]->encoderDirection)
                edata[1]->stepCount++;
            else
                edata[1]->stepCount--;
        }
        else if (pin == pinConfig.ENC_Y_A)
        { // Y-A changed
            bool pinA = digitalRead(pinConfig.ENC_Y_A);
            bool pinB = digitalRead(pinConfig.ENC_Y_B);

            if ((pinA == pinB) ^ edata[2]->encoderDirection)
                edata[2]->stepCount++;
            else
                edata[2]->stepCount--;
        }
        else if (pin == pinConfig.ENC_Y_B)
        { // Y-B changed
            bool pinA = digitalRead(pinConfig.ENC_Y_A);
            bool pinB = digitalRead(pinConfig.ENC_Y_B);

            if ((pinA != pinB) ^ edata[2]->encoderDirection)
                edata[2]->stepCount++;
            else
                edata[2]->stepCount--;
        }
        else if (pin == pinConfig.ENC_Z_A)
        { // Z-A changed
            bool pinA = digitalRead(pinConfig.ENC_Z_A);
            bool pinB = digitalRead(pinConfig.ENC_Z_B);
            if ((pinA == pinB) ^ edata[3]->encoderDirection)
                edata[3]->stepCount++;
            else
                edata[3]->stepCount--;
        }
        else if (pin == pinConfig.ENC_Z_B)
        { // Z-B changed
            bool pinA = digitalRead(pinConfig.ENC_Z_A);
            bool pinB = digitalRead(pinConfig.ENC_Z_B);

            if ((pinA != pinB) ^ edata[3]->encoderDirection)
                edata[3]->stepCount++;
            else
                edata[3]->stepCount--;
        }
    }

    /*
    Handle REST calls to the LinearEncoderController module
    */
    int act(cJSON *j)
    {
        log_i("linearencoder_act_fct");
        int qid = cJsonTool::getJsonInt(j, "qid");

        // calibrate the step-to-mm value
        cJSON *movePrecise = cJSON_GetObjectItem(j, key_linearencoder_moveprecise);
        cJSON *home = cJSON_GetObjectItem(j, key_linearencoder_home);
        cJSON *plot = cJSON_GetObjectItem(j, key_linearencoder_plot);

        if (plot != NULL)
        {
            /*******
             * PLOT THE ENCODER VALUES in the loop -> set flag true/false
             *******/
            // {"task": "/linearencoder_act", "plot": 1}

            // get plot true/false
            isPlot = (plot->valueint == 1);
        }
        else if (home != NULL)
        /*******
         * HOME THE LINEARENCODER
         ********/
        {
#ifdef HOME_MOTOR
            // we want to start a motor until the linear encoder does not track any position change
            //{"task": "/linearencoder_act", "home": {"steppers": [ { "stepperid": 1,  "speed": -40000} ]}}
            cJSON *stprs = cJSON_GetObjectItem(home, key_steppers);
            if (stprs != NULL)
            {
                // HOMING THE LINEARENCODER
                cJSON *stp = NULL;
                cJSON_ArrayForEach(stp, stprs)
                {
                    Stepper s = static_cast<Stepper>(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint);
                    // measure current value
                    edata[s]->positionPreMove = getCurrentPosition(s);
                    log_i("pre home %f", edata[s]->positionPreMove);
                    int speed = cJSON_GetObjectItemCaseSensitive(stp, key_speed)->valueint;
                    
                    // Execute homing immediately for fast response times
                    executeHomingBlocking(s, speed);
                }
            }
#endif
        }
        else if (movePrecise != NULL)
        {
            /*******
             * MOVE THE LINEARENCODER TO A PRECISE POSITION
             *******/
            // initiate a motor start and let the motor run until it reaches the position
            /*
             {"task": "/linearencoder_get", "stepperid": 1}
            {"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 1, "position": 20000, "isabs":1,  "cp":10, "ci":0., "cd":0, "speed":20000} ]}}
            {"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 1, "position": 500, "isabs":0,  "cp":100, "ci":0., "cd":10} ]}}
            {"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 1, "position": 1500 , "isabs":0, "cp":20, "ci":1, "cd":0.5} ]}}
            {"task":"/linearencoder_get", "linencoder": { "posval": 1,    "id": 1  }}
            {"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 2, "position": 5000 , "isabs":0, "speed": 2000, "cp":20, "ci":10, "cd":5, "encdir":1, "motdir":0, "res":1} ]}}
            {"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 2, "position": 5000 , "isabs":0, "speed": 2000, "cp":20, "ci":10, "cd":5, "encdir":1, "motdir":0, "res":1} ]}}
            {"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 1, "position": 0 , "isabs":1, "speed": -10000, "cp":10, "ci":10, "cd":10} ]}}
            {"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 1, "position": 10000 , "cp":40, "ci":1, "cd":10} ]}}
            */
            // TODO: We need to retreive the PCNT value here as well to have a better starting point
            cJSON *stprs = cJSON_GetObjectItem(movePrecise, key_steppers);
            if (stprs != NULL)
            {
                cJSON *stp = NULL;
                cJSON_ArrayForEach(stp, stprs) // TODO: We only want to do that for one axis (i.e. x == 1)
                {
                    int s = cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint;
                    // measure current value
                    edata[s]->positionPreMove = getCurrentPosition(s);

                    // retreive abs/rel flag
                    if (cJSON_GetObjectItemCaseSensitive(stp, key_isabs) != NULL)
                        edata[s]->isAbsolute = cJSON_GetObjectItemCaseSensitive(stp, key_isabs)->valueint;
                    else
                        edata[s]->isAbsolute = true;

                    // retreive position to go
                    int posToGo = 0;
                    if (cJSON_GetObjectItemCaseSensitive(stp, key_linearencoder_position) != NULL)
                        posToGo = cJSON_GetObjectItemCaseSensitive(stp, key_linearencoder_position)->valueint;

                    // calculate the position to go
                    float distanceToGo = posToGo;
                    if (!edata[s]->isAbsolute)
                        distanceToGo += edata[s]->positionPreMove; // TODO: Check if this is in correct units?
                    edata[s]->positionToGo = distanceToGo;

                    // PID Controller
                    edata[s]->c_p = 10.;
                    edata[s]->c_i = 0.5;
                    edata[s]->c_d = .0;
                    if (cJSON_GetObjectItemCaseSensitive(stp, key_linearencoder_cp) != NULL)
                        edata[s]->c_p = cJSON_GetObjectItemCaseSensitive(stp, key_linearencoder_cp)->valuedouble;
                    if (cJSON_GetObjectItemCaseSensitive(stp, key_linearencoder_ci) != NULL)
                        edata[s]->c_i = cJSON_GetObjectItemCaseSensitive(stp, key_linearencoder_ci)->valuedouble;
                    if (cJSON_GetObjectItemCaseSensitive(stp, key_linearencoder_cd) != NULL)
                        edata[s]->c_d = cJSON_GetObjectItemCaseSensitive(stp, key_linearencoder_cd)->valuedouble;
                    if (cJSON_GetObjectItemCaseSensitive(stp, key_speed) != NULL)
                        edata[s]->maxSpeed = abs(cJSON_GetObjectItemCaseSensitive(stp, key_speed)->valueint);
                    if (cJSON_GetObjectItemCaseSensitive(stp, "encdir") != NULL)
                        edata[s]->encoderDirection = abs(cJSON_GetObjectItemCaseSensitive(stp, "encdir")->valueint);
                    if (cJSON_GetObjectItemCaseSensitive(stp, "motdir") != NULL)
                    {
                        getData()[s]->directionPinInverted = abs(cJSON_GetObjectItemCaseSensitive(stp, "motdir")->valueint);
                        // TODO: We need to store this permanently in the preferences for this very motor
                    }

                    edata[s]->pid = PIDController(edata[s]->c_p, edata[s]->c_i, edata[s]->c_d);
                    // Set output limits based on maximum speed
                    edata[s]->pid.setOutputLimits(-edata[s]->maxSpeed, edata[s]->maxSpeed);

                    float speed = edata[s]->pid.compute(edata[s]->positionToGo, getCurrentPosition(s));

                    // Speed is already limited by PID controller output limits
                    // No need for additional clamping unless we want stricter limits

                    log_d("Move precise from %f to %f at motor speed %f, computed speed %f, encoderDirection %f", edata[s]->positionPreMove, edata[s]->positionToGo, getData()[s]->speed, speed, edata[s]->encoderDirection);
                    
                    // Execute precision motion control immediately for fast response times
                    // This replaces the slow loop-based polling with immediate execution
                    executePrecisionMotionBlocking(s);
                }
            }
            else
            {
                log_e("unknown command");
            }
        }

        // Handle encoder configuration settings - simplified to essential parameters only
        // {"task": "/linearencoder_act", "config": {"stepsToEncoderUnits": 0.3125}}
        // {"task": "/linearencoder_act", "config": {"steppers": [{"stepperid": 1, "encdir": 1, "motdir": 0}]}}
        cJSON *config = cJSON_GetObjectItem(j, "config");
        if (config != NULL)
        {
            // Handle global motor-encoder conversion factor configuration (primary parameter)
            cJSON *stepsToEncUnits = cJSON_GetObjectItem(config, "stepsToEncoderUnits");
            if (stepsToEncUnits != NULL && cJSON_IsNumber(stepsToEncUnits))
            {
                float conversionFactor = (float)stepsToEncUnits->valuedouble;
                MotorEncoderConfig::setStepsToEncoderUnits(conversionFactor);
                MotorEncoderConfig::saveToPreferences();
                log_i("Global conversion factor set to: %f step per step", conversionFactor);
            }

            // Handle per-axis direction configuration (simplified)
            cJSON *stprs = cJSON_GetObjectItem(config, key_steppers);
            if (stprs != NULL)
            {
                cJSON *stp = NULL;
                cJSON_ArrayForEach(stp, stprs)
                {
                    int s = cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint;

                    // Set encoder direction (simplified - sign can be incorporated into global conversion factor)
                    if (cJSON_GetObjectItemCaseSensitive(stp, "encdir") != NULL)
                    {
                        bool encDir = cJSON_GetObjectItemCaseSensitive(stp, "encdir")->valueint;
                        edata[s]->encoderDirection = encDir;
                        log_i("Set encoder direction for axis %d: %s", s, encDir ? "forward" : "reverse");

                        // Store in preferences
                        Preferences preferences;
                        preferences.begin("UC2_ENC", false);
                        preferences.putBool(("encdir" + String(s)).c_str(), encDir);
                        preferences.end();
                    }

                    // Set motor direction 
                    if (cJSON_GetObjectItemCaseSensitive(stp, "motdir") != NULL)
                    {
                        bool motDir = cJSON_GetObjectItemCaseSensitive(stp, "motdir")->valueint;
                        getData()[s]->directionPinInverted = motDir;
                        log_i("Set motor direction for axis %d: %s", s, motDir ? "inverted" : "normal");

                        // Store in preferences
                        Preferences preferences;
                        preferences.begin("UC2", false);
                        String motdirKey = "";
                        switch (s)
                        {
                        case 0: motdirKey = "motainv"; break; // A axis
                        case 1: motdirKey = "motxinv"; break; // X axis
                        case 2: motdirKey = "motyinv"; break; // Y axis
                        case 3: motdirKey = "motzinv"; break; // Z axis
                        default: break;
                        }
                        if (motdirKey != "")
                        {
                            preferences.putBool(motdirKey.c_str(), motDir);
                        }
                        preferences.end();
                    }

                    log_i("Encoder configuration updated for axis %d", s);
                }
            }
        }

        return qid;
    }

    void setCurrentPosition(int encoderIndex, float offsetPos)
    {
        if (encoderIndex < 0 || encoderIndex >= 4)
            return;

        // Use selected encoder interface
        if (edata[encoderIndex]->encoderInterface == ENCODER_PCNT_BASED && PCNTEncoderController::isPCNTAvailable())
        {
            PCNTEncoderController::setCurrentPosition(encoderIndex, offsetPos);
        }
        else
        {
            // Convert position back to step counts using global conversion factor
            float globalConversionFactor = MotorEncoderConfig::getStepsToEncoderUnits();
            edata[encoderIndex]->stepCount = (long)(offsetPos / globalConversionFactor);
            // Update posval for compatibility
            edata[encoderIndex]->posval = offsetPos;
        }
    }

    float getCurrentPosition(int encoderIndex)
    {
        if (encoderIndex < 0 || encoderIndex >= 4)
            return 0.0f;

        // Use selected encoder interface
        if (edata[encoderIndex]->encoderInterface == ENCODER_PCNT_BASED && PCNTEncoderController::isPCNTAvailable())
        {
            return PCNTEncoderController::getCurrentPosition(encoderIndex);
        }
        else
        {
            // Apply global conversion factor to step counts for accurate position
            float globalConversionFactor = MotorEncoderConfig::getStepsToEncoderUnits();
            float position = edata[encoderIndex]->stepCount * globalConversionFactor;
            // Update posval for compatibility with existing code
            edata[encoderIndex]->posval = position;
            return position;
        }
    }

    cJSON *get(cJSON *docin)
    {
        // {"task":"/linearencoder_get"}
        // {"task":"/linearencoder_get", "linencoder": { "posval": 1,    "id": 2  }}
        // {"task":"/linearencoder_get", "linencoder": { "posval": 1,    "id": 1  }}
        // {"task":"/linearencoder_get", "linencoder": { "posval": 1,    "id": 3  }}
        log_i("linearencoder_get_fct");
        int isPos = -1;
        int linearencoderID = -1;
        float posVal = 0.0f;

        cJSON *doc = cJSON_CreateObject();
        cJSON *linearencoder = cJSON_GetObjectItemCaseSensitive(docin, key_linearencoder);
        if (cJSON_IsObject(linearencoder))
        {
            cJSON *posval = cJSON_GetObjectItemCaseSensitive(linearencoder, key_linearencoder_getpos);
            if (cJSON_IsNumber(posval))
            {
                isPos = posval->valueint;
            }

            cJSON *id = cJSON_GetObjectItemCaseSensitive(linearencoder, key_linearencoder_id);
            if (cJSON_IsNumber(id))
            {
                linearencoderID = id->valueint;
            }
        }

        if (isPos > 0 and linearencoderID >= 0)
        {

            cJSON *aritem = cJSON_CreateObject();
            posVal = encoders[linearencoderID].getPosition();

            cJSON_AddNumberToObject(aritem, "linearencoderID", linearencoderID);
            cJSON_AddNumberToObject(aritem, "absolutePos", edata[linearencoderID]->posval);
            cJSON_AddItemToObject(doc, "linearencoder_edata", aritem);

            Serial.println("linearencoder_edata for axis " + String(linearencoderID) + " is " + String(edata[linearencoderID]->posval) + " mm");
        }
        else
        {
            // return all linearencoder edata
            for (int i = 0; i < 4; i++)
            {
                cJSON *aritem = cJSON_CreateObject();
                posVal = encoders[linearencoderID].getPosition();

                cJSON_AddNumberToObject(aritem, "linearencoderID", linearencoderID);
                cJSON_AddNumberToObject(aritem, "absolutePos", edata[linearencoderID]->posval);
                cJSON_AddItemToObject(doc, "linearencoder_edata", aritem);
                // getEncoderInterface
                cJSON_AddNumberToObject(aritem, "encoderInterface", getEncoderInterface(i));
                Serial.println("linearencoder_edata for axis " + String(linearencoderID) + " is " + String(edata[linearencoderID]->posval) + " mm");
            }
        }
        return doc;
    }

    /*
        get called repeatedly, dont block this
    */
    void loop()
    {
        // print current position of the linearencoder
        // log_i("edata:  %f  %f  %f  %f", edata[0]->posval, edata[1]->posval, edata[2]->posval, edata[3]->posval);

        if (isPlot)
        {
            // Minimal encoder plotting to avoid serial interference with counting
            // Very reduced frequency to minimize impact on encoder accuracy
            static int plotCounter = 0;
            if (++plotCounter % 20 == 0)
            {
                // Only plot every 20 loops to reduce serial interference
                // Use log_i only (not Serial.println) to prevent direct serial interference
                // log_i("plot: %f", getCurrentPosition(1));
                // get current motor position for motor x / axis 1
                // get current motor position for motor x / axis 1 - improved direct access
                long currentMotorPos = getCurrentMotorPosition(1); // Updated by FAccelStep::updateData()
                Serial.print(currentMotorPos);
                Serial.print("; ");
                Serial.print(getCurrentPosition(1));
                Serial.println("; ");
                Serial.flush();
            }
        }
#ifdef MOTOR_CONTROLLER
        // Encoder control moved to immediate execution for fast response times
        // - Precision motion: executePrecisionMotionBlocking() provides 1kHz PID updates  
        // - Homing: executeHomingBlocking() provides fast mechanical limit detection
        // - Focus on X-axis only for maximum accuracy and performance
#endif
    }

    /*
    Execute precision motion control with immediate PID updates for fast response times.
    This replaces the slow loop-based polling with blocking execution for optimal performance.
    */
    void executePrecisionMotionBlocking(int stepperIndex)
    {
        if (stepperIndex < 0 || stepperIndex >= 4 || !edata[stepperIndex]){
            log_e("Invalid stepper index %d for precision motion", stepperIndex);
            return;
        }

        int s = stepperIndex;
        
        // Initialize the initial PID speed and start the motor
        float speed = edata[s]->pid.compute(edata[s]->positionToGo, getCurrentPosition(s));
        getData()[s]->isforever = true;
        getData()[s]->speed = speed;
        edata[s]->timeSinceMotorStart = millis();
        edata[s]->movePrecise = true;
        startStepper(s, true);

        log_i("Starting precision motion for axis %d: target=%f, initial_speed=%f", s, edata[s]->positionToGo, speed);

        // Fast precision control loop with immediate PID updates
        const unsigned long maxMotionTime = 10000; // 10 second safety timeout // TODO: This should be calculated based on distance and max speed
        const float positionTolerance = 10.0f; // step tolerance for completion
        const float stuckThreshold = 20.01f; // step threshold for detecting stuck motor
        const unsigned long stuckTimeout = 300; // ms before considering motor stuck

        
        unsigned long motionStartTime = millis();
        float previousPosition = getCurrentPosition(s);
        unsigned long lastPositionChangeTime = millis();

        while (edata[s]->movePrecise && (millis() - motionStartTime) < maxMotionTime)
        {
            // Read current encoder position with high frequency
            float currentPos = getCurrentPosition(s); // this returns the encoder position in counts which relates to steps from the motor to be consistent 
            float distanceToGo = edata[s]->positionToGo - currentPos;

            // Check if we've reached the target position
            if (abs(distanceToGo) < positionTolerance)
            {
                log_i("Precision motion completed for axis %d: final_pos=%f, target=%f, error=%f", 
                      s, currentPos, edata[s]->positionToGo, distanceToGo);
                break;
            }

            // Compute new PID speed based on current position
            speed = edata[s]->pid.compute(edata[s]->positionToGo, currentPos);
            // TODO: Perhaps we can detect a sign (i.e. motor direction vs encoder direction) error here and stop the motor or reverse direction
            // Update motor speed immediately for fast response
            getData()[s]->speed = speed;
            getData()[s]->isforever = true;
            startStepper(s, true);

            // Check for stuck motor condition
            if (abs(currentPos - previousPosition) > stuckThreshold)
            {
                lastPositionChangeTime = millis();
                previousPosition = currentPos;
            }
            else if ((millis() - lastPositionChangeTime) > stuckTimeout && abs(distanceToGo) > 5.0f)
            {
                // TODO: We should try again with a different approach (e.g. start again with reduced speed and then ramp up once, if still stuck, abort)
                log_w("Motor %d appears stuck at position %f (target %f), stopping motion", 
                      s, currentPos, edata[s]->positionToGo);
                break;
            }

            // Small delay to prevent watchdog timeout while maintaining fast response
            vTaskDelay(1); // 1ms delay for 1kHz update rate
        }

        // TODO: We should have a second run with a much lower speed to really get to the target position accurately maybe simply have a wrapping for loop 

        // Stop the motor and cleanup
        getData()[s]->speed = 0;
        getData()[s]->isforever = false;
        FocusMotor::stopStepper(s);
        edata[s]->movePrecise = false;

        // Save encoder position when motion completes
        saveEncoderPosition(s);
        
        float finalPosition = getCurrentPosition(s);
        float finalError = edata[s]->positionToGo - finalPosition;
        log_i("Precision motion finished for axis %d: final_pos=%f, final_error=%f", s, finalPosition, finalError);
    }

    /*
    Execute encoder-based homing with immediate response for fast detection of mechanical constraints.
    This replaces the slow loop-based polling with blocking execution for optimal homing performance.
    */
    void executeHomingBlocking(int stepperIndex, int speed)
    {
        if (stepperIndex < 0 || stepperIndex >= 4 || !edata[stepperIndex])
            return;

        int s = stepperIndex;
        
        log_i("Starting encoder-based homing for axis %d at speed %d", s, speed);

        // Initialize homing motion
        edata[s]->timeSinceMotorStart = millis();
        getData()[s]->isforever = true;
        getData()[s]->speed = speed;
        edata[s]->homeAxis = true;
        startStepper(s, true);

        // Fast homing control loop with immediate position monitoring
        const unsigned long maxHomingTime = 30000; // 30 second safety timeout
        const float positionChangeThreshold = 1.f; // step threshold for detecting no movement
        const unsigned long startupTimeout = 2000; // ms before monitoring for stuck condition
        const unsigned long sampleInterval = 50; // ms between position samples for averaging
        
        unsigned long homingStartTime = millis();
        float previousPositions[5] = {0, 0, 0, 0, 0}; // Rolling average buffer
        int positionIndex = 0;
        unsigned long lastSampleTime = millis();

        while (edata[s]->homeAxis && (millis() - homingStartTime) < maxHomingTime)
        {
            float currentPos = getCurrentPosition(s);
            
            // Update rolling average every sample interval
            if (millis() - lastSampleTime >= sampleInterval)
            {
                previousPositions[positionIndex] = currentPos;
                positionIndex = (positionIndex + 1) % 5;
                lastSampleTime = millis();
                
                // Calculate rolling average
                float avgPos = 0;
                for (int i = 0; i < 5; i++) {
                    avgPos += previousPositions[i];
                }
                avgPos /= 5.0f;
                
                // Check if motor appears stuck after startup timeout
                if ((millis() - homingStartTime) > startupTimeout)
                {
                    float positionChange = abs(avgPos - currentPos);
                    if (positionChange < positionChangeThreshold)
                    {
                        log_i("Homing complete for axis %d - no position change detected (stuck at mechanical limit)", s);
                        break;
                    }
                }
                
                log_d("Homing axis %d: pos=%f, avg=%f, change=%f", s, currentPos, avgPos, abs(avgPos - currentPos));
            }

            // Small delay to prevent watchdog timeout while maintaining fast response
            vTaskDelay(1); // 1ms delay for high frequency monitoring
        }

        // Stop the motor and set position to zero
        getData()[s]->speed = 0;
        getData()[s]->isforever = false;
        FocusMotor::stopStepper(s);
        edata[s]->homeAxis = false;

        // Move slightly away from endstop
        getData()[s]->absolutePosition = false;
        startStepper(s, true);
        
        // Wait for motor to finish moving away from endstop - non-blocking check
        unsigned long waitStart = millis();
        while (FocusMotor::isRunning(s))
        {
            vTaskDelay(1); // Non-blocking delay to prevent watchdog timeout
            
            // Add timeout protection to prevent infinite loop
            if (millis() - waitStart > 10000) { // 10 second timeout
                log_w("Homing timeout - motor %d may be stuck", s);
                FocusMotor::stopStepper(s);
                break;
            }
        }

        // Final cleanup and set zero position
        FocusMotor::stopStepper(s);
        getData()[s]->isforever = false;
        edata[s]->lastPosition = -1000000.0f;
        getData()[s]->speed = 0;
        
        // Set current position to zero using both methods for compatibility
        edata[s]->stepCount = 0;
        edata[s]->posval = 0;

        // Save the homed position (zero) to persistent storage
        saveEncoderPosition(s);
        log_i("Homing completed for encoder %d, zero position saved", s);
    }

    /*
    not needed all stuff get setup inside motor and digitalin, but must get implemented
    */
    void setup()
    {
        getData = FocusMotor::getData;
        startStepper = FocusMotor::startStepper;

        // for AS5311
        log_i("LinearEncoder setup AS5311 - A/B interface");

        for (int i = 0; i < 4; i++)
        {
            // A,X,y,z // the zeroth is unused but we need to keep the loops happy
            edata[i] = new LinearEncoderData();
            edata[i]->linearencoderID = i;
        }

        // Load encoder configuration from preferences
        Preferences preferences;
        preferences.begin("UC2_ENC", false);
        for (int i = 0; i < 4; i++)
        {
            // Load encoder direction
            edata[i]->encoderDirection = preferences.getBool(("encdir" + String(i)).c_str(), edata[i]->encoderDirection);

            log_i("Loaded encoder config for axis %d: encdir=%d", i, edata[i]->encoderDirection);
        }
        preferences.end();

        // Initialize PCNT encoder controller first for maximum accuracy
        if (PCNTEncoderController::isPCNTAvailable())
        {
            log_i("Initializing ESP32Encoder PCNT interface for high accuracy");
            PCNTEncoderController::setup();
        }
        else
        {
            log_w("ESP32Encoder PCNT interface not available");
        }

        // Configure only X-axis encoder for maximum stability and count accuracy
        // Multiple interrupt-based encoders can interfere with ESP32Encoder ISR handling
        if (pinConfig.ENC_X_A >= 0)
        {
            log_i("Configuring X-axis encoder: A=%d, B=%d", pinConfig.ENC_X_A, pinConfig.ENC_X_B);
            pinMode(pinConfig.ENC_X_A, INPUT_PULLUP);
            pinMode(pinConfig.ENC_X_B, INPUT_PULLUP);
            edata[1]->encoderDirection = pinConfig.ENC_X_encoderDirection;

            // Only add interrupt listeners if PCNT is not available to avoid conflicts
            if (!PCNTEncoderController::isPCNTAvailable())
            {
                InterruptController::addInterruptListner(pinConfig.ENC_X_A, (void (*)(uint8_t))&processEncoderEvent, gpio_int_type_t::GPIO_INTR_ANYEDGE);
                InterruptController::addInterruptListner(pinConfig.ENC_X_B, (void (*)(uint8_t))&processEncoderEvent, gpio_int_type_t::GPIO_INTR_ANYEDGE);
                log_i("X-axis encoder: using interrupt-based interface (PCNT not available)");
            }
            else
            {
                log_i("X-axis encoder: using PCNT interface for maximum accuracy");
            }

            setEncoderInterface(1, (EncoderInterface)PCNTEncoderController::isPCNTAvailable());
        }

        // Disable Y and Z axis encoders for now to focus on X-axis accuracy
        // They can be re-enabled later once X-axis counting is proven stable
        if (pinConfig.ENC_Y_A >= 0)
        {
            log_i("Y-axis encoder pins available but disabled (focusing on X-axis accuracy)");
        }
        if (pinConfig.ENC_Z_A >= 0)
        {
            log_i("Z-axis encoder pins available but disabled (focusing on X-axis accuracy)");
        }

        // Restore encoder positions from persistent storage
        loadAllEncoderPositions();
    }

    void setEncoderInterface(int encoderIndex, EncoderInterface interface)
    {
        log_i("Set Encoder Available Interface: %d, with interface %d", encoderIndex, interface);
        if (encoderIndex < 0 || encoderIndex >= 4)
            return;

        if (interface == ENCODER_PCNT_BASED && !PCNTEncoderController::isPCNTAvailable())
        {
            log_w("PCNT not available, keeping interrupt-based interface for encoder %d", encoderIndex);
            return;
        }

        edata[encoderIndex]->encoderInterface = interface;
        log_i("Encoder %d interface set to %s", encoderIndex,
              (interface == ENCODER_PCNT_BASED) ? "PCNT" : "Interrupt");
    }

    EncoderInterface getEncoderInterface(int encoderIndex)
    {
        if (encoderIndex < 0 || encoderIndex >= 4)
            return ENCODER_INTERRUPT_BASED;
        return edata[encoderIndex]->encoderInterface;
    }

    bool isPCNTEncoderSupported()
    {
        return PCNTEncoderController::isPCNTAvailable();
    }

    // Backward compatibility bridge functions
    int16_t getPCNTCount(int encoderIndex)
    {
        return PCNTEncoderController::getPCNTCount(encoderIndex);
    }

    void resetPCNTCount(int encoderIndex)
    {
        PCNTEncoderController::resetPCNTCount(encoderIndex);
    }

    bool isPCNTAvailable()
    {
        return PCNTEncoderController::isPCNTAvailable();
    }

    void saveEncoderPosition(int encoderIndex)
    {
        if (encoderIndex < 0 || encoderIndex >= 4 || !edata[encoderIndex])
            return;
        MotorEncoderConfig::saveEncoderPosition(encoderIndex, edata[encoderIndex]->posval);
    }

    void loadEncoderPosition(int encoderIndex)
    {
        if (encoderIndex < 0 || encoderIndex >= 4 || !edata[encoderIndex])
            return;
        float savedPosition = MotorEncoderConfig::loadEncoderPosition(encoderIndex);
        edata[encoderIndex]->posval = savedPosition;
        log_i("Restored encoder %d position to: %f step", encoderIndex, savedPosition);
    }

    void saveAllEncoderPositions()
    {
        log_i("Saving all encoder positions to persistent storage");
        for (int i = 0; i < 4; i++)
        {
            if (edata[i])
            {
                saveEncoderPosition(i);
            }
        }
    }

    void loadAllEncoderPositions()
    {
        log_i("Loading all encoder positions from persistent storage");
        for (int i = 0; i < 4; i++)
        {
            if (edata[i])
            {
                loadEncoderPosition(i);
            }
        }
    }

} // namespace name
