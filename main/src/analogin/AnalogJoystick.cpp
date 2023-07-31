#include "AnalogJoystick.h"
#include "../../ModuleController.h"
#include "../motor/FocusMotor.h"
#include "cJSON.h"

void processLoopAJoy(void * param)
{
    for(;;)
    {
        moduleController.get(AvailableModules::analogJoystick)->loop();
        vTaskDelay(5 / portTICK_PERIOD_MS);
    }
}

AnalogJoystick::AnalogJoystick(/* args */){};
AnalogJoystick::~AnalogJoystick(){};
void AnalogJoystick::setup()
{
    log_d("Setup analog joystick");
    pinMode(pinConfig.ANLOG_JOYSTICK_X, INPUT);
    pinMode(pinConfig.ANLOG_JOYSTICK_Y, INPUT);
    xTaskCreate(&processLoopAJoy, "analogJoyStick_task", pinConfig.ANALOGJOYSTICK_TASK_STACKSIZE, NULL, 5, NULL);
}
int AnalogJoystick::act(cJSON* jsonDocument) { return 1;}


/*"joy" : 
    {
        "joyX" : 1,
        "joyY" : 1
    }*/
cJSON* AnalogJoystick::get(cJSON*  doc) {

    cJSON *monitor = cJSON_CreateObject();
    cJSON *analogholder = NULL;
    cJSON *x = NULL;
    cJSON *y = NULL;
    analogholder = cJSON_CreateObject();
    cJSON_AddItemToObject(monitor, key_joy, analogholder);
    x = cJSON_CreateNumber(pinConfig.ANLOG_JOYSTICK_X);
    y = cJSON_CreateNumber(pinConfig.ANLOG_JOYSTICK_Y);
    cJSON_AddItemToObject(analogholder, key_joiypinX, x);
    cJSON_AddItemToObject(analogholder, key_joiypinY, y);
    return monitor;
}

void AnalogJoystick::loop()
{ 
    
    if (moduleController.get(AvailableModules::motor) != nullptr)
    {
        FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
        if (pinConfig.ANLOG_JOYSTICK_X > 0)
        {
            int x = analogRead(pinConfig.ANLOG_JOYSTICK_X) - max_in_value / 2;
            //log_i("X: %i" , x);
            if (x >= zeropoint|| x <= -zeropoint)
            {
                motor->data[Stepper::X]->speed = x;
                motor->data[Stepper::X]->isforever = true;
                
                if (motor->data[Stepper::X]->stopped && joystick_drive_X == 0)
                {
                     log_i("start motor X");
                    joystick_drive_X = 1;
                    motor->startStepper(Stepper::X);
                }
            }
            else if (!motor->data[Stepper::X]->stopped && joystick_drive_X > 0 && (x <= zeropoint || x >= -zeropoint))
            {
                log_i("stop motor X stopped:%i x drive:%i , x:%i", motor->data[Stepper::X]->stopped, joystick_drive_X, x);
                motor->stopStepper(Stepper::X);
                joystick_drive_X = 0;
            }
        }
        if (pinConfig.ANLOG_JOYSTICK_Y > 0)
        {
            int y = analogRead(pinConfig.ANLOG_JOYSTICK_Y) - max_in_value / 2;
            //log_i("Y: %i", y);
            if (y >= zeropoint || y <= -zeropoint)
            {
                motor->data[Stepper::Y]->speed = y;
                motor->data[Stepper::Y]->isforever = true;
                if (motor->data[Stepper::Y]->stopped && joystick_drive_Y == 0)
                {
                    log_i("start motor Y");
                    joystick_drive_Y = 1;
                    motor->startStepper(Stepper::Y);
                }
            }
            else if (!motor->data[Stepper::Y]->stopped && joystick_drive_Y > 0 && (y <= zeropoint || y >= -zeropoint))
            {
                log_i("stop motor Y");
                motor->stopStepper(Stepper::Y);
                joystick_drive_Y = 0;
            }
        }
    }
    
}