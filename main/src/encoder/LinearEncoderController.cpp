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

    void processEncoderEvent(uint8_t pin)
    {
        if ((pin == pinConfig.ENC_X_B))
        { // X-A changed
            bool pinA = digitalRead(pinConfig.ENC_X_A);
            bool pinB = digitalRead(pinConfig.ENC_X_B);
            if ((pinA == pinB) ^ edata[1]->encoderDirection)
                edata[1]->posval += edata[1]->mumPerStep; // TODO: We should update single steps (e.g. int) and then multiple later when retrieving the position somewhere else with the gloval conversion factor
            else
                edata[1]->posval -= edata[1]->mumPerStep;
        }
        else if (pin == pinConfig.ENC_X_A)
        { // X-B changed
            bool pinA = digitalRead(pinConfig.ENC_X_A);
            bool pinB = digitalRead(pinConfig.ENC_X_B);

            if ((pinA != pinB) ^ edata[1]->encoderDirection)
                edata[1]->posval += edata[1]->mumPerStep;
            else
                edata[1]->posval -= edata[1]->mumPerStep;
        }
        else if (pin == pinConfig.ENC_Y_A)
        { // Y-A changed
            bool pinA = digitalRead(pinConfig.ENC_Y_A);
            bool pinB = digitalRead(pinConfig.ENC_Y_B);

            if ((pinA == pinB) ^ edata[2]->encoderDirection)
                edata[2]->posval += edata[2]->mumPerStep;
            else
                edata[2]->posval -= edata[2]->mumPerStep;
        }
        else if (pin == pinConfig.ENC_Y_B)
        { // Y-B changed
            bool pinA = digitalRead(pinConfig.ENC_Y_A);
            bool pinB = digitalRead(pinConfig.ENC_Y_B);

            if ((pinA != pinB) ^ edata[2]->encoderDirection)
                edata[2]->posval += edata[2]->mumPerStep;
            else
                edata[2]->posval -= edata[2]->mumPerStep;
        }
        else if (pin == pinConfig.ENC_Z_A)
        { // Z-A changed
            bool pinA = digitalRead(pinConfig.ENC_Z_A);
            bool pinB = digitalRead(pinConfig.ENC_Z_B);
            if ((pinA == pinB) ^ edata[3]->encoderDirection)
                edata[3]->posval += edata[3]->mumPerStep;
            else
                edata[3]->posval -= edata[3]->mumPerStep;
        }
        else if (pin == pinConfig.ENC_Z_B)
        { // Z-B changed
            bool pinA = digitalRead(pinConfig.ENC_Z_A);
            bool pinB = digitalRead(pinConfig.ENC_Z_B);

            if ((pinA != pinB) ^ edata[3]->encoderDirection)
                edata[3]->posval += edata[3]->mumPerStep;
            else
                edata[3]->posval -= edata[3]->mumPerStep;
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
        cJSON *setup = cJSON_GetObjectItem(j, key_linearencoder_setup);
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
                    // get the motor object and let it run forever int he specfied direction
                    edata[s]->timeSinceMotorStart = millis();
                    getData()[s]->isforever = true;
                    getData()[s]->speed = speed;
                    startStepper(s, true);
                    edata[s]->homeAxis = true;
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
            // {"task": "/linearencoder_get", "stepperid": 1}
            //{"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 1, "position": 1000, "isabs":1,  "cp":100, "ci":0., "cd":10, "speed":1000} ]}}
            //{"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 1, "position": 500, "isabs":0,  "cp":100, "ci":0., "cd":10} ]}}
            //{"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 1, "position": 1500 , "isabs":0, "cp":20, "ci":1, "cd":0.5} ]}}
            //{"task":"/linearencoder_get", "linencoder": { "posval": 1,    "id": 1  }}
            //{"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 2, "position": 5000 , "isabs":0, "speed": 2000, "cp":20, "ci":10, "cd":5, "encdir":1, "motdir":0, "res":1} ]}}
            //{"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 2, "position": 5000 , "isabs":0, "speed": 2000, "cp":20, "ci":10, "cd":5, "encdir":1, "motdir":0, "res":1} ]}}
            //{"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 1, "position": 0 , "isabs":1, "speed": -10000, "cp":10, "ci":10, "cd":10} ]}}
            //{"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 1, "position": 10000 , "cp":40, "ci":1, "cd":10} ]}}
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

                        getData()[s]->isforever = true;
                        getData()[s]->speed = speed;
                        edata[s]->timeSinceMotorStart = millis();
                        edata[s]->movePrecise = true;
                        log_d("Move precise from %f to %f at motor speed %f, computed speed %f, encoderDirection %f", edata[s]->positionPreMove, edata[s]->positionToGo, getData()[s]->speed, speed, edata[s]->encoderDirection);
                        startStepper(s, true);
                }
            }
            else if (setup != NULL)
            {
                // {"task": "/linearencoder_act", "setup": {"steppers": [ { "stepperid": 1, "position": 0}]}}
                // setup the linearencoder
                // print setup cjson
                cJSON *stprs = cJSON_GetObjectItem(setup, key_steppers);
                if (stprs != NULL)
                {
                    // print stprs

                    cJSON *stp = NULL;
                    cJSON_ArrayForEach(stp, stprs)
                    {
                        Stepper s = static_cast<Stepper>(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint);
                        // set the current position of the encoder to the given value
                        float newPosition = (float)cJSON_GetObjectItemCaseSensitive(stp, key_linearencoder_position)->valueint;

                        /*
                        encoders[s]->setOffset(newPosition - encoders[s]->readPosition());
                        log_i("set position %f", getCurrentPosition(s));
                        */
                    }
                }
            }
            else
            {
                log_e("unknown command");
            }
        }

        // Handle encoder configuration settings
        // {"task": "/linearencoder_act", "config": {"steppers": [ { "stepperid": 1, "encdir": 1, "motdir": 0} ]}}
        cJSON *config = cJSON_GetObjectItem(j, "config");
        if (config != NULL)
        {
            cJSON *stprs = cJSON_GetObjectItem(config, key_steppers);
            if (stprs != NULL)
            {
                cJSON *stp = NULL;
                cJSON_ArrayForEach(stp, stprs)
                {
                    int s = cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint;

                    // Set encoder direction permanently
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

                    // Set motor direction permanently
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
                        case 0:
                            motdirKey = "motainv";
                            break; // A axis
                        case 1:
                            motdirKey = "motxinv";
                            break; // X axis
                        case 2:
                            motdirKey = "motyinv";
                            break; // Y axis
                        case 3:
                            motdirKey = "motzinv";
                            break; // Z axis
                        default:
                            break;
                        }
                        if (motdirKey != "")
                        {
                            preferences.putBool(motdirKey.c_str(), motDir);
                        }
                        preferences.end();
                    }

                    // Set micrometer per step (encoder resolution) permanently
                    if (cJSON_GetObjectItemCaseSensitive(stp, "mumpstp") != NULL)
                    {
                        float mumPerStep = cJSON_GetObjectItemCaseSensitive(stp, "mumpstp")->valuedouble;
                        edata[s]->mumPerStep = mumPerStep;
                        log_i("Set micrometer per step for axis %d: %f", s, mumPerStep);

                        // Store in preferences
                        Preferences preferences;
                        preferences.begin("UC2_ENC", false);
                        preferences.putFloat(("mumpstp" + String(s)).c_str(), mumPerStep);
                        preferences.end();
                    }

                    log_i("Encoder configuration updated for axis %d", s);
                }
            }

            // Handle global motor-encoder conversion factor configuration
            // {"task": "/linearencoder_act", "config": {"stepsToEncoderUnits": 0.3125}}
            cJSON *stepsToEncUnits = cJSON_GetObjectItem(config, "stepsToEncoderUnits");
            if (stepsToEncUnits != NULL && cJSON_IsNumber(stepsToEncUnits))
            {
                float conversionFactor = (float)stepsToEncUnits->valuedouble;
                MotorEncoderConfig::setStepsToEncoderUnits(conversionFactor);
                MotorEncoderConfig::saveToPreferences();
                log_i("Global motor-encoder conversion factor set to: %f µm per step", conversionFactor);
            }
        }

        // Handle diagnostic commands for encoder health checks
        // {"task": "/linearencoder_act", "diagnostic": {"stepperid": 1}}
        cJSON *diagnostic = cJSON_GetObjectItem(j, "diagnostic");
        if (diagnostic != NULL)
        {
            cJSON *stepperidItem = cJSON_GetObjectItem(diagnostic, key_stepperid);
            if (stepperidItem != NULL && cJSON_IsNumber(stepperidItem))
            {
                int stepperid = stepperidItem->valueint;
                log_i("Running encoder diagnostic for axis %d", stepperid);

                if (stepperid >= 1 && stepperid <= 3)
                {
                    // Run encoder accuracy test
                    if (PCNTEncoderController::isPCNTAvailable())
                    {
                        PCNTEncoderController::testEncoderAccuracy(stepperid);
                        log_i("Encoder diagnostic completed for axis %d", stepperid);
                    }
                    else
                    {
                        log_w("Encoder diagnostic not available - ESP32Encoder interface not active");
                    }
                }
                else
                {
                    log_e("Invalid stepperid %d for diagnostic (valid range: 1-3)", stepperid);
                }
            }
        }
        /*
                // Handle encoder diagnostics
        // {"task": "/linearencoder_act", "diagnostic": {"stepperid": 1}}
        cJSON *diagnostic = cJSON_GetObjectItem(j, "diagnostic");
        if (diagnostic != NULL)
        {
            int stepperid = cJsonTool::getJsonInt(diagnostic, key_stepperid);
            if (stepperid >= 1 && stepperid <= 3) {
                log_i("Running encoder diagnostic for axis %d", stepperid);

                // Test encoder accuracy using PCNT controller

                if (PCNTEncoderController::isPCNTAvailable()) {
                    PCNTEncoderController::testEncoderAccuracy(stepperid);
                }


                // Report current encoder state
                float currentPos = getCurrentPosition(stepperid);
                int64_t currentCount = 0;
                if (edata[stepperid]->encoderInterface == ENCODER_PCNT_BASED) {
                    currentCount = PCNTEncoderController::getEncoderCount(stepperid);
                } else {
                    currentCount = (int64_t)(edata[stepperid]->posval / edata[stepperid]->mumPerStep);
                }

                log_i("Encoder %d diagnostic: pos=%.3f, count=%lld, interface=%s",
                      stepperid, currentPos, currentCount,
                      (edata[stepperid]->encoderInterface == ENCODER_PCNT_BASED) ? "PCNT" : "Interrupt");
            }
        }
        */
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
            // Fallback to interrupt-based
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
            // Fallback to interrupt-based
            return edata[encoderIndex]->posval;
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

        float pwmVal = 0.0f;
        int edgeCounter = 0;
        if (isPos > 0 and linearencoderID >= 0)
        {

            cJSON *aritem = cJSON_CreateObject();
            pwmVal = 0;      // encoders[linearencoderID]->readPWM();
            edgeCounter = 0; // encoders[linearencoderID]->readEdgeCounter();
            posVal = encoders[linearencoderID].getPosition();

            cJSON_AddNumberToObject(aritem, "linearencoderID", linearencoderID);
            cJSON_AddNumberToObject(aritem, "absolutePos", edata[linearencoderID]->posval);
            cJSON_AddItemToObject(doc, "linearencoder_edata", aritem);

            Serial.println("linearencoder_edata for axis " + String(linearencoderID) + " is " + String(edata[linearencoderID]->posval) + " mm");
        }
        else
        {
            // return all linearencoder edata
            cJSON *aritem = cJSON_CreateObject();
            for (int i = 0; i < 4; i++)
            {
                cJSON *aritem = cJSON_CreateObject();
                pwmVal = 0;      // encoders[linearencoderID]->readPWM();
                edgeCounter = 0; // encoders[linearencoderID]->readEdgeCounter();
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
        // check if we need to read the linearencoder for all motors
        for (int i = 0; i < 4; i++) // TODO: Only use 1 motor (i.e. X)
        {
            if (edata[i]->homeAxis)
            {
                // we track the position and if there is no to little change we will stop the motor and set the position to zero
                float currentPos = getCurrentPosition(i);
                float currentPosAvg = calculateRollingAverage(currentPos);
                log_d("current pos %f, current pos avg %f, need to go to: %f", currentPos, currentPosAvg, edata[i]->positionToGo);
                float thresholdPositionChange = 0.1f;
                int startupTimout = 2000; // time to track if there is no movement which could mean the motion is blocked

                if (abs(currentPosAvg - currentPos) < thresholdPositionChange and
                    (millis() - edata[i]->timeSinceMotorStart) > startupTimout)
                {
                    log_i("Stopping motor  %i because there might be something in the way", i);
                    getData()[i]->isforever = false;
                    FocusMotor::stopStepper(i);
                    // move opposite direction to get the motor away from the endstop
                    // FocusMotor::setPosition(i, 0);
                    // blocks until stepper reached new position wich would be optimal outside of the endstep
                    getData()[i]->absolutePosition = false;
                    startStepper(i, true);
                    // wait until stepper reached new position - non-blocking check
                    int waitStart = millis();
                    while (FocusMotor::isRunning(i))
                    {
                        // Feed the watchdog and yield to other tasks to prevent watchdog panic
                        yield();
                        delay(10); // Reduced delay to prevent watchdog timeout

                        // Add timeout protection to prevent infinite loop
                        if (millis() - waitStart > 10000)
                        { // 10 second timeout
                            log_w("Homing timeout - motor %d may be stuck", i);
                            FocusMotor::stopStepper(i);
                            break;
                        }
                    }
                    // FocusMotor::setPosition(i, 0);
                    FocusMotor::stopStepper(i);
                    getData()[i]->isforever = false;
                    edata[i]->homeAxis = false;
                    edata[i]->lastPosition = -1000000.0f;
                    getData()[i]->speed = 0;
                    // set the current position to zero
                    edata[i]->posval = 0;

                    // Save the homed position (zero) to persistent storage
                    saveEncoderPosition(i);
                    log_i("Homing completed for encoder %d, zero position saved", i);
                }
                edata[i]->lastPosition = currentPos;
            }
            if (edata[i]->movePrecise)
            {
                // TODO: maybe we have to convert this into a blocking action to readout position faster and react faster

                // read current encoder position
                float currentPos = getCurrentPosition(i);
                float currentPosAvg = calculateRollingAverage(currentPos);

                // PID Controller - speed is already limited by PID output limits
                float speed = edata[i]->pid.compute(edata[i]->positionToGo, currentPos);
                // The PID controller already handles output limits, no need for additional clamping
                // initiate a motion with updated speed
                // log_i("Current position %f, position to go %f, speed %f\n",
                //      edata[i]->posval, edata[i]->positionToGo, speed);
                getData()[i]->speed = speed;
                getData()[i]->isforever = true;
                startStepper(i, true);

                // when should we end the motion?!
                float distanceToGo = edata[i]->positionToGo - currentPos;

                if (abs(distanceToGo) < 1) // TODO: adaptive threshold ?
                {
                    log_i("Stopping motor %i, distance to go: %f", i, distanceToGo);
                    getData()[i]->speed = 0;
                    getData()[i]->isforever = false;
                    FocusMotor::stopStepper(i);
                    edata[i]->movePrecise = false;

                    // Save encoder position when motion completes successfully
                    saveEncoderPosition(i);
                    log_i("Precision motion completed for encoder %d, position saved", i);
                }

                // in case the motor position does not move for 5 cycles, we stop the motor
                // {"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 1, "position": 1000 , "isabs":0, "speed": 10000, "cp":20, "ci":0, "cd":10} ]}}
                log_d("current pos %f, current pos avg %f, need to go to: %f at speed %f, distanceToGo %f", currentPos, currentPosAvg, edata[i]->positionToGo, speed, distanceToGo);
                float thresholdPositionChange = 0.01f;
                int startupTimout = 1000;

                // Stopping motor if there is no movement => something is blocking the motor and we are beyond the startup timeout and we are within the threshold of 5 µm
                if (abs(currentPosAvg - currentPos) < thresholdPositionChange and
                    (millis() - edata[i]->timeSinceMotorStart) > startupTimout and
                    abs(distanceToGo) > 5)
                {
                    log_i("Stopping motor  %i because there might be something in the way", i);
                    getData()[i]->speed = 0;
                    getData()[i]->isforever = false;
                    FocusMotor::stopStepper(i);
                    edata[i]->movePrecise = false;
                    log_i("Final Position %f", getCurrentPosition(i));

                    // Save encoder position when motion stops due to obstruction
                    saveEncoderPosition(i);
                    log_i("Motion stopped for encoder %d, position saved", i);
                }
            }
        }
#endif
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

            // Load micrometer per step
            edata[i]->mumPerStep = preferences.getFloat(("mumpstp" + String(i)).c_str(), edata[i]->mumPerStep);

            log_i("Loaded encoder config for axis %d: encdir=%d, mumPerStep=%f",
                  i, edata[i]->encoderDirection, edata[i]->mumPerStep);
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

    float calculateRollingAverage(float newVal)
    {

        static float values[5] = {0.0, 0.0, 0.0, 0.0, 0.0};
        static int insertIndex = 0;
        static float sum = 0.0;

        sum -= values[insertIndex];
        values[insertIndex] = newVal;
        sum += newVal;

        insertIndex = (insertIndex + 1) % 5;

        return sum / 5.0;
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
        log_i("Restored encoder %d position to: %f µm", encoderIndex, savedPosition);
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
