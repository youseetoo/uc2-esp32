#include "AnalogJoystick.h"
#include "../../ModuleController.h"
#include "../motor/FocusMotor.h"

#include "cJSON.h"
#include "../encoder/InterruptController.h"

static const char * JoyTAG = "AnalogJoystick";

AnalogJoystick::AnalogJoystick(/* args */){};
AnalogJoystick::~AnalogJoystick(){};

void processEvent(uint8_t pin)
{
    log_d("processEvent from pin %i", pin);
    AnalogJoystick *j = (AnalogJoystick *)moduleController.get(AvailableModules::analogJoystick);
    if (pin == pinConfig.ANLOG_JOYSTICK_X)
        j->driveMotor(Stepper::X, j->joystick_drive_X, pin);
    if (pin == pinConfig.ANLOG_JOYSTICK_Y)
        j->driveMotor(Stepper::Y, j->joystick_drive_Y, pin);
}

void AnalogJoystick::setup()
{
    ESP_LOGI(JoyTAG,"Setup analog joystick");
    addInterruptListener(pinConfig.ANLOG_JOYSTICK_X, (Listener)&processEvent, gpio_int_type_t::GPIO_INTR_ANYEDGE);
    addInterruptListener(pinConfig.ANLOG_JOYSTICK_Y, (Listener)&processEvent, gpio_int_type_t::GPIO_INTR_ANYEDGE);
}
int AnalogJoystick::act(cJSON *jsonDocument) { return 1; }

/*"joy" :
    {
        "joyX" : 1,
        "joyY" : 1
    }*/
cJSON *AnalogJoystick::get(cJSON *doc)
{

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

void AnalogJoystick::driveMotor(Stepper s, int joystick_drive, int pin)
{
    
    if (moduleController.get(AvailableModules::motor) != nullptr)
    {
        FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
        int val = analogRead(pin) - max_in_value / 2;
        ESP_LOGI(JoyTAG,"drive motor :%i x drive:%i , x:%i", motor->data[s]->stopped, joystick_drive, val);
        if (val >= zeropoint || val <= -zeropoint)
        {
            motor->data[s]->speed = val;
            motor->data[s]->isforever = true;

            if (motor->data[s]->stopped && joystick_drive_X == 0)
            {
                ESP_LOGI(JoyTAG,"start motor X");
                joystick_drive = 1;
                motor->startStepper(s);
            }
        }
        else if (!motor->data[s]->stopped && joystick_drive > 0 && (val <= zeropoint || val >= -zeropoint))
        {
            ESP_LOGI(JoyTAG,"stop motor X stopped:%i x drive:%i , x:%i", motor->data[s]->stopped, joystick_drive, val);
            motor->stopStepper(s);
            joystick_drive = 0;
        }
    }
}

void AnalogJoystick::loop()
{
    
}