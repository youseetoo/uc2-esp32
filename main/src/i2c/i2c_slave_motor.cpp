#include "i2c_slave_motor.h"
#include "Wire.h"
#include "PinConfig.h"
#include "../motor/FocusMotor.h"
#include "../home/HomeMotor.h"
#include "../tmc/TMCController.h"

using namespace FocusMotor;
using namespace HomeMotor;
namespace i2c_slave_motor
{

    void parseMotorEvent(int numBytes)
    {
        // incoming command from I2C master will be converted to a motor action
        if (numBytes == sizeof(MotorData))
        {
            MotorData receivedMotorData;
            uint8_t *dataPtr = (uint8_t *)&receivedMotorData;
            for (int i = 0; i < numBytes; i++)
            {
                dataPtr[i] = Wire.read();
            }
            // assign the received data to the motor to MotorData *data[4];
            Stepper mStepper = static_cast<Stepper>(pinConfig.I2C_MOTOR_AXIS);
            // FocusMotor::setData(pinConfig.I2C_MOTOR_AXIS, &receivedMotorData);
            FocusMotor::getData()[mStepper]->qid = receivedMotorData.qid;
            FocusMotor::getData()[mStepper]->isEnable = receivedMotorData.isEnable;
            FocusMotor::getData()[mStepper]->targetPosition = receivedMotorData.targetPosition;
            FocusMotor::getData()[mStepper]->absolutePosition = receivedMotorData.absolutePosition;
            FocusMotor::getData()[mStepper]->speed = receivedMotorData.speed;
            FocusMotor::getData()[mStepper]->acceleration = receivedMotorData.acceleration;
            FocusMotor::getData()[mStepper]->isforever = receivedMotorData.isforever;
            FocusMotor::getData()[mStepper]->isStop = receivedMotorData.isStop;
            // prevent the motor from getting stuck
            if (FocusMotor::getData()[mStepper]->acceleration <= 0)
            {
                FocusMotor::getData()[mStepper]->acceleration = MAX_ACCELERATION_A;
            }
            /*
            if (FocusMotor::getData()[mStepper]->speed == 0) // in case
            {
                FocusMotor::getData()[mStepper]->speed = 1000;
            }
            */
            FocusMotor::toggleStepper(mStepper, FocusMotor::getData()[mStepper]->isStop);
            log_i("Received MotorData from I2C");
            log_i("MotorData:");
            log_i("  qid: %i", (int)FocusMotor::getData()[mStepper]->qid);
            log_i("  isEnable: %i", (bool)FocusMotor::getData()[mStepper]->isEnable);
            log_i("  targetPosition: %i", (int)FocusMotor::getData()[mStepper]->targetPosition);
            log_i("  absolutePosition: %i", (bool)FocusMotor::getData()[mStepper]->absolutePosition);
            log_i("  speed: %i", (int)FocusMotor::getData()[mStepper]->speed);
            log_i("  acceleration: %i", (bool)FocusMotor::getData()[mStepper]->acceleration);
            log_i("  isforever: %i", (bool)FocusMotor::getData()[mStepper]->isforever);
            log_i("  isEnable: %i", (bool)FocusMotor::getData()[mStepper]->isEnable);
            log_i("  isStop: %i", (bool)FocusMotor::getData()[mStepper]->isStop);

            // Now `receivedMotorData` contains the deserialized data
            // You can process `receivedMotorData` as needed
            // bool isStop = receivedMotorData.isStop;
        }
        else if (numBytes == sizeof(HomeData))
        {
            // parse a possible home event
            log_i("Received HomeData from I2C");
            HomeData receivedHomeData;
            uint8_t *dataPtr = (uint8_t *)&receivedHomeData;
            for (int i = 0; i < numBytes; i++)
            {
                dataPtr[i] = Wire.read();
            }
            // assign the received data to the motor to MotorData *data[4];
            Stepper mStepper = static_cast<Stepper>(pinConfig.I2C_MOTOR_AXIS);
            int homeTimeout = receivedHomeData.homeTimeout;
            int homeSpeed = receivedHomeData.homeSpeed;
            int homeMaxspeed = receivedHomeData.homeMaxspeed;
            int homeDirection = receivedHomeData.homeDirection;
            int homeEndStopPolarity = receivedHomeData.homeEndStopPolarity;
            HomeMotor::startHome(mStepper, homeTimeout, homeSpeed, homeMaxspeed, homeDirection, homeEndStopPolarity);
            
        }
        else if (numBytes == sizeof(TMCData))
        {
            // parse a possible TMC event
            log_i("Received TMCData from I2C");
            TMCData receivedTMCData;
            uint8_t *dataPtr = (uint8_t *)&receivedTMCData;
            for (int i = 0; i < numBytes; i++)
            {
                dataPtr[i] = Wire.read();
            }
            // assign the received data to the motor to MotorData *data[4];
            TMCController::setTMCData(receivedTMCData);
        }
        else
        {
            // Handle error: received data size does not match expected size
            log_e("Error: Received data size does not match MotorData size.");
        }
    }

    void receiveEvent(int numBytes)
    {
        // Master and Slave
        // log_i("Receive Event");
        if (pinConfig.I2C_CONTROLLER_TYPE == I2CControllerType::mMOTOR)
        {
            // Motor, Home, TMC Events
            parseMotorEvent(numBytes);
        }
        else
        {
            // Handle error: I2C controller type not supported
            log_e("Error: I2C controller type not supported.");
        }
    }

    void requestEvent()
    {
        // The master request data from the slave
        // !THIS IS ONLY EXECUTED IN I2C SLAVE MODE!
        // for the motor we would need to send the current position and the state of isRunning
        if (pinConfig.I2C_CONTROLLER_TYPE == I2CControllerType::mMOTOR)
        {
            // The master request data from the slave
            MotorState motorState;
            bool isRunning = !FocusMotor::getData()[pinConfig.I2C_MOTOR_AXIS]->stopped;
            long currentPosition = FocusMotor::getData()[pinConfig.I2C_MOTOR_AXIS]->currentPosition;
            motorState.currentPosition = currentPosition;
            motorState.isRunning = isRunning;
            // Serial.println("motor is running: " + String(motorState.isRunning));
            Wire.write((uint8_t *)&motorState, sizeof(MotorState));
        }
        else
        {
            // Handle error: I2C controller type not supported
            log_e("Error: I2C controller type not supported.");
            Wire.write(0);
        }
    }

    void setI2CAddress(int address)
    {
#ifdef I2C_SLAVE_MOTOR
        // Set the I2C address of the slave
        // Save the I2C address to the preference
        Preferences preferences;
        preferences.begin("I2C", false);
        preferences.putInt("address", address);
        preferences.end();
        ESP.restart();
#endif
    }

    int getI2CAddress()
    {
#ifdef I2C_SLAVE_MOTOR
        // Get the I2C address of the slave
        // Load the I2C address form the preference
        Preferences preferences;
        preferences.begin("I2C", false);
        int address = preferences.getInt("address", pinConfig.I2C_ADD_SLAVE);
        preferences.end();
        return address;
#else
        return -1;
#endif
    }

    void setup()
    {

        // Begin I2C slave communication with the defined pins and address
        // log_i("I2C Slave mode on address %i", pinConfig.I2C_ADD_SLAVE);
        Wire.begin(getI2CAddress(), pinConfig.I2C_SDA, pinConfig.I2C_SCL, 100000);
        Wire.onReceive(receiveEvent);
        Wire.onRequest(requestEvent);
        log_i("Starting I2C as slave on address: %i", getI2CAddress());
    }
}