#include "PsXController.h"

void PsXController::setup(String mac)
{
    psx.startListening(mac);
}
void PsXController::loop()
{
    if (psx.isConnected())
    {
        if (moduleController.get(AvailableModules::led) != nullptr)
        {
            LedController *led = (LedController *)moduleController.get(AvailableModules::led);
            if (psx.event.button_down.cross)
            {
                IS_PS_CONTROLER_LEDARRAY = !led->TurnedOn();
                led->set_all(255 * IS_PS_CONTROLER_LEDARRAY, 255 * IS_PS_CONTROLER_LEDARRAY, 255 * IS_PS_CONTROLER_LEDARRAY);
                delay(1000); // Debounce?
            }
            if (psx.event.button_down.circle)
            {
                IS_PS_CONTROLER_LEDARRAY = !led->TurnedOn();
                led->set_center(255 * IS_PS_CONTROLER_LEDARRAY, 255 * IS_PS_CONTROLER_LEDARRAY, 255 * IS_PS_CONTROLER_LEDARRAY);
                delay(1000); // Debounce?
            }
        }
        /* code */

        if (moduleController.get(AvailableModules::laser) != nullptr)
        {
            LaserController *laser = (LaserController *)moduleController.get(AvailableModules::laser);
            if (psx.event.button_down.triangle)
            {
                Serial.print("Turning on LAser 10000");
                ledcWrite(laser->PWM_CHANNEL_LASER_1, 10000);
                // delay(100); // Debounce?
            }
            if (psx.event.button_down.square)
            {
                Serial.print("Turning off LAser ");
                ledcWrite(laser->PWM_CHANNEL_LASER_1, 0);
                // delay(100); // Debounce?
            }

            // FOCUS
            /*
              if (pS4Controller.event.button_down.up) {
                if (not getEnableMotor())
                  setEnableMotor(true);
                POSITION_MOTOR_X = stepper_X.currentPosition();
                stepper_X.move(POSITION_MOTOR_X+2);
                delay(100); //Debounce?
              }
              if (pS4Controller.event.button_down.down) {
                    if (not getEnableMotor())
                  setEnableMotor(true);
                POSITION_MOTOR_X = stepper_X.currentPosition();
                stepper_X.move(POSITION_MOTOR_X-2);
                delay(100); //Debounce?
              }
            */

            // LASER 1
            if (psx.event.button_down.up)
            {
                Serial.print("Turning on LAser 10000");
                ledcWrite(laser->PWM_CHANNEL_LASER_2, 20000);
                delay(100); // Debounce?
            }
            if (psx.event.button_down.down)
            {
                Serial.print("Turning off LAser ");
                ledcWrite(laser->PWM_CHANNEL_LASER_2, 0);
                delay(100); // Debounce?
            }

            // LASER 2
            if (psx.event.button_down.right)
            {
                Serial.print("Turning on LAser 10000");
                ledcWrite(laser->PWM_CHANNEL_LASER_1, 20000);
                delay(100); // Debounce?
            }
            if (psx.event.button_down.left)
            {
                Serial.print("Turning off LAser ");
                ledcWrite(laser->PWM_CHANNEL_LASER_1, 0);
                delay(100); // Debounce?
            }
        }

        if (moduleController.get(AvailableModules::motor) != nullptr)
		{
			/* code */
			FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
			// Y-Direction
			if (abs(psx.event.analog_changed.stick.ly) > offset_val)
			{
				// move_z
				stick_ly = psx.event.analog_changed.stick.ly;
				stick_ly = stick_ly - sgn(stick_ly) * offset_val;
				motor->data[Stepper::Y]->speed = stick_ly * 5 * global_speed;
				if (motor->data[Stepper::Y]->stopped)
                {
                    motor->startStepper(Stepper::Y);
                }
			}
			else if (motor->data[Stepper::Y]->speed != 0)
			{
				motor->data[Stepper::Y]->speed = 0;
				motor->steppers[Stepper::Y]->setSpeed(motor->data[Stepper::Y]->speed); // set motor off only once to not affect other modes
			}

			// Z-Direction
			if ((abs(psx.event.analog_changed.stick.rx) > offset_val))
			{
				// move_x
				stick_rx = psx.event.analog_changed.stick.rx;
				stick_rx = stick_rx - sgn(stick_rx) * offset_val;
				motor->data[Stepper::Z]->speed = stick_rx * 5 * global_speed;
				if (motor->data[Stepper::Z]->stopped)
                {
                    motor->startStepper(Stepper::Z);
                }
			}
			else if (motor->data[Stepper::Z]->speed != 0)
			{
				motor->data[Stepper::Z]->speed = 0;
				motor->steppers[Stepper::Z]->setSpeed(motor->data[Stepper::Z]->speed); // set motor off only once to not affect other modes
			}

			// X-direction
			if ((abs(psx.event.analog_changed.stick.ry) > offset_val))
			{
				// move_y
				stick_ry = psx.event.analog_changed.stick.ry;
				stick_ry = stick_ry - sgn(stick_ry) * offset_val;
				motor->data[Stepper::X]->speed = stick_ry * 5 * global_speed;
				if (motor->data[Stepper::X]->stopped)
                {
                    motor->startStepper(Stepper::X);
                }
			}
			else if (motor->data[Stepper::X]->speed != 0)
			{
				motor->data[Stepper::X]->speed = 0;
				motor->steppers[Stepper::X]->setSpeed(motor->data[Stepper::X]->speed); // set motor off only once to not affect other modes
			}
		}

        if (moduleController.get(AvailableModules::analogout) != nullptr)
		{
			AnalogOutController *analogout = (AnalogOutController *)moduleController.get(AvailableModules::analogout);
			/*
			   Keypad left
			*/
			if (psx.event.button_down.left)
			{
				// fine lens -
				analogout_val_1 -= 1;
				delay(100);
				ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
			}
			if (psx.event.button_down.right)
			{
				// fine lens +
				analogout_val_1 += 1;
				delay(100);
				ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
			}
			// unknown button
			/*if (PS4.data.button.start) {
			  // reset
			  analogout_val_1 = 0;
			  ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
			}*/

			int offset_val_shoulder = 5;
			if (abs(psx.event.button_down.r2) > offset_val_shoulder)
			{
				// analogout_val_1++ coarse
				if ((analogout_val_1 + 1000 < pwm_max))
				{
					analogout_val_1 += 1000;
					ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
				}
                Serial.println(analogout_val_1);
				//delay(100);
			}

			if (abs(psx.event.button_down.l2) > offset_val_shoulder)
			{
				// analogout_val_1-- coarse
				if ((analogout_val_1 - 1000 > 0))
				{
					analogout_val_1 -= 1000;
					ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
				}
                Serial.println(analogout_val_1);
				//delay(100);
			}

			if (abs(psx.event.button_down.l1) > offset_val_shoulder)
			{
				// analogout_val_1 + semi coarse
				if ((analogout_val_1 + 100 < pwm_max))
				{
					analogout_val_1 += 100;
					ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
					//delay(100);
				}
			}
			if (abs(psx.event.button_down.r1) > offset_val_shoulder)
			{
				// analogout_val_1 - semi coarse
				if ((analogout_val_1 - 100 > 0))
				{
					analogout_val_1 -= 100;
					ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
					//delay(50);
				}
			}
		}
		if (moduleController.get(AvailableModules::motor) != nullptr)
		{
			/* code */
			FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
			// run all motors simultaneously
			for (int i = 0; i < motor->steppers.size(); i++)
			{
				motor->steppers[i]->setSpeed(motor->data[i]->speed);
				if (motor->data[i]->speed != 0)
				{
					motor->data[i]->isforever = true;
				}
				else
					motor->data[i]->isforever = false;
			}
		}
    }
}