#include "LinearEncoderController.h"
#include "FastAccelStepper.h"
#include "../digitalin/DigitalInController.h"
#include "../config/ConfigController.h"
#include "HardwareSerial.h"
#include "PIDController.h"

LinearEncoderController::LinearEncoderController() : Module() { log_i("ctor"); }
LinearEncoderController::~LinearEncoderController() { log_i("~ctor"); }

void processLinearEncoderLoop(void *p)
{
    Module *m = moduleController.get(AvailableModules::linearencoder);
}

/*
Handle REST calls to the LinearEncoderController module
*/
int LinearEncoderController::act(cJSON *j)
{
    log_i("linearencoder_act_fct");

    // calibrate the step-to-mm value
    cJSON *movePrecise = cJSON_GetObjectItem(j, key_linearencoder_moveprecise);
    cJSON *calibrate = cJSON_GetObjectItem(j, key_linearencoder_calibrate);
    cJSON *setup = cJSON_GetObjectItem(j, key_linearencoder_setup);
    cJSON *home = cJSON_GetObjectItem(j, key_linearencoder_home);

    if (calibrate != NULL)
    {
        // {"task": "/linearencoder_act", "calpos": {"steppers": [ { "stepperid": 1, "calibsteps": -32000 , "speed": 10000} ]}}
        // depending on the axis, move for 1000 steps forward and measure the distance using the linearencoder in mm
        // do the same for the way back and calculate the steps per mm
        // print calibrate cjson
        cJSON *stprs = cJSON_GetObjectItem(calibrate, key_steppers);
        if (stprs != NULL)
        {
            // print stprs
            FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
            cJSON *stp = NULL;
            cJSON_ArrayForEach(stp, stprs)
            {
                Stepper s = static_cast<Stepper>(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint);
                // measure current value
                edata[s]->valuePreCalib = getCurrentPosition(s);
                log_i("pre calib %f", edata[s]->valuePreCalib);
                int calibsteps = cJSON_GetObjectItemCaseSensitive(stp, key_linearencoder_calibpos)->valueint;
                int speed = cJSON_GetObjectItemCaseSensitive(stp, key_speed)->valueint;
                // get the motor object and chang the values so that it will move 1000 steps forward
                edata[s]->calibsteps = calibsteps;
                motor->data[s]->targetPosition = calibsteps;
                motor->data[s]->absolutePosition = false;
                motor->data[s]->speed = speed;
                motor->startStepper(s);
                edata[s]->requestCalibration = true;
                log_d("pre calib %f", edata[s]->valuePreCalib);
            }
        }
        //{"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 2, "position": 22000 , "speed": 10000, "cp":1, "ci":1, "cd":1} ]}}
    }
    else if (home != NULL)
    {
        // we want to start a motor until the linear encoder does not track any position change
        //{"task": "/linearencoder_act", "home": {"steppers": [ { "stepperid": 2, "speed": 10000} ]}}
        //{"task": "/linearencoder_act", "home": {"steppers": [ { "stepperid": 2, "direction":1, "speed": 15000} ]}}
        cJSON *stprs = cJSON_GetObjectItem(home, key_steppers);
        if (stprs != NULL)
        {
            FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
            cJSON *stp = NULL;
            cJSON_ArrayForEach(stp, stprs)
            {
                Stepper s = static_cast<Stepper>(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint);
                // measure current value
                edata[s]->valuePreCalib = getCurrentPosition(s);
                log_i("pre home %f", edata[s]->valuePreCalib);
                int speed = cJSON_GetObjectItemCaseSensitive(stp, key_speed)->valueint;
                // get the motor object and let it run forever int he specfied direction
                int direction = 1;
                if (cJSON_GetObjectItemCaseSensitive(stp, key_home_direction) != NULL)
                {
                    // 1 or -1
                    direction = cJSON_GetObjectItemCaseSensitive(stp, key_home_direction)->valueint;
                }
                log_d("home direction %i", direction);
                motor->data[s]->isforever = true;
                motor->data[s]->speed = direction * abs(speed);
                motor->startStepper(s);
                edata[s]->homeAxis = true;
            }
        }
    }
    else if (movePrecise != NULL)
    {
        // initiate a motor start and let the motor run until it reaches the position
        //{"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 1, "position": 500 , "speed": 10000} ]}}
        //{"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 2, "position": 500 , "speed": 10000} ]}}
        //{"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 2, "position": 500 , "speed": 10000, "cp":10, "ci":1, "cd":0.5} ]}}
        //{"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 2, "position": 500 , "speed": 10000, "cp":20, "ci":1, "cd":0.5} ]}}
        cJSON *stprs = cJSON_GetObjectItem(movePrecise, key_steppers);
        if (stprs != NULL)
        {
            FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
            cJSON *stp = NULL;
            cJSON_ArrayForEach(stp, stprs)
            {
                Stepper s = static_cast<Stepper>(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint);
                // measure current value
                edata[s]->valuePreCalib = getCurrentPosition(s);
                int posToGo = cJSON_GetObjectItemCaseSensitive(stp, key_linearencoder_position)->valueint;
                float distanceToGo = posToGo - edata[s]->valuePreCalib;
                int sign = distanceToGo > 0 ? 1 : -1;
                edata[s]->positionToGo = posToGo;

                // PID Controller
                edata[s]->c_p = 2.0;
                float c_p = cJSON_GetObjectItemCaseSensitive(stp, key_linearencoder_cp)->valuedouble;
                float c_i = cJSON_GetObjectItemCaseSensitive(stp, key_linearencoder_ci)->valuedouble;
                float c_d = cJSON_GetObjectItemCaseSensitive(stp, key_linearencoder_cd)->valuedouble;
                if (c_p != NULL)
                {
                    edata[s]->c_p = c_p;
                }
                if (c_i != NULL)
                {
                    edata[s]->c_i = c_i;
                }
                if (c_d != NULL)
                {
                    edata[s]->c_d = c_d;
                }
                log_i("setting PID to: %f, %f, %f", edata[s]->c_p, edata[s]->c_i, edata[s]->c_d);

                int speed = cJSON_GetObjectItemCaseSensitive(stp, key_speed)->valueint;
                // get the motor object and chang the values so that it will move 1000 steps forward
                motor->data[s]->isforever = true;
                motor->data[s]->speed = abs(speed) * sign;
                motor->startStepper(s);
                edata[s]->movePrecise = true;
                log_d("Move precise %f at speed %f", edata[s]->valuePreCalib, motor->data[s]->speed);
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
            FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
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
    return 1;
}

void LinearEncoderController::setCurrentPosition(int encoderIndex, float offsetPos)
{
    edata[encoderIndex]->offset = offsetPos;
}

float LinearEncoderController::getCurrentPosition(int encoderIndex)
{
    return edata[encoderIndex]->offset + encoders[encoderIndex]->getPosition();
}

cJSON *LinearEncoderController::get(cJSON *docin)
{
    // {"task":"/linearencoder_get", "linencoder": { "posval": 1,    "id": 2  }}
    // {"task":"/linearencoder_get", "linencoder": { "posval": 1,    "id": 1  }}
    // {"task":"/linearencoder_get", "linencoder": { "posval": 1,    "id": 3  }}
    log_i("linearencoder_get_fct");
    int isPos = -1;
    int linearencoderID = -1;
    // print json

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
    float posVal = 0.0f;
    if (isPos > 0 and linearencoderID >= 0)
    {
        cJSON *aritem = cJSON_CreateObject();
        pwmVal = 0;// encoders[linearencoderID]->readPWM();
        edgeCounter = 0; // encoders[linearencoderID]->readEdgeCounter();
        posVal = encoders[linearencoderID]->getPosition();
        edata[linearencoderID]->posval = posVal;

        log_d("read linearencoder %i get position %f", linearencoderID, edata[linearencoderID]->posval);
        cJSON_AddNumberToObject(aritem, "pwmVal", pwmVal);
        cJSON_AddNumberToObject(aritem, "edgeCounter", edgeCounter);
        cJSON_AddNumberToObject(aritem, "linearencoderID", linearencoderID);
        cJSON_AddNumberToObject(aritem, "absolutePos", edata[linearencoderID]->posval);
        cJSON_AddItemToObject(doc, "linearencoder_data", aritem);
    }
    return doc;
}

/*
    get called repeatedly, dont block this
*/
void LinearEncoderController::loop()
{
    if (moduleController.get(AvailableModules::linearencoder) != nullptr)
    {

        FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);

        // check if we need to read the linearencoder for all motors
        for (int i = 0; i < motor->data.size(); i++)
        {

            if (edata[i]->requestCalibration and motor->data[i]->stopped)
            {
                edata[i]->requestCalibration = false;
                delay(1000); // wait until slide settles
                // read linearencoder value for given axis
                edata[i]->valuePostCalib = getCurrentPosition(i);
                log_i("post calib %f", edata[i]->valuePostCalib);
                edata[i]->stepsPerMM = edata[i]->calibsteps / (edata[i]->valuePostCalib - edata[i]->valuePreCalib);
                log_d("calibrated linearencoder %i with %f steps per mm", i, edata[i]->stepsPerMM);
            }
            if (edata[i]->homeAxis)
            {
                // we track the position and if there is no to little change we will stop the motor and set the position to zero
                float currentPos = getCurrentPosition(i);
                float currentPosAvg = calculateRollingAverage(currentPos);
                log_d("current pos %f, current pos avg %f", currentPos, currentPosAvg);
                float thresholdPositionChange = 0.2;
                if (abs(currentPosAvg - edata[i]->lastPosition) < thresholdPositionChange)
                {
                    log_i("Stopping motor %i", i);
                    motor->data[i]->speed = 0;
                    motor->data[i]->isforever = false;
                    // move opposite direction to get the motor away from the endstop
                    //motor->setPosition(i, 0);
                    // blocks until stepper reached new position wich would be optimal outside of the endstep
                    if (motor->data[i]->speed > 0)
                        motor->data[i]->targetPosition = -50;
                    else
                        motor->data[i]->targetPosition = 50;
                    motor->data[i]->absolutePosition = false;
                    motor->startStepper(i);
                    // wait until stepper reached new position
                    while (motor->isRunning(i))
                        delay(1);
                    //motor->setPosition(i, 0);
                    motor->stopStepper(i);
                    motor->data[i]->isforever = false;
                    edata[i]->homeAxis = false;
                    edata[i]->lastPosition = 0.0f;

                    // set the current position to zero
                    edata[i]->offset = -getCurrentPosition(i);
                }
                edata[i]->lastPosition = currentPos;
            }
            if (edata[i]->movePrecise)
            {
                // read current encoder position
                edata[i]->posval = getCurrentPosition(i);

                // PID Controller
                PIDController pid(edata[i]->c_p, edata[i]->c_i, edata[i]->c_d); // Tune these parameters
                float speed = pid.compute(edata[i]->positionToGo, edata[i]->posval);
                int sign = speed > 0 ? 1 : -1;
                // if speed is too high, limit it to the maximum of motor
                if (abs(speed) > motor->data[i]->maxspeed)
                {
                    speed = sign * motor->data[i]->maxspeed;
                }
                // initiate a motion with updated speed
                log_i("Current position %f, position to go %f, speed %f\n",
                      edata[i]->posval, edata[i]->positionToGo, speed);
                motor->data[i]->speed = speed;
                motor->data[i]->isforever = true;
                motor->startStepper(i);

                // when should we end the motion?!
                float distanceToGo = edata[i]->positionToGo - edata[i]->posval;

                if (abs(distanceToGo) < 10)
                {
                    log_i("Stopping motor %i", i);
                    motor->data[i]->speed = 0;
                    motor->data[i]->isforever = false;
                    motor->stopStepper(i);
                    edata[i]->movePrecise = false;
                }
            } //{"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 2, "position": 500 , "speed": 10000, "cp":10, "ci":10.0, "cd":1.0} ]}}
            //{"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 2, "position": -40000 , "speed": 10000, "cp":10.0, "ci":5.0, "cd":0.0} ]}}
        }
    }
}

/*
not needed all stuff get setup inside motor and digitalin, but must get implemented
*/
void LinearEncoderController::setup()
{
    log_i("LinearEncoder setup");

    for (int i = 0; i < 4; i++)
    {
        // A,X,y,z // the zeroth is unused but we need to keep the loops happy
        edata[i] = new LinearEncoderData();
        edata[i]->linearencoderID = i;
    }
    if (pinConfig.X_ENC_PWM >= 0)
    {
        log_i("Adding X Encoder: %i, %i", pinConfig.X_ENC_PWM, pinConfig.X_ENC_IND);
        encoders[1] = new AS5311AB(pinConfig.X_ENC_PWM, pinConfig.X_ENC_IND);
        encoders[1]->begin();
    }
    /*
    if (pinConfig.Y_ENC_PWM >= 0)
    {
        log_i("Adding Y Encoder: %i, %i", pinConfig.Y_ENC_PWM, pinConfig.Y_ENC_IND);
        encoders[2] = new AS5311AB(pinConfig.Y_ENC_PWM, pinConfig.Y_ENC_IND, true);
        encoders[2]->begin();
    }
    if (pinConfig.Z_ENC_PWM >= 0)
    {
        log_i("Adding Z Encoder: %i, %i", pinConfig.Z_ENC_PWM, pinConfig.Z_ENC_IND);
        encoders[3] = new AS5311AB(pinConfig.Z_ENC_PWM, pinConfig.Z_ENC_IND, true);
        encoders[3]->begin();
    }
    */
}

float LinearEncoderController::calculateRollingAverage(float newVal)
{

    static float values[3] = {0.0, 0.0, 0.0};
    static int insertIndex = 0;
    static float sum = 0.0;

    sum -= values[insertIndex];
    values[insertIndex] = newVal;
    sum += newVal;

    insertIndex = (insertIndex + 1) % 3;

    return sum / 3.0;
}
