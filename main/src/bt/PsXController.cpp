#include "PsXController.h"

void PsXController::setup(String mac, int type)
{
	// select the type of wifi controller - either PS3 or PS4
	if(type == 1)
		psx = new PSController(nullptr, PSController::kPS3);
	else if(type == 2)
		psx = new PSController(nullptr, PSController::kPS4);
    psx->startListening(mac);
}

void PsXController::loop()
{
	// This is called in every MCU cycle

	// This ensures the mapping of physical inputs (e.g. buttons) to logical outputs (e.g. motor)

	//log_i("psx connected:%i",psx.isConnected());
    if (psx != nullptr && psx->isConnected())
    {
        if (moduleController.get(AvailableModules::led) != nullptr)
        {
			// switch LED on/off on cross/circle button press
            LedController *led = (LedController *)moduleController.get(AvailableModules::led);
            if (psx->event.button_down.cross)
            {
				log_i("Turn on LED ");
                IS_PS_CONTROLER_LEDARRAY = !led->TurnedOn();
                led->set_all(255, 255, 255);
            }
            if (psx->event.button_down.circle)
            {
				log_i("Turn off LED ");
                IS_PS_CONTROLER_LEDARRAY = !led->TurnedOn();
                led->set_all(0,0,0);
            }
        }
        /* code */

        if (moduleController.get(AvailableModules::laser) != nullptr)
        {
			
            LaserController *laser = (LaserController *)moduleController.get(AvailableModules::laser);
            
            // LASER 1
			// switch laser 1 on/off on triangle/square button press
            if (psx->event.button_down.up)
            {
				// Switch laser 2 on/off on up/down button press
                Serial.print("Turning on LAser 10000");
                ledcWrite(laser->PWM_CHANNEL_LASER_2, 20000);
            }
            if (psx->event.button_down.down)
            {
                Serial.print("Turning off LAser ");
                ledcWrite(laser->PWM_CHANNEL_LASER_2, 0);
            }

            // LASER 2
			// switch laser 2 on/off on triangle/square button press
            if (psx->event.button_down.right)
            {
                Serial.print("Turning on LAser 10000");
                ledcWrite(laser->PWM_CHANNEL_LASER_1, 20000);
            }
            if (psx->event.button_down.left)
            {
                Serial.print("Turning off LAser ");
                ledcWrite(laser->PWM_CHANNEL_LASER_1, 0);
            }
        }

		// MOTORS
        if (moduleController.get(AvailableModules::motor) != nullptr)
		{
			/* code */
			FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
			// Z-Direction
			if (abs(psx->state.analog.stick.ly) > offset_val)
			{
				stick_ly = psx->state.analog.stick.ly;
				stick_ly = stick_ly - sgn(stick_ly) * offset_val;
				if(abs(stick_ly)>50)stick_ly=2*stick_ly; // add more speed above threshold
				motor->data[Stepper::Z]->speed = stick_ly * global_speed;
				//Serial.println(motor->data[Stepper::Z]->speed);
				motor->data[Stepper::Z]->isforever = true;
				joystick_drive_Z = true;
				if (motor->data[Stepper::Z]->stopped)
                {
                    motor->startStepper(Stepper::Z);
                }
			}
			else if (motor->data[Stepper::Z]->speed != 0 && joystick_drive_Z)
			{
				motor->stopStepper(Stepper::Z);
				joystick_drive_Z = false;
			}

			// X-Direction
			if ((abs(psx->state.analog.stick.rx) > offset_val))
			{
				// move_x
				stick_rx = psx->state.analog.stick.rx;
				stick_rx = stick_rx - sgn(stick_rx) * offset_val;
				motor->data[Stepper::X]->speed = stick_rx * global_speed;
				if(abs(stick_rx)>50)stick_rx=2*stick_rx; // add more speed above threshold
				motor->data[Stepper::X]->isforever = true;
				joystick_drive_X = true;
				if (motor->data[Stepper::X]->stopped)
                {
                    motor->startStepper(Stepper::X);
                }
			}
			else if (motor->data[Stepper::X]->speed != 0 && joystick_drive_X)
			{
				motor->stopStepper(Stepper::X);
				joystick_drive_X = false;
			}

			// Y-direction
			if ((abs(psx->state.analog.stick.ry) > offset_val))
			{
				stick_ry = psx->state.analog.stick.ry;
				stick_ry = stick_ry - sgn(stick_ry) * offset_val;
				motor->data[Stepper::Y]->speed = stick_ry * global_speed;
				if(abs(stick_ry)>50)stick_ry=2*stick_ry; // add more speed above threshold
				motor->data[Stepper::Y]->isforever = true;
				joystick_drive_Y = true;
				if (motor->data[Stepper::Y]->stopped)
                {
                    motor->startStepper(Stepper::Y);
                }
			}
			else if (motor->data[Stepper::Y]->speed != 0 && joystick_drive_Y)
			{
				motor->stopStepper(Stepper::Y);
				joystick_drive_Y = false;
			}
		}

        if (moduleController.get(AvailableModules::analogout) != nullptr)
		{
			AnalogOutController *analogout = (AnalogOutController *)moduleController.get(AvailableModules::analogout);
			/*
			   Keypad left
			*/
			if (psx->event.button_down.left)
			{
				// fine lens -
				analogout_val_1 -= 1;
				delay(50);
				ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
			}
			if (psx->event.button_down.right)
			{
				// fine lens +
				analogout_val_1 += 1;
				delay(50);
				ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
			}
			// unknown button
			/*if (PS4.data.button.start) {
			  // reset
			  analogout_val_1 = 0;
			  ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
			}*/

			int offset_val_shoulder = 5;
			if (abs(psx->event.button_down.r2) > offset_val_shoulder)
			{
				// analogout_val_1++ coarse
				if ((analogout_val_1 + 1000 < pwm_max))
				{
					analogout_val_1 += 1000;
					ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
				}
                //Serial.println(analogout_val_1);
				//delay(100);
			}

			if (abs(psx->event.button_down.l2) > offset_val_shoulder)
			{
				// analogout_val_1-- coarse
				if ((analogout_val_1 - 1000 > 0))
				{
					analogout_val_1 -= 1000;
					ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
				}
                //Serial.println(analogout_val_1);
				//delay(100);
			}

			if (abs(psx->event.button_down.l1) > offset_val_shoulder)
			{
				// analogout_val_1 + semi coarse
				if ((analogout_val_1 + 100 < pwm_max))
				{
					analogout_val_1 += 100;
					ledcWrite(analogout->PWM_CHANNEL_analogout_1, analogout_val_1);
					//delay(100);
				}
			}
			if (abs(psx->event.button_down.r1) > offset_val_shoulder)
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

					/*
			if (psx->event.button_down.triangle)
            {
                Serial.print("Turning on LAser 10000");
                ledcWrite(laser->PWM_CHANNEL_LASER_1, 10000);
                // delay(100); // Debounce?
            }
            if (psx->event.button_down.square)
            {
                Serial.print("Turning off LAser ");
                ledcWrite(laser->PWM_CHANNEL_LASER_1, 0);
                // delay(100); // Debounce?
            }
			*/

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


		/*if (moduleController.get(AvailableModules::motor) != nullptr)
		{
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
		}*/
    }
}