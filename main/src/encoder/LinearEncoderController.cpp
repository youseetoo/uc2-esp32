#include "LinearEncoderController.h"
#include "../digitalin/DigitalInController.h"
#include "../config/ConfigController.h"
#include "HardwareSerial.h"
#ifdef MOTOR_CONTROLLER
#include "../motor/FocusMotor.h"
#endif
#ifdef I2C_MASTER
#include "../motor/MotorTypes.h"
#include "../i2c/i2c_master.h"
#endif

#include "InterruptController.h"
#include "JsonKeys.h"

namespace LinearEncoderController
{

    //function pointer to motordata from different possible targets
    MotorData **(*getData)();
    //function pointer to start a stepper
    void (*startStepper)(int, bool);

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
                edata[1]->posval += edata[1]->mumPerStep;
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
        cJSON *calibrate = cJSON_GetObjectItem(j, key_linearencoder_calibrate);
        cJSON *setup = cJSON_GetObjectItem(j, key_linearencoder_setup);
        cJSON *home = cJSON_GetObjectItem(j, key_linearencoder_home);

        /******
         * CALIBRATE THE STEP-TO-MM VALUE
         ******/
        if (calibrate != NULL)
        {
            // {"task": "/linearencoder_act", "calpos": {"steppers": [ { "stepperid": 1, "calibsteps": 32000 , "speed": 10000} ]}}
            // depending on the axis, move for 1000 steps forward and measure the distance using the linearencoder in mm
            // do the same for the way back and calculate the steps per mm
            // print calibrate cjson
            cJSON *stprs = cJSON_GetObjectItem(calibrate, key_steppers);
            if (stprs != NULL)
            {
                // CALIBRATE THE STEP-TO-MM VALUE
                cJSON *stp = NULL;
                cJSON_ArrayForEach(stp, stprs)
                {
                    Stepper s = static_cast<Stepper>(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint);
                    // measure current value
                    edata[s]->positionPreMove = getCurrentPosition(s);
                    log_i("pre calib %f", edata[s]->positionPreMove);
                    int calibsteps = cJSON_GetObjectItemCaseSensitive(stp, key_linearencoder_calibpos)->valueint;
                    int speed = cJSON_GetObjectItemCaseSensitive(stp, key_speed)->valueint;
                    // get the motor object and chang the values so that it will move 1000 steps forward
                    getData()[s]->isforever = false;
                    getData()[s]->targetPosition = calibsteps;
                    getData()[s]->absolutePosition = false;
                    getData()[s]->speed = speed;
                    startStepper(s, true);
                    edata[s]->calibsteps = calibsteps;
                    edata[s]->requestCalibration = true;
                }
            }
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
            //{"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 2, "position": 5000 , "isabs":0, "speed": 2000, "cp":20, "ci":10, "cd":5, "encdir":1, "motdir":0, "res":1, "stp2phys":0.3125} ]}}
            //{"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 1, "position": 0 , "isabs":1, "speed": -10000, "cp":10, "ci":10, "cd":10} ]}}
            //{"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 1, "position": 10000 , "cp":40, "ci":1, "cd":10} ]}}

            cJSON *stprs = cJSON_GetObjectItem(movePrecise, key_steppers);
            if (stprs != NULL)
            {
                cJSON *stp = NULL;
                cJSON_ArrayForEach(stp, stprs)
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
                        distanceToGo += edata[s]->positionPreMove;
                    edata[s]->positionToGo = distanceToGo;

                    // PID Controller
                    edata[s]->c_p = 100.;
                    edata[s]->c_i = 0.5;
                    edata[s]->c_d = 10.;
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
                        getData()[s]->directionPinInverted = abs(cJSON_GetObjectItemCaseSensitive(stp, "motdir")->valueint);
                    if (cJSON_GetObjectItemCaseSensitive(stp, "res") != NULL)
                        edata[s]->correctResidualOnly = abs(cJSON_GetObjectItemCaseSensitive(stp, "res")->valueint);
                    else
                        edata[s]->correctResidualOnly = false;
                    if (cJSON_GetObjectItemCaseSensitive(stp, "stp2phys") != NULL)
                    {
                        edata[s]->stp2phys = abs(cJSON_GetObjectItemCaseSensitive(stp, "stp2phys")->valuedouble);
                    }
                    else
                    {
                        edata[s]->stp2phys = 1.0;
                    }

                    edata[s]->pid = PIDController(edata[s]->c_p, edata[s]->c_i, edata[s]->c_d);
                    float speed = edata[s]->pid.compute(edata[s]->positionToGo, getCurrentPosition(s));
                    int sign = speed > 0 ? 1 : -1;
                    if (abs(speed) > abs(edata[s]->maxSpeed))
                    {
                        speed = (float)(sign * edata[s]->maxSpeed);
                    }

                    // get the motor object and chang the values so that it will move 1000 steps forward
                    if (edata[s]->correctResidualOnly)
                    {
                        getData()[s]->isforever = false;
                        getData()[s]->speed = speed;
                        edata[s]->timeSinceMotorStart = millis();
                        edata[s]->movePrecise = true;

                        if (edata[s]->isAbsolute)
                            getData()[s]->targetPosition = (float)posToGo / edata[s]->stp2phys;
                        else
                            getData()[s]->targetPosition = (float)(posToGo + edata[s]->positionPreMove) / edata[s]->stp2phys;
                        log_d("Move precise from (residual only) %f to %f at motor speed %f, computed speed %f, encoderDirection %f with PID %f, %f, %f", edata[s]->positionPreMove, edata[s]->positionToGo, getData()[s]->speed, speed, edata[s]->encoderDirection, edata[s]->c_p, edata[s]->c_i, edata[s]->c_d);
                        startStepper(s, true);
                    }
                    else
                    {
                        getData()[s]->isforever = true;
                        getData()[s]->speed = speed;
                        edata[s]->timeSinceMotorStart = millis();
                        edata[s]->movePrecise = true;
                        log_d("Move precise from %f to %f at motor speed %f, computed speed %f, encoderDirection %f", edata[s]->positionPreMove, edata[s]->positionToGo, getData()[s]->speed, speed, edata[s]->encoderDirection);
                        startStepper(s, true);
                    }
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
        return qid;
    }

    void setCurrentPosition(int encoderIndex, float offsetPos)
    {
        edata[encoderIndex]->posval = offsetPos;
    }

    float getCurrentPosition(int encoderIndex)
    {
        return edata[encoderIndex]->posval;
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

#ifdef MOTOR_CONTROLLER
        // check if we need to read the linearencoder for all motors
        for (int i = 0; i < 4; i++)
        {

            if (edata[i]->requestCalibration and getData()[i]->stopped)
            {
                edata[i]->requestCalibration = false;
                delay(1000); // wait until slide settles
                // read linearencoder value for given axis
                edata[i]->valuePostCalib = getCurrentPosition(i);
                log_i("post calib %f", edata[i]->valuePostCalib);
                edata[i]->stepsPerMM = edata[i]->calibsteps / (edata[i]->valuePostCalib - edata[i]->positionPreMove);
                log_d("calibrated linearencoder %i with %f steps per mm", i, edata[i]->stepsPerMM);
            }
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
                    // wait until stepper reached new position
                    while (FocusMotor::isRunning(i))
                        delay(1);
                    // FocusMotor::setPosition(i, 0);
                    FocusMotor::stopStepper(i);
                    getData()[i]->isforever = false;
                    edata[i]->homeAxis = false;
                    edata[i]->lastPosition = -1000000.0f;
                    getData()[i]->speed = 0;
                    // set the current position to zero
                    edata[i]->posval = 0;
                }
                edata[i]->lastPosition = currentPos;
            }
            if (edata[i]->correctResidualOnly and getData()[i]->stopped)
            {
                // if we went a concrete number of step we have to move the motor to the correct position based on the encoder
                edata[i]->correctResidualOnly = false;
                getData()[i]->isforever = true;
                // read current encoder position
                float currentPos = getCurrentPosition(i);
                edata[i]->positionToGo -= currentPos;
                edata[i]->timeSinceMotorStart = millis();
                edata[i]->movePrecise = true;
                log_d("Move precise from %f to %f at motor speed %f, encoderDirection %f", edata[i]->positionPreMove, edata[i]->positionToGo, getData()[i]->speed, edata[i]->encoderDirection);
                startStepper(i, true);
            }

            if (edata[i]->movePrecise and not edata[i]->correctResidualOnly)
            {

                // read current encoder position
                float currentPos = getCurrentPosition(i);
                float currentPosAvg = calculateRollingAverage(currentPos);

                // PID Controller
                float speed = edata[i]->pid.compute(edata[i]->positionToGo, currentPos);
                int sign = speed > 0 ? 1 : -1;
                // if speed is too high, limit it to the maximum of motor
                if (abs(speed) > abs(edata[i]->maxSpeed))
                {
                    speed = (float)(sign * edata[i]->maxSpeed);
                }
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
#ifdef MOTOR_CONTROLLER
        getData = FocusMotor::getData;
        startStepper = FocusMotor::startStepper;
#endif
#ifdef I2C_MASTER
        getData = i2c_master::getData;
        startStepper = i2c_master::startStepper;
#endif
        // for AS5311
        log_i("LinearEncoder setup AS5311 - A/B interface");

        for (int i = 0; i < 4; i++)
        {
            // A,X,y,z // the zeroth is unused but we need to keep the loops happy
            edata[i] = new LinearEncoderData();
            edata[i]->linearencoderID = i;
        }

        if (pinConfig.ENC_X_A >= 0)
        {
            log_i("Adding X LinearEncoder: %i, %i", pinConfig.ENC_X_A, pinConfig.ENC_X_B);
            pinMode(pinConfig.ENC_X_A, INPUT_PULLUP);
            pinMode(pinConfig.ENC_X_B, INPUT_PULLUP);
            edata[1]->encoderDirection = pinConfig.ENC_X_encoderDirection;
            InterruptController::addInterruptListner(pinConfig.ENC_X_A, (void (*)(uint8_t)) & processEncoderEvent, gpio_int_type_t::GPIO_INTR_ANYEDGE);
            InterruptController::addInterruptListner(pinConfig.ENC_X_B, (void (*)(uint8_t)) & processEncoderEvent, gpio_int_type_t::GPIO_INTR_ANYEDGE);
        }
        if (pinConfig.ENC_X_A >= 0)
        {
            log_i("Adding Y LinearEncoder: %i, %i", pinConfig.ENC_Y_A, pinConfig.ENC_Y_B);
            pinMode(pinConfig.ENC_Y_A, INPUT_PULLUP);
            pinMode(pinConfig.ENC_Y_B, INPUT_PULLUP);
            edata[2]->encoderDirection = pinConfig.ENC_Y_encoderDirection;
            InterruptController::addInterruptListner(pinConfig.ENC_Y_A, (void (*)(uint8_t)) & processEncoderEvent, gpio_int_type_t::GPIO_INTR_ANYEDGE);
            InterruptController::addInterruptListner(pinConfig.ENC_Y_B, (void (*)(uint8_t)) & processEncoderEvent, gpio_int_type_t::GPIO_INTR_ANYEDGE);
        }
        if (pinConfig.ENC_Z_A >= 0)
        {
            log_i("Adding Z LinearEncoder: %i, %i", pinConfig.ENC_Z_A, pinConfig.ENC_Z_B);
            pinMode(pinConfig.ENC_Z_A, INPUT_PULLUP);
            pinMode(pinConfig.ENC_Z_B, INPUT_PULLUP);
            edata[3]->encoderDirection = pinConfig.ENC_Z_encoderDirection;
            InterruptController::addInterruptListner(pinConfig.ENC_Z_A, (void (*)(uint8_t)) & processEncoderEvent, gpio_int_type_t::GPIO_INTR_ANYEDGE);
            InterruptController::addInterruptListner(pinConfig.ENC_Z_B, (void (*)(uint8_t)) & processEncoderEvent, gpio_int_type_t::GPIO_INTR_ANYEDGE);
        }
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

} // namespace name
