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
                edata[s]->valuePreCalib = encoders[s]->readPosition();
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
    }
    else if (movePrecise!= NULL){
        // initiate a motor start and let the motor run until it reaches the position 
        //{"task": "/linearencoder_act", "moveP": {"steppers": [ { "stepperid": 1, "position": 500 , "speed": 10000} ]}}        
        cJSON *stprs = cJSON_GetObjectItem(movePrecise, key_steppers);
        log_i("move precise");
        if (stprs != NULL)
        {
            log_i("move precise 2");
            FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
            cJSON *stp = NULL;
            cJSON_ArrayForEach(stp, stprs)
            {
                Stepper s = static_cast<Stepper>(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint);
                // measure current value
                edata[s]->valuePreCalib = encoders[s]->readPosition();
                int posToGo = cJSON_GetObjectItemCaseSensitive(stp, key_linearencoder_position)->valueint;
                float distanceToGo = posToGo - edata[s]->valuePreCalib;
                int sign = distanceToGo>0?1:-1;
                edata[s]->positionToGo = posToGo;
                int speed = cJSON_GetObjectItemCaseSensitive(stp, key_speed)->valueint;
                // get the motor object and chang the values so that it will move 1000 steps forward
                motor->data[s]->isforever = true;
                motor->data[s]->speed = abs(speed)*sign;
                motor->startStepper(s);
                edata[s]->movePrecise = true;
                log_d("Move precise %f at speed %f", edata[s]->valuePreCalib, motor->data[s]->speed);
            }
        }
    }
    else if (setup!=NULL){
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
                float newPosition = (float) cJSON_GetObjectItemCaseSensitive(stp, key_linearencoder_position)->valueint;
                encoders[s]->setOffset(newPosition-encoders[s]->readPosition());
                log_i("set position %f", encoders[s]->readPosition());
            }
        }
    }
    else
    {
        log_e("unknown command");
    }
    return 1;
}

cJSON *LinearEncoderController::get(cJSON *docin)
{
    // {"task":"/linearencoder_get", "linencoder": { "posval": 1,    "id": 1  }}
    // {"task":"/linearencoder_get", "linencoder": { "posval": 1,    "id": 0  }}
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

    float pwmVal = 0.;
    int edgeCounter = 0;
    float posVal = 0.0f;
    if (isPos > 0 and linearencoderID >= 0)
    {
        cJSON *aritem = cJSON_CreateObject();
        pwmVal = encoders[linearencoderID]->readPWM();
        edgeCounter = encoders[linearencoderID]->readEdgeCounter();
        posVal = encoders[linearencoderID]->readPosition();
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
                edata[i]->valuePostCalib = encoders[i]->readPosition();
                log_i("post calib %f", edata[i]->valuePostCalib);
                edata[i]->stepsPerMM = edata[i]->calibsteps / (edata[i]->valuePostCalib - edata[i]->valuePreCalib);
                log_d("calibrated linearencoder %i with %f steps per mm", i, edata[i]->stepsPerMM);
            }
            if (edata[i]->movePrecise)
            {
                // read current encoder position
                edata[i]->posval = encoders[i]->readPosition();

                // PID Controller
                PIDController pid(2.0, 1.0, 0.5);  // Tune these parameters
                float speed = pid.compute(edata[i]->positionToGo, edata[i]->posval);

                log_i("Current position %f, position to go %f, speed %f\n",
                      edata[i]->posval, edata[i]->positionToGo, speed);

                // need to be PID?!
                float distanceToGo = edata[i]->positionToGo - edata[i]->posval;
                int sign = motor->data[i]->speed>0?1:-1;
                log_d("Current position %f, position to go %f, distance2Go %f, sign %i", edata[i]->posval, edata[i]->positionToGo, distanceToGo, sign);
                if (sign*(edata[i]->positionToGo - edata[i]->posval)<0){
                    motor->stopStepper(i);
                    edata[i]->movePrecise = false;
                }
            }
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
        encoders[1] = new AS5311(pinConfig.X_ENC_PWM, pinConfig.X_ENC_IND);
        encoders[1]->begin();
    }
    if (pinConfig.Y_ENC_PWM >= 0)
    {
        log_i("Adding Y Encoder: %i, %i", pinConfig.Y_ENC_PWM, pinConfig.Y_ENC_IND);
        encoders[2] = new AS5311(pinConfig.Y_ENC_PWM, pinConfig.Y_ENC_IND);
        encoders[2]->begin();
    }
    if (pinConfig.Z_ENC_PWM >= 0)
    {
        log_i("Adding Z Encoder: %i, %i", pinConfig.Z_ENC_PWM, pinConfig.Z_ENC_IND);
        encoders[3] = new AS5311(pinConfig.Z_ENC_PWM, pinConfig.Z_ENC_IND);
        encoders[3]->begin();
    }
}
