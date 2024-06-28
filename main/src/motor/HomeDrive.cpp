#include "HomeDrive.h"
#include "MotorTypes.h"
#include "FocusMotor.h"

namespace HomeDrive
{
    bool taskRunning[4];

    void sendHomeDone(int axis)
	{
		// send home done to client
		cJSON *json = cJSON_CreateObject();
		cJSON *home = cJSON_CreateObject();
		cJSON_AddItemToObject(json, "home", home);
		cJSON *steppers = cJSON_CreateObject();
		cJSON_AddItemToObject(home, key_steppers, steppers);
		cJSON *axs = cJSON_CreateNumber(axis);
		cJSON *done = cJSON_CreateNumber(true);
		cJSON_AddItemToObject(steppers, "axis", axs);
		cJSON_AddItemToObject(steppers, "isDone", done);
		Serial.println("++");
		char *ret = cJSON_Print(json);
		cJSON_Delete(json);
		Serial.println(ret);
		free(ret);
		Serial.println("--");
	}

    void driveHomeTask(int stepper, int speed, int dir)
    {
        taskRunning[stepper] = true;
        FocusMotor::getData()[stepper]->isforever = true;
        FocusMotor::getData()[stepper]->speed = speed * dir;
        FocusMotor::startStepper(stepper);
        bool searchHome = true;
        while (searchHome)
        {
            if (FocusMotor::getData()[stepper]->endstop_hit)
            {
                searchHome = false;
                FocusMotor::stopStepper(stepper);
            }
            
        }
        vTaskDelay(30);
        FocusMotor::getData()[stepper]->isforever = true;
        FocusMotor::getData()[stepper]->speed = speed * dir * -1;
        FocusMotor::startStepper(stepper);
        while (!searchHome)
        {
            if(!FocusMotor::getData()[stepper]->endstop_hit)
            {
                searchHome = true;
                FocusMotor::stopStepper(stepper);
                FocusMotor::getData()[stepper]->currentPosition = 0;
                FocusMotor::getData()[stepper]->targetPosition = 0;
            }
            vTaskDelay(1);
        }
        taskRunning[stepper] = false;
        sendHomeDone(stepper);
        vTaskDelete(NULL);
    }

    void driveHomeX(void * p)
    {
        driveHomeTask(Stepper::X, pinConfig.HOME_X_SPEED, pinConfig.HOME_X_DIR);
    }
    void driveHomeY(void * p)
    {
        driveHomeTask(Stepper::Y, pinConfig.HOME_Y_SPEED, pinConfig.HOME_Y_DIR);
    }
    void driveHomeZ(void * p)
    {
        driveHomeTask(Stepper::Z, pinConfig.HOME_Z_SPEED, pinConfig.HOME_Z_DIR);
    }

    void driveHome(int i)
    {
        if(taskRunning[i])
            return;
        if (i == Stepper::X)
        {
            xTaskCreate(&driveHomeX, "home_task_X", pinConfig.MOTOR_TASK_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY, NULL);
        }
        if (i == Stepper::Y)
        {
            xTaskCreate(&driveHomeY, "home_task_Y", pinConfig.MOTOR_TASK_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY, NULL);
        }
        if (i == Stepper::Z)
        {
            xTaskCreate(&driveHomeZ, "home_task_Z", pinConfig.MOTOR_TASK_STACKSIZE, NULL, pinConfig.DEFAULT_TASK_PRIORITY, NULL);
        }
    }
} // namespace HomeDrive
