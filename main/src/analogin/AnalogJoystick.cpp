#include "AnalogJoystick.h"

namespace RestApi
{
    void AnalogJoystick_set()
    {
        serialize(moduleController.get(AvailableModules::analogJoystick)->set(deserialize()));
    }
    void AnalogJoystick_get()
    {
        serialize(moduleController.get(AvailableModules::analogJoystick)->get(deserialize()));
    }
};

AnalogJoystick::AnalogJoystick(/* args */){};
AnalogJoystick::~AnalogJoystick(){};
void AnalogJoystick::setup()
{
    pins =new JoystickPins();
    Config::getAnalogJoyStickPins(pins);
    pinMode(pins->x_pin, INPUT);
    pinMode(pins->y_pin, INPUT);
}
int AnalogJoystick::act(DynamicJsonDocument jsonDocument) { return 1;}

int AnalogJoystick::set(DynamicJsonDocument doc) {
	if (doc.containsKey(key_joy))
    {
        if((doc)[key_joy].containsKey(key_joiypinX))
            pins->x_pin = (doc)[key_joy][key_joiypinX];
        if((doc)[key_joy].containsKey(key_joiypinY))
            pins->y_pin = (doc)[key_joy][key_joiypinY];
    }
    doc.clear();
    Config::setAnalogJoyStickPins(pins);
    setup();
    return 1;
}

DynamicJsonDocument AnalogJoystick::get(DynamicJsonDocument  doc) {
    doc.clear();
    doc[key_joy][key_joiypinX] = pins->x_pin;
    doc[key_joy][key_joiypinY] = pins->y_pin;
    return doc;
}

void AnalogJoystick::loop()
{

    // log_i("X: %i Y: %i", x, y);
    if (moduleController.get(AvailableModules::motor) != nullptr)
    {
        FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
        if (pins->x_pin > 0)
        {
            int x = analogRead(pins->x_pin) - 4096 / 2;
            if (x >= 200 || x <= -200)
            {
                motor->data[Stepper::X]->speed = x;
                motor->data[Stepper::X]->isforever = true;
                joystick_drive_X = true;
                if (motor->data[Stepper::X]->stopped)
                {
                    motor->startStepper(Stepper::X);
                }
            }
            else if ((x <= 200 || x >= -200) && !motor->data[Stepper::X]->stopped && joystick_drive_X)
            {
                log_i("stop motor X");
                motor->stopStepper(Stepper::X);
                joystick_drive_X = false;
            }
        }
        if (pins->y_pin > 0)
        {
            int y = analogRead(pins->y_pin) - 4096 / 2;
            if (y >= 200 || y <= -200)
            {
                motor->data[Stepper::Y]->speed = y;
                motor->data[Stepper::Y]->isforever = true;
                joystick_drive_Y = true;
                if (motor->data[Stepper::Y]->stopped)
                    motor->startStepper(Stepper::Y);
            }
            else if ((y <= 200 || y >= -200) && !motor->data[Stepper::Y]->stopped && joystick_drive_Y)
            {
                log_i("stop motor Y");
                motor->stopStepper(Stepper::Y);
                joystick_drive_Y = false;
            }
        }
    }
}