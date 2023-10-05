#include "LinearEncoderController.h"
#include "FastAccelStepper.h"
#include "../digitalin/DigitalInController.h"
#include "../config/ConfigController.h"
#include "HardwareSerial.h"

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
    // set position
    log_i("linearencoder_act_fct");

    // set position
    cJSON *calibrate = cJSON_GetObjectItem(j, key_linearencoder_calibrate);

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
                for (int t = 0; t < 2; t++)
                { // read twice?
                    //edata[s]->valuePreCalib = readValue(edata[s]->clkPin, edata[s]->dataPin);
                    delay(40);
                }
                delay(100); // wait until slide settles
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
                /*
                    // calibrate for 1000 steps in X
                    float steps_to_go = 1000;

                    // read linearencoder value for X and store value
                    float posval_init = readValue(edata[s]->clkPin, edata[s]->dataPin);

                    // move 1000 steps forward in background
                    motor->move(s, steps_to_go, true);

                    // read linearencoder value for X and store value
                    float posval_final = readValue(edata[s]->clkPin, edata[s]->dataPin);

                    // calculate steps per mm
                    float steps_per_mm = steps_to_go / (posval_final - posval_init);

                    // store steps per mm in config
                    //ConfigController *config = (ConfigController *)moduleController.get(AvailableModules::config);
                    //config->setLinearEncoderStepsPerMM(s, steps_per_mm);
                    log_d("calibrated linearencoder %i with %f steps per mm", s, steps_per_mm);
                    //motor->setPosition(s, cJSON_GetObjectItemCaseSensitive(stp,key_currentpos)->valueint);
                    */
            }
        }
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
    //Serial.println(cJSON_Print(docin));
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

    float posval = 0;
    int edgeCounter = 0;
    if (isPos > 0 and linearencoderID >= 0)
    {
        cJSON *aritem = cJSON_CreateObject();
        posval = encoders[linearencoderID]->readPosition();
        edgeCounter = encoders[linearencoderID]->readEdgeCounter();
        edata[linearencoderID]->posval = edgeCounter+posval;

        log_d("read linearencoder %i get position %f", linearencoderID, edata[linearencoderID]->posval);
        cJSON_AddNumberToObject(aritem, "posval", posval);
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
       // Serial.println(encoders[1]->readPosition());


        FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);

        // check if we need to read the linearencoder for all motors
        for (int i = 0; i < motor->data.size(); i++)
        {
            if (edata[i]->requestCalibration and motor->data[i]->stopped)
            {
                edata[i]->requestCalibration = false;
                delay(1000); // wait until slide settles
                for (int t = 0; t < 2; t++)
                { // read twice?
                    //edata[i]->valuePostCalib = readValue(edata[i]->clkPin, edata[i]->dataPin);
                    delay(40);
                }
                edata[i]->stepsPerMM = edata[i]->calibsteps / (edata[i]->valuePostCalib - edata[i]->valuePreCalib);
                log_d("post calib %f", edata[i]->valuePostCalib);
                log_d("calibrated linearencoder %i with %f steps per mm", i, edata[i]->stepsPerMM);
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
