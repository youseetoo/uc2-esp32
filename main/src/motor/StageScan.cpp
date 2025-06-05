#include "StageScan.h"
#include "Arduino.h"
#include "FocusMotor.h"
#include "../i2c/tca_controller.h"

namespace StageScan
{
    StageScanningData stageScanningData;

    StageScanningData * getStageScanData()
    {
        return &stageScanningData;
    }
    
    void setCoordinates(StagePosition* coords, int count)
    {
        // Clear existing coordinates first
        clearCoordinates();
        
        if (coords != nullptr && count > 0)
        {
            stageScanningData.coordinates = new StagePosition[count];
            for (int i = 0; i < count; i++)
            {
                stageScanningData.coordinates[i] = coords[i];
            }
            stageScanningData.coordinateCount = count;
            stageScanningData.useCoordinates = true;
        }
    }
    
    void clearCoordinates()
    {
        if (stageScanningData.coordinates != nullptr)
        {
            delete[] stageScanningData.coordinates;
            stageScanningData.coordinates = nullptr;
        }
        stageScanningData.coordinateCount = 0;
        stageScanningData.useCoordinates = false;
    }

    void moveMotor(int stepPin, int dirPin, int steps, bool direction, int delayTimeStep)
    {
        steps = abs(steps);

        //  direction perhaps externally controlled
        if (pinConfig.I2C_SCL > -1)
        {
            #ifdef USE_TCA
            tca_controller::setExternalPin(dirPin, direction);
            #endif
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

    void stageScan(bool isThread)
    {
        FocusMotor::setEnable(true);
        
        // Get common parameters
        int delayTimeStep = stageScanningData.delayTimeStep;
        int nFrames = stageScanningData.nFrames;

        int pinDirPixel = FocusMotor::getData()[Stepper::X]->dirPin;
        int pinDirLine = FocusMotor::getData()[Stepper::Y]->dirPin;
        int pinStpPixel = FocusMotor::getData()[Stepper::X]->stpPin;
        int pinStpLine = FocusMotor::getData()[Stepper::Y]->stpPin;
        int pinTrigPixel = FocusMotor::getData()[Stepper::X]->triggerPin;
        int pinTrigLine = FocusMotor::getData()[Stepper::Y]->triggerPin;
        int pinTrigFrame = FocusMotor::getData()[Stepper::Z]->triggerPin;

        // Check if we should use coordinate-based scanning
        if (stageScanningData.useCoordinates && stageScanningData.coordinates != nullptr)
        {
            // Coordinate-based scanning
            for (int iFrame = 0; iFrame < nFrames; iFrame++)
            {
                if (stageScanningData.stopped)
                {
                    break;
                }

                // Set pins high simultaneously at frame start
                uint32_t mask = (1 << pinConfig.DIGITAL_OUT_1) | (1 << pinConfig.DIGITAL_OUT_2) | (1 << pinConfig.DIGITAL_OUT_3);
                GPIO.out_w1ts = mask; // all high

                // Store current position to return to later
                int startX = FocusMotor::getData()[Stepper::X]->currentPosition;
                int startY = FocusMotor::getData()[Stepper::Y]->currentPosition;

                for (int i = 0; i < stageScanningData.coordinateCount; i++)
                {
                    if (stageScanningData.stopped)
                    {
                        break;
                    }

                    // Calculate steps needed to reach target coordinate
                    int targetX = stageScanningData.coordinates[i].x;
                    int targetY = stageScanningData.coordinates[i].y;
                    
                    int currentX = FocusMotor::getData()[Stepper::X]->currentPosition;
                    int currentY = FocusMotor::getData()[Stepper::Y]->currentPosition;
                    
                    int stepsX = targetX - currentX;
                    int stepsY = targetY - currentY;

                    // Move to coordinate
                    if (stepsX != 0)
                    {
                        moveMotor(pinStpPixel, pinDirPixel, abs(stepsX), stepsX > 0, delayTimeStep);
                        FocusMotor::getData()[Stepper::X]->currentPosition += stepsX;
                    }
                    
                    if (stepsY != 0)
                    {
                        moveMotor(pinStpLine, pinDirLine, abs(stepsY), stepsY > 0, delayTimeStep);
                        FocusMotor::getData()[Stepper::Y]->currentPosition += stepsY;
                    }

                    // Trigger camera at this position
                    triggerOutput(pinTrigPixel);
                    triggerOutput(pinTrigLine);
                    
                    ets_delay_us(delayTimeStep * 1000); // Additional delay between positions
                }

                // Return to start position
                int currentX = FocusMotor::getData()[Stepper::X]->currentPosition;
                int currentY = FocusMotor::getData()[Stepper::Y]->currentPosition;
                
                int returnStepsX = startX - currentX;
                int returnStepsY = startY - currentY;
                
                if (returnStepsX != 0)
                {
                    moveMotor(pinStpPixel, pinDirPixel, abs(returnStepsX), returnStepsX > 0, delayTimeStep);
                    FocusMotor::getData()[Stepper::X]->currentPosition += returnStepsX;
                }
                
                if (returnStepsY != 0)
                {
                    moveMotor(pinStpLine, pinDirLine, abs(returnStepsY), returnStepsY > 0, delayTimeStep);
                    FocusMotor::getData()[Stepper::Y]->currentPosition += returnStepsY;
                }

                triggerOutput(pinTrigFrame, 0);
                ets_delay_us(10000); // Adjust delay for speed
            }
        }
        else
        {
            // Original grid-based scanning logic
            int nStepsLine = stageScanningData.nStepsLine;
            int dStepsLine = stageScanningData.dStepsLine;
            int nStepsPixel = stageScanningData.nStepsPixel;
            int dStepsPixel = stageScanningData.dStepsPixel;
            int nTriggerPixel = stageScanningData.nTriggerPixel;

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
        }

        if (isThread)
            vTaskDelete(NULL);
    }

    void stageScanThread(void *arg)
    // Grid-based scanning examples:
    // {"task": "/motor_act", "stagescan": {"nStepsLine": 100, "dStepsLine": 1, "nTriggerLine": 1, "nStepsPixel": 100, "dStepsPixel": 1, "nTriggerPixel": 1, "delayTimeStep": 5, "stopped": 0, "nFrames": 3000}}
    // Coordinate-based scanning examples:
    // {"task": "/motor_act", "stagescan": {"coordinates": [{"x": 100, "y": 200}, {"x": 300, "y": 400}], "delayTimeStep": 10, "nFrames": 1}}
    {
        stageScan(true);
    }
} // namespace StageScan
