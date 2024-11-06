#include <PinConfig.h>

#include "AnalogJoystick.h"
#include "Arduino.h"
#include "../../JsonKeys.h"
#ifdef MOTOR_CONTROLLER
#include "../motor/FocusMotor.h"
#include "../motor/MotorTypes.h"
#endif
#include "../encoder/InterruptController.h"

namespace AnalogJoystick
{
    using namespace InterruptController;
    const char *TAG = "AnalogJoystick";

    void processEvent(uint8_t pin)
    {
        ESP_LOGI(TAG, "processEvent from pin %i", pin);
        if (pin == pinConfig.ANLOG_JOYSTICK_X)
            driveMotor(Stepper::X, joystick_drive_X, pin);
        if (pin == pinConfig.ANLOG_JOYSTICK_Y)
            driveMotor(Stepper::Y, joystick_drive_Y, pin);
    }

    void setup()
    {
        ESP_LOGI(TAG, "Setup analog joystick");
        addInterruptListner(pinConfig.ANLOG_JOYSTICK_X, (Listner)&processEvent, gpio_int_type_t::GPIO_INTR_ANYEDGE);
        addInterruptListner(pinConfig.ANLOG_JOYSTICK_Y, (Listner)&processEvent, gpio_int_type_t::GPIO_INTR_ANYEDGE);
    }

    void driveMotor(Stepper s, int joystick_drive, int pin)
    {
#ifdef MOTOR_CONTROLLER

        int val = analogRead(pin) - max_in_value / 2;
        ESP_LOGI(TAG, "drive motor :%i x drive:%i , x:%i", FocusMotor::getData()[s]->stopped, joystick_drive, val);
        if (val >= zeropoint || val <= -zeropoint)
        {
            FocusMotor::getData()[s]->speed = val;
            FocusMotor::getData()[s]->isforever = true;

            if (FocusMotor::getData()[s]->stopped && joystick_drive_X == 0)
            {
                ESP_LOGI(TAG, "start motor X");
                joystick_drive = 1;
                FocusMotor::startStepper(s);
            }
        }
        else if (!FocusMotor::getData()[s]->stopped && joystick_drive > 0 && (val <= zeropoint || val >= -zeropoint))
        {
            ESP_LOGI(TAG, "stop motor X stopped:%i x drive:%i , x:%i", FocusMotor::getData()[s]->stopped, joystick_drive, val);
            FocusMotor::stopStepper(s);
            joystick_drive = 0;
        }

#endif
    }
}
