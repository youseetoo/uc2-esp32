#include <PinConfig.h>

#include "AnalogJoystick.h"
#include "Arduino.h"
#include "../../JsonKeys.h"
#ifdef FOCUS_MOTOR
#include "../motor/FocusMotor.h"
#include "../motor/MotorTypes.h"
#endif
#include "../encoder/InterruptController.h"


namespace AnalogJoystick
{

    static const char *JoyTAG = "AnalogJoystick";

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
        ESP_LOGI(JoyTAG, "Setup analog joystick");
        addInterruptListner(pinConfig.ANLOG_JOYSTICK_X, (Listner)&processEvent, gpio_int_type_t::GPIO_INTR_ANYEDGE);
        addInterruptListner(pinConfig.ANLOG_JOYSTICK_Y, (Listner)&processEvent, gpio_int_type_t::GPIO_INTR_ANYEDGE);
    }

    void driveMotor(Stepper s, int joystick_drive, int pin)
    {
#ifdef FOCUS_MOTOR

        int val = analogRead(pin) - max_in_value / 2;
        ESP_LOGI(JoyTAG, "drive motor :%i x drive:%i , x:%i", data[s]->stopped, joystick_drive, val);
        if (val >= zeropoint || val <= -zeropoint)
        {
            data[s]->speed = val;
            data[s]->isforever = true;

            if (data[s]->stopped && joystick_drive_X == 0)
            {
                ESP_LOGI(JoyTAG, "start motor X");
                joystick_drive = 1;
                FocusMotor::startStepper(s);
            }
        }
        else if (!data[s]->stopped && joystick_drive > 0 && (val <= zeropoint || val >= -zeropoint))
        {
            ESP_LOGI(JoyTAG, "stop motor X stopped:%i x drive:%i , x:%i", data[s]->stopped, joystick_drive, val);
            FocusMotor::stopStepper(s);
            joystick_drive = 0;
        }

#endif
    }
}
