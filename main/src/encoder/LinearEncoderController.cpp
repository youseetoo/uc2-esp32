#include "LinearEncoderController.h"
#include "../digitalin/DigitalInController.h"
#include "../config/ConfigController.h"
#include "../motor/MotorEncoderConfig.h"
#include "HardwareSerial.h"
#include <Preferences.h>
#include "esp_task_wdt.h"
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

    // Performance configuration
    static bool useTaskBasedOperation = true; // Can be enabled for non-blocking operations
    static TaskHandle_t precisionMotionTask = nullptr;
    static TaskHandle_t homingTask = nullptr;
    static bool precisionMotionTaskRunning = false;
    // Add task status tracking for better cleanup
    static volatile bool 
    d = false;
    static volatile bool homingTaskRunning = false;

    std::array<LinearEncoderData *, 4> edata;
    // std::array<AS5311 *, 4> encoders;
    std::array<AS5311AB, 4> encoders;

    // Interrupt-based encoder handling - optimized for X-axis performance
    // Architecture note: Could be refactored to separate class for better modularity
    
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
            /*
            {"task": "/linearencoder_act", "home": {"steppers": [ { "stepperid": 1,  "speed": 20000 } ]}}
            */
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
                    
                    // Execute homing - can use blocking or task-based approach
                    if (useTaskBasedOperation) {
                        // Non-blocking task-based execution
                        if (!startHomingTask(s, speed)) {
                            log_e("Failed to start homing task for axis %d", s);
                        }
                    } else {
                        // Immediate execution for fastest response times (default)
                        executeHomingBlocking(s, speed);
                    }
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
            {"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 1, "position": 20000, "isabs":0,  "cp":5, "ci":0., "cd":0, "speed":25000} ]}}
            {"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 1, "position": 500, "isabs":0,  "cp":100, "ci":0., "cd":10} ]}}
            {"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 1, "position": 1500 , "isabs":0, "cp":20, "ci":1, "cd":0.5} ]}}
            {"task":"/linearencoder_get", "linencoder": { "posval": 1,    "id": 1  }}
            {"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 2, "position": 5000 , "isabs":0, "speed": 2000, "cp":20, "ci":10, "cd":5, "encdir":1, "motdir":0, "res":1} ]}}
            {"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 2, "position": 5000 , "isabs":0, "speed": 2000, "cp":20, "ci":10, "cd":5, "encdir":1, "motdir":0, "res":1} ]}}
            {"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 1, "position": 0 , "isabs":1, "speed": -10000, "cp":10, "ci":10, "cd":10} ]}}
            {"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 1, "position": 10000 , "cp":40, "ci":1, "cd":10} ]}}
            */
            // PRECISION MOTION CONTROL - Focus on single axis for optimal performance
            // Retrieve initial encoder position for accurate starting reference
            cJSON *stprs = cJSON_GetObjectItem(movePrecise, key_steppers);
            if (stprs != NULL)
            {
                cJSON *stp = NULL;
                cJSON_ArrayForEach(stp, stprs)
                {
                    int s = cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint;
                    
                    // Focus on X-axis (stepperid=1) for maximum stability and performance
                    if (s != 1) {
                        log_w("Precision motion currently optimized for X-axis only (stepperid=1), skipping axis %d", s);
                        continue;
                    }
                    
                    // Measure current encoder position for accurate starting reference
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

                    // Calculate the position to go with unit consistency verification
                    float distanceToGo = posToGo;
                    if (!edata[s]->isAbsolute)
                    {
                        // For relative positioning, add to current position
                        // Units are already normalized (steps), so addition is straightforward
                        distanceToGo += edata[s]->positionPreMove;
                    }
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
                        
                        // Store motor direction permanently in preferences for this specific motor
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
                            preferences.putBool(motdirKey.c_str(), getData()[s]->directionPinInverted);
                            log_i("Motor direction for axis %d permanently stored: %s", s, getData()[s]->directionPinInverted ? "inverted" : "normal");
                        }
                        preferences.end();
                    }

                    edata[s]->pid = PIDController(edata[s]->c_p, edata[s]->c_i, edata[s]->c_d);
                    // Set output limits based on maximum speed
                    edata[s]->pid.setOutputLimits(-edata[s]->maxSpeed, edata[s]->maxSpeed);

                    float speed = edata[s]->pid.compute(edata[s]->positionToGo, getCurrentPosition(s));

                    log_d("Move precise from %f to %f at motor speed %f, computed speed %f, encoderDirection %f", edata[s]->positionPreMove, edata[s]->positionToGo, getData()[s]->speed, speed, edata[s]->encoderDirection);
                    
                    // Execute precision motion control - can use blocking or task-based approach
                    if (useTaskBasedOperation) {
                        // Non-blocking task-based execution for better system responsiveness
                        if (!startPrecisionMotionTask(s)) {
                            log_e("Failed to start precision motion task for axis %d", s);
                        }
                    } else {
                        // Immediate execution for fastest response times (default)
                        executePrecisionMotionBlocking(s);
                    }
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
    Task-based precision motion control for non-blocking operations
    Can be enabled as alternative to blocking approach for better system responsiveness
    */
    void precisionMotionTaskWrapper(void* parameter) {
        precisionMotionTaskRunning = true;
        int stepperIndex = *((int*)parameter);
        executePrecisionMotionBlocking(stepperIndex);
        // Clean up task handle and status before deleting task to avoid race condition
        precisionMotionTaskRunning = false;
        precisionMotionTask = nullptr;
        vTaskDelete(nullptr); // Clean up task when complete
    }
    
    void homingTaskWrapper(void* parameter) {
        homingTaskRunning = true;
        struct HomingParams {
            int stepperIndex;
            int speed;
        };
        HomingParams* params = (HomingParams*)parameter;
        executeHomingBlocking(params->stepperIndex, params->speed);
        delete params;
        // Clean up task handle and status before deleting task to avoid race condition
        homingTaskRunning = false;
        homingTask = nullptr;
        vTaskDelete(nullptr); // Clean up task when complete
    }
    
    // Start precision motion in task (non-blocking alternative)
    bool startPrecisionMotionTask(int stepperIndex) {
        // Check if task is still running using both handle and status flag
        if (precisionMotionTaskRunning || precisionMotionTask != nullptr) {
            // Double-check task state if handle exists
            if (precisionMotionTask != nullptr) {
                eTaskState taskState = eTaskGetState(precisionMotionTask);
                if (taskState == eDeleted || taskState == eInvalid) {
                    // Task has finished, clean up the handle and flag
                    precisionMotionTask = nullptr;
                    precisionMotionTaskRunning = false;
                    log_i("Cleaned up finished precision motion task");
                } else {
                    log_w("Precision motion task already running (state: %d)", taskState);
                    return false;
                }
            } else {
                log_w("Precision motion task status flag indicates running but no handle");
                return false;
            }
        }
        
        static int taskStepperIndex = stepperIndex;
        BaseType_t result = xTaskCreatePinnedToCore(
            precisionMotionTaskWrapper,
            "PrecisionMotion",
            4096, // Stack size
            &taskStepperIndex,
            2, // Priority
            &precisionMotionTask,
            1 // Core 1
        );
        
        if (result == pdPASS) {
            log_i("Started precision motion task for axis %d", stepperIndex);
        } else {
            // Reset handle if task creation failed
            precisionMotionTask = nullptr;
        }
        
        return result == pdPASS;
    }
    
    // Start homing in task (non-blocking alternative)  
    bool startHomingTask(int stepperIndex, int speed) {
        // Check if task is still running using both handle and status flag
        if (homingTaskRunning || homingTask != nullptr) {
            // Double-check task state if handle exists
            if (homingTask != nullptr) {
                eTaskState taskState = eTaskGetState(homingTask);
                if (taskState == eDeleted || taskState == eInvalid) {
                    // Task has finished, clean up the handle and flag
                    homingTask = nullptr;
                    homingTaskRunning = false;
                    log_i("Cleaned up finished homing task");
                } else {
                    log_w("Homing task already running (state: %d)", taskState);
                    return false;
                }
            } else {
                log_w("Homing task status flag indicates running but no handle");
                return false;
            }
        }
        
        struct HomingParams {
            int stepperIndex;
            int speed;
        };
        HomingParams* params = new HomingParams{stepperIndex, speed};
        
        BaseType_t result = xTaskCreatePinnedToCore(
            homingTaskWrapper,
            "Homing",
            4096, // Stack size
            params,
            2, // Priority
            &homingTask,
            1 // Core 1
        );
        
        if (result == pdPASS) {
            log_i("Started homing task for axis %d at speed %d", stepperIndex, speed);
        } else {
            delete params; // Clean up params if task creation failed
            homingTask = nullptr; // Reset handle if task creation failed
        }
        
        return result == pdPASS;
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
        startStepper(s, 0);

        log_i("Starting precision motion for axis %d: target=%f, initial_speed=%f", s, edata[s]->positionToGo, speed);

        // Fast precision control loop with immediate PID updates
        // Dynamic timeout calculation based on distance and maximum speed
        float distance = abs(edata[s]->positionToGo - getCurrentPosition(s));
        float safetyMargin = 5.0f; // 5 second safety margin
        unsigned long calculatedTimeout = (unsigned long)((distance / edata[s]->maxSpeed) * 1000.0f) + (safetyMargin * 1000.0f);
        const unsigned long maxMotionTime = max(calculatedTimeout, 3000UL); // Minimum 3 seconds, maximum based on calculation
        const float positionTolerance = 10.0f; // step tolerance for completion
        const float stuckThreshold = 10.01f; // step threshold for detecting stuck motor
        const unsigned long stuckTimeout = 300; // ms before considering motor stuck

        
        unsigned long motionStartTime = millis();
        float previousPosition = getCurrentPosition(s);
        unsigned long lastPositionChangeTime = millis();

        unsigned long lastWatchdogReset = millis();
        const unsigned long watchdogResetInterval = 100; // Reset watchdog every 100ms
        
        while (edata[s]->movePrecise )
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

            if (!(millis() - motionStartTime < maxMotionTime)){
                log_w("Precision motion timeout for axis %d after %lu ms: current_pos=%f, target=%f, error=%f", 
                      s, millis() - motionStartTime, currentPos, edata[s]->positionToGo, distanceToGo);
                break;
            }

            // Compute new PID speed based on current position
            speed = edata[s]->pid.compute(edata[s]->positionToGo, currentPos);
            
            // Direction error detection: Check if motor and encoder are working in opposite directions
            if (false && (abs(distanceToGo) > 50.0f && abs(speed) > 100.0f)) { // Only check when meaningful motion is expected (//TODO: This is currently not working robustly)
                static float lastDistanceToGo = distanceToGo;
                static float lastSpeed = speed;
                
                // If distance is increasing while motor is running at significant speed, directions may be mismatched
                if ((lastDistanceToGo != 0) && (abs(distanceToGo) > abs(lastDistanceToGo) + 10.0f) && 
                    (abs(speed) > abs(lastSpeed) * 0.8f)) {
                    log_w("Possible direction mismatch detected for axis %d - stopping motor. Distance: %f->%f, Speed: %f", 
                          s, lastDistanceToGo, distanceToGo, speed);
                    break;
                }
                lastDistanceToGo = distanceToGo;
                lastSpeed = speed;
            }
            // Update motor speed immediately for fast response
            getData()[s]->speed = speed;
            getData()[s]->isforever = true;
            startStepper(s, 0);

            // Check for stuck motor condition
            if (abs(currentPos - previousPosition) > stuckThreshold)
            {
                lastPositionChangeTime = millis();
                previousPosition = currentPos;
            }
            else if ((millis() - lastPositionChangeTime) > stuckTimeout && abs(distanceToGo) > 5.0f)
            {
                // Intelligent stuck motor recovery: Try reduced speed approach first
                static int recoveryAttempt = 0;
                recoveryAttempt++;
                
                if (recoveryAttempt <= 2) {
                    log_w("Motor %d stuck (attempt %d) - trying recovery with reduced speed", s, recoveryAttempt);
                    
                    // Reduce speed by half and try again
                    float recoverySpeed = speed * 0.5f;
                    getData()[s]->speed = recoverySpeed;
                    getData()[s]->isforever = true;
                    startStepper(s, 0);
                    
                    // Reset stuck detection for recovery attempt
                    lastPositionChangeTime = millis();
                    previousPosition = currentPos;
                    
                    // Give recovery attempt some time
                    vTaskDelay(pdMS_TO_TICKS(100));
                    continue;
                } else {
                    log_e("Motor %d recovery failed after %d attempts - stopping motion", s, recoveryAttempt);
                    recoveryAttempt = 0; // Reset for next motion
                    break;
                }
            }

            // Reset watchdog timer periodically to prevent timeout
            if ((millis() - lastWatchdogReset) > watchdogResetInterval)
            {
                esp_task_wdt_reset();
                lastWatchdogReset = millis();
            }

            // Delay to prevent watchdog timeout while maintaining reasonable response time
            vTaskDelay(pdMS_TO_TICKS(1)); 
        }

        // Two-stage precision approach for higher accuracy
        // Stage 1: Fast approach (if we're still far from target)
        float finalDistance = abs(edata[s]->positionToGo - getCurrentPosition(s));
        edata[s]->pid.kp= edata[s]->c_p * .25f; // Decrease P gain for final approach
        edata[s]->pid.ki= edata[s]->c_i * .25f; // Decrease I gain for final approach
        edata[s]->pid.kd= edata[s]->c_d * .25f; // Decrease D gain for final approach
        if (finalDistance > 5.0f) { // If we're still far from target, do a slower precision pass
            log_i("Starting precision stage 2 for axis %d: remaining distance=%f", s, finalDistance);
            
            const float precisionSpeed = min(edata[s]->maxSpeed * 0.2f, 1000.0f); // 20% of max speed or 1000, whichever is lower
            const float precisionTolerance = 2.0f; // Tighter tolerance for final positioning
            const unsigned long precisionTimeout = 5000; // 5 second timeout for precision stage
            
            unsigned long precisionStartTime = millis();
            
            while ((millis() - precisionStartTime) < precisionTimeout) {
                float currentPos = getCurrentPosition(s);
                float distanceToGo = edata[s]->positionToGo - currentPos;
                
                if (abs(distanceToGo) < precisionTolerance) {
                    log_i("Precision stage 2 completed for axis %d: final_error=%f", s, distanceToGo);
                    break;
                }
                
                // Use conservative speed for precision positioning
                speed = edata[s]->pid.compute(edata[s]->positionToGo, currentPos);

                float speed = (distanceToGo > 0) ? precisionSpeed : -precisionSpeed;
                getData()[s]->speed = speed;
                getData()[s]->isforever = true;
                startStepper(s, 0);
                
                // Reset watchdog periodically
                if ((millis() - lastWatchdogReset) > watchdogResetInterval) {
                    esp_task_wdt_reset();
                    lastWatchdogReset = millis();
                }
                
                vTaskDelay(pdMS_TO_TICKS(2)); // Slower update rate for precision
            }
        } 

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

        // Initialize homing motion with proper state cleanup
        edata[s]->timeSinceMotorStart = millis();
        
        // Ensure motor is properly stopped before starting homing
        // FocusMotor::stopStepper(s);
        // vTaskDelay(pdMS_TO_TICKS(100)); // Give motor time to stop
        
        // Clear any previous motion state that might interfere
        getData()[s]->isforever = true;
        getData()[s]->speed = speed;
        getData()[s]->absolutePosition = false; // Ensure relative positioning mode
        edata[s]->homeAxis = true;
        log_i("Starting motor for homing on axis %d", s);
        FocusMotor::startStepper(s, 0);
        // Fast homing control loop with immediate position monitoring
        // High frequency sampling eliminates need for rolling averages
        const unsigned long maxHomingTime = 30000; // 30 second safety timeout
        const float positionChangeThreshold = 1.0f; // step threshold for detecting no movement
        const unsigned long stuckDetectionInterval = 1000; // ms between position checks
        
        unsigned long homingStartTime = millis();
        float lastSamplePosition = getCurrentPosition(s);
        unsigned long lastSampleTime = millis();
        unsigned long lastWatchdogReset = millis();
        const unsigned long watchdogResetInterval = 100; // Reset watchdog every 100ms

        while (edata[s]->homeAxis && (millis() - homingStartTime) < maxHomingTime)
        {
            float currentPos = getCurrentPosition(s);
            
            // Check for stuck condition every sample interval (no need for rolling average with fast sampling)
            if (millis() - lastSampleTime >= stuckDetectionInterval)
            {
                float positionChange = abs(currentPos - lastSamplePosition);
                
                // Check if motor appears stuck (no significant movement detected)
                if (positionChange < positionChangeThreshold)
                {
                    log_i("Homing complete for axis %d - no position change detected (reached mechanical limit)", s);
                    break;
                }
                
                lastSamplePosition = currentPos;
                lastSampleTime = millis();
                
                log_d("Homing axis %d: pos=%f, change=%f", s, currentPos, positionChange);
            }

            // Reset watchdog timer periodically to prevent timeout
            if ((millis() - lastWatchdogReset) > watchdogResetInterval)
            {
                esp_task_wdt_reset();
                lastWatchdogReset = millis();
            }

            // Delay to prevent watchdog timeout while maintaining reasonable response time
            vTaskDelay(pdMS_TO_TICKS(1)); // 5ms delay instead of 1ms for better watchdog compatibility
        }

        // Motor state cleanup and final positioning 

        // Stop the motor and set position to zero
        //getData()[s]->speed = 0;
        getData()[s]->isforever = false;
        FocusMotor::stopStepper(s);
        edata[s]->homeAxis = false;

        // Move slightly away from endstop
        getData()[s]->absolutePosition = false;
        startStepper(s, 0);
        
        // Wait for motor to finish moving away from endstop - non-blocking check
        unsigned long waitStart = millis();
        unsigned long lastWatchdogResetWait = millis();
        const unsigned long watchdogResetIntervalWait = 100; // Reset watchdog every 100ms
        
        while (FocusMotor::isRunning(s))
        {
            // Reset watchdog timer periodically to prevent timeout
            if ((millis() - lastWatchdogResetWait) > watchdogResetIntervalWait)
            {
                esp_task_wdt_reset();
                lastWatchdogResetWait = millis();
            }
            
            vTaskDelay(pdMS_TO_TICKS(5)); // 5ms delay to prevent watchdog timeout
            
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
    
    // Task-based operation control functions
    void setTaskBasedOperation(bool enabled) {
        useTaskBasedOperation = enabled;
        log_i("Task-based operation %s", enabled ? "enabled" : "disabled");
    }
    
    bool isTaskBasedOperationEnabled() {
        return useTaskBasedOperation;
    }

} // namespace name
