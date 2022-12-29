#include "AnalogJoystick.h"

namespace RestApi
{
    void AnalogJoystick_get()
    {
        serialize(moduleController.get(AvailableModules::analogJoystick)->get(deserialize()));
    }
};

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
    pinMode(pinConfig.ANLOG_JOYSTICK_X, INPUT);
    pinMode(pinConfig.ANLOG_JOYSTICK_Y, INPUT);
    xTaskCreate(&processLoopAJoy, "analogJoyStick_task", 2048, NULL, 5, NULL);
}
int AnalogJoystick::act(DynamicJsonDocument jsonDocument) { return 1;}

DynamicJsonDocument AnalogJoystick::get(DynamicJsonDocument  doc) {
    doc.clear();
    doc[key_joy][key_joiypinX] = pinConfig.ANLOG_JOYSTICK_X;
    doc[key_joy][key_joiypinY] = pinConfig.ANLOG_JOYSTICK_Y;
    return doc;
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