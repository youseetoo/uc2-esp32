#include "StageScan.h"
#include "Arduino.h"
#include "FocusMotor.h"

namespace StageScan
{
    StageScanningData stageScanningData;

    StageScanningData * getStageScanData()
    {
        return &stageScanningData;
    }

    void moveMotor(int stepPin, int dirPin, int steps, bool direction, int delayTimeStep)
    {
        steps = abs(steps);

        //  direction perhaps externally controlled
        if (pinConfig.I2C_SCL > -1)
        {
            FocusMotor::setExternalPin(dirPin, direction);
        }
        else
        {
            digitalWrite(dirPin, direction ? HIGH : LOW);
        }

        for (int i = 0; i < steps; ++i)
        {
            digitalWrite(stepPin, HIGH);
            ets_delay_us(delayTimeStep); // Adjust delay for speed
            digitalWrite(stepPin, LOW);
            ets_delay_us(delayTimeStep); // Adjust delay for speed
        }
    }

    void writeToPin(int digitaloutid, int digitaloutval, int triggerdelay)
    {
        if (digitaloutval == -1)
		{
			// perform trigger
			digitalWrite(digitaloutid, HIGH);
			delay(triggerdelay);
			digitalWrite(digitaloutid, LOW);
		}
		else
		{
			digitalWrite(digitaloutid, digitaloutval);
		}
    }

    void triggerOutput(int outputPin, int state = -1)
    {
        // if state is -1 we alternate from 0,1,0
        // Output trigger logic
        if (state == -1)
        {
            writeToPin(outputPin, 1, 0);
            ets_delay_us(1); // Adjust delay for speed
            writeToPin(outputPin, 0, 0);
        }
        else
        {
            writeToPin(outputPin, state, 0);
        }
    }

    void stageScan(bool isThread = false)
    {

        FocusMotor::enable(true);
        // Scanning logic
        int nStepsLine = stageScanningData.nStepsLine;
        int dStepsLine = stageScanningData.dStepsLine;
        int nStepsPixel = stageScanningData.nStepsPixel;
        int dStepsPixel = stageScanningData.dStepsPixel;
        int nTriggerPixel = stageScanningData.nTriggerPixel;
        int delayTimeStep = stageScanningData.delayTimeStep;
        int nFrames = stageScanningData.nFrames;

        int pinDirPixel = FocusMotor::getData()[Stepper::X]->dirPin;
        int pinDirLine = FocusMotor::getData()[Stepper::Y]->dirPin;
        int pinStpPixel = FocusMotor::getData()[Stepper::X]->stpPin;
        int pinStpLine = FocusMotor::getData()[Stepper::Y]->stpPin;
        int pinTrigPixel = FocusMotor::getData()[Stepper::X]->triggerPin;
        int pinTrigLine = FocusMotor::getData()[Stepper::Y]->triggerPin;
        int pinTrigFrame = FocusMotor::getData()[Stepper::Z]->triggerPin;

        for (int iFrame = 0; iFrame < nFrames; iFrame++)
        {
            // frameclock

            int stepCounterPixel = 0;
            int stepCounterLine = 0;
            bool directionX = 0;
            int iPixel = 0;

            // Set pins high simultaenously at frame start
            uint32_t mask = (1 << pinConfig.DIGITAL_OUT_1) | (1 << pinConfig.DIGITAL_OUT_2) | (1 << pinConfig.DIGITAL_OUT_3);
            GPIO.out_w1ts = mask; // all high

            for (int iLine = 0; iLine < nStepsLine; iLine += dStepsLine)
            {
                // pulses the line trigger (or switch off in the first round)
                triggerOutput(pinTrigLine);

                for (iPixel = 0; iPixel < nStepsPixel; iPixel += dStepsPixel)
                {

                    if (stageScanningData.stopped)
                    {
                        break;
                    }
                    // pulses the pixel trigger (or switch off in the first round)
                    triggerOutput(pinTrigPixel);

                    // Move X motor forward at even steps, backward at odd steps
                    // bool directionX = iLine % 2 == 0;
                    moveMotor(pinStpPixel, pinDirPixel, dStepsPixel, directionX, delayTimeStep);
                    // stepCounterPixel += (dStepsPixel * (directionX ? 1 : -1));
                }

                // move back x stepper by step counter
                moveMotor(pinStpPixel, pinDirPixel, iPixel, !directionX, (2 + delayTimeStep) * 10);

                // Move Y motor after each line
                bool directionY = 0;
                moveMotor(pinStpLine, pinDirLine, dStepsLine, directionY, delayTimeStep);
            }

            // Reset Position and move back to origin
            /*
            getData()[Stepper::X]->currentPosition += stepCounterPixel;
            getData()[Stepper::Y]->currentPosition += stepCounterLine;
            moveMotor(pinStpPixel, pinDirPixel, stepCounterPixel, stepCounterLine > 0, delayTimeStep*10);
            */
            triggerOutput(pinTrigFrame, 0);

            moveMotor(pinStpLine, pinDirLine, nStepsLine, stepCounterLine > 0, (2 + delayTimeStep) * 10);
            FocusMotor::getData()[Stepper::X]->currentPosition -= stepCounterPixel;
            FocusMotor::getData()[Stepper::Y]->currentPosition -= stepCounterLine;
            ets_delay_us(10000); // Adjust delay for speed
        }

        if (isThread)
            vTaskDelete(NULL);
    }

    void stageScanThread(void *arg)
    //{"task": "/motor_act", "stagescan": {"nStepsLine": 100, "dStepsLine": 1, "nTriggerLine": 1, "nStepsPixel": 100, "dStepsPixel": 1, "nTriggerPixel": 1, "delayTimeStep": 5, "stopped": 0, "nFrames": 3000}}}
    { // {"task": "/motor_act", "stagescan": {"nStepsLine": 50, "dStepsLine": 1, "nTriggerLine": 1, "nStepsPixel": 50, "dStepsPixel": 1, "nTriggerPixel": 1, "delayTimeStep": 1, "stopped": 0, "nFrames": 50}}}
        // {"task": "/motor_act", "stagescan": {"nStepsLine": 5, "dStepsLine": 1, "nTriggerLine": 1, "nStepsPixel": 13, "dStepsPixel": 1, "nTriggerPixel": 1, "delayTimeStep": 1, "stopped": 0, "nFrames": 50}}}
        // {"task": "/motor_act", "stagescan": {"nStepsLine": 16, "dStepsLine": 1, "nTriggerLine": 1, "nStepsPixel": 16, "dStepsPixel": 1, "nTriggerPixel": 1, "delayTimeStep": 1, "stopped": 0, "nFrames": 50}}}
        // {"task": "/motor_act", "stagescan": {"stopped": 1"}}
        // {"task": "/motor_act", "stagescan": {"nStepsLine": 1{"task": "/motor_act", "stagescan": {"nStepsLine": 10, "dStepsLine": 1, "nTriggerLine": 1, "nStepsPixel": 10, "dStepsPixel": 1, "nTriggerPixel": 1, "delayTimeStep": 10, "stopped": 0, "nFrames": 5}}"}}0, "dStepsLine": 1, "nTriggerLine": 1, "nStepsPixel": 10, "dStepsPixel": 1, "nTriggerPixel": 1, "delayTimeStep": 10, "stopped": 0, "nFrames": 5}}"}}
        stageScan(true);
    }
} // namespace FocusMotor
