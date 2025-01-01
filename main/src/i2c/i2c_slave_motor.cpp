#include "i2c_slave_motor.h"
#include "Wire.h"
#include "PinConfig.h"
#include "../motor/FocusMotor.h"
#include "../home/HomeMotor.h"
#include "../tmc/TMCController.h"
#include "../state/State.h"

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
            Stepper mStepper = static_cast<Stepper>(pinConfig.REMOTE_MOTOR_AXIS_ID);
            // FocusMotor::setData(pinConfig.REMOTE_MOTOR_AXIS_ID, &receivedMotorData);
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
            log_i("Received MotorData from I2C, isEnable: %i, targetPosition: %i, absolutePosition: %i, speed: %i, acceleration: %i, isforever: %i, isStop: %i", receivedMotorData.isEnable, receivedMotorData.targetPosition, receivedMotorData.absolutePosition, receivedMotorData.speed, receivedMotorData.acceleration, receivedMotorData.isforever, receivedMotorData.isStop);
            FocusMotor::toggleStepper(mStepper, FocusMotor::getData()[mStepper]->isStop, false);
            // Now `receivedMotorData` contains the deserialized data
            // You can process `receivedMotorData` as needed
            // bool isStop = receivedMotorData.isStop;
        }
        else if (numBytes == sizeof(long))
        {
            // we forcefully set the position on the slave
            long motorPosition;
            uint8_t *dataPtr = (uint8_t *)&motorPosition;
            for (int i = 0; i < numBytes; i++)
            {
                dataPtr[i] = Wire.read();
            }
            Stepper mStepper = static_cast<Stepper>(pinConfig.REMOTE_MOTOR_AXIS_ID);
            FocusMotor::getData()[mStepper]->currentPosition = motorPosition;
            log_i("Received MotorPosition from I2C %i", motorPosition);
        }
        else if (numBytes == sizeof(MotorDataReduced))
        {
            // parse a possible motor event
            MotorDataReduced receivedMotorData;
            uint8_t *dataPtr = (uint8_t *)&receivedMotorData;
            for (int i = 0; i < numBytes; i++)
            {
                dataPtr[i] = Wire.read();
            }
            // assign the received data to the motor to MotorData *data[4];
            Stepper mStepper = static_cast<Stepper>(pinConfig.REMOTE_MOTOR_AXIS_ID);
            FocusMotor::getData()[mStepper]->targetPosition = receivedMotorData.targetPosition;
            FocusMotor::getData()[mStepper]->isforever = receivedMotorData.isforever;
            FocusMotor::getData()[mStepper]->absolutePosition = receivedMotorData.absolutePosition;
            FocusMotor::getData()[mStepper]->speed = receivedMotorData.speed;
            FocusMotor::getData()[mStepper]->isStop = receivedMotorData.isStop;
            FocusMotor::getData()[mStepper]->acceleration = MAX_ACCELERATION_A;
            FocusMotor::toggleStepper(mStepper, FocusMotor::getData()[mStepper]->isStop, false);
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
            Stepper mStepper = static_cast<Stepper>(pinConfig.REMOTE_MOTOR_AXIS_ID);
            int homeTimeout = receivedHomeData.homeTimeout;
            int homeSpeed = receivedHomeData.homeSpeed;
            int homeMaxspeed = receivedHomeData.homeMaxspeed;
            int homeDirection = receivedHomeData.homeDirection;
            int homeEndStopPolarity = receivedHomeData.homeEndStopPolarity;
            log_i("Received HomeData from I2C, homeTimeout: %i, homeSpeed: %i, homeMaxspeed: %i, homeDirection: %i, homeEndStopPolarity: %i", homeTimeout, homeSpeed, homeMaxspeed, homeDirection, homeEndStopPolarity);
            HomeMotor::startHome(mStepper, homeTimeout, homeSpeed, homeMaxspeed, homeDirection, homeEndStopPolarity, 0, false);
            // TODO: absolutely no clue, but it seems the first one does have a wrong state and does not run 
            //delay(50);
            //HomeMotor::startHome(mStepper, homeTimeout, homeSpeed, homeMaxspeed, homeDirection, homeEndStopPolarity, 0, false);

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

        // if we receive one byte, it is the request type, otherwise it's the data we need to parse
        if (numBytes == 1)
        {
            // The master sends the request for the return type
            uint8_t requestType = Wire.read();
            currentRequest = static_cast<I2C_REQUESTS>(requestType);
            switch (currentRequest)
            {
            case I2C_REQUESTS::REQUEST_MOTORSTATE:
                break;
            case I2C_REQUESTS::REQUEST_HOMESTATE:
                break;
            case I2C_REQUESTS::REQUEST_TMCDATA:
                break;
            case I2C_REQUESTS::REQUEST_OTAUPDATE:
                // start ota server 
                log_i("Starting OTA");
                State::startOTA();
                break;
            case I2C_REQUESTS::REQUEST_REBOOT:
                // reboot the device
                log_i("Rebooting device");
                ESP.restart();
                break;
            default:
                log_e("Unknown Request Type");
                break;
            }
        }
        else
        {
            // Motor, Home, TMC Events
            parseMotorEvent(numBytes);
        }
    }

    void requestEvent()
    {
        // The master request data from the slave
        // !THIS IS ONLY EXECUTED IN I2C SLAVE MODE!
        // for the motor we would need to send the current position and the state of isRunning
        if (currentRequest == I2C_REQUESTS::REQUEST_MOTORSTATE)
        {
            // The master request data from the slave
            MotorState motorState;
            bool isRunning = !FocusMotor::getData()[pinConfig.REMOTE_MOTOR_AXIS_ID]->stopped;
            long currentPosition = FocusMotor::getData()[pinConfig.REMOTE_MOTOR_AXIS_ID]->currentPosition;
            bool isForever = FocusMotor::getData()[pinConfig.REMOTE_MOTOR_AXIS_ID]->isforever;
            motorState.currentPosition = currentPosition;
            motorState.isRunning = isRunning;
            // motorState.isForever = isForever;
            //log_i("Motor is running: %i, at position: %i", isRunning, currentPosition);
            Wire.write((uint8_t *)&motorState, sizeof(MotorState));
        }
        else if (currentRequest == I2C_REQUESTS::REQUEST_HOMESTATE)
        {
            // The master request data from the slave
            HomeState homeState;
            bool isHoming = HomeMotor::getHomeData()[pinConfig.REMOTE_MOTOR_AXIS_ID]->homeIsActive;
            int homeInEndposReleaseMode = HomeMotor::getHomeData()[pinConfig.REMOTE_MOTOR_AXIS_ID]->homeInEndposReleaseMode;
            homeState.isHoming = isHoming;
            homeState.homeInEndposReleaseMode = homeInEndposReleaseMode;
            log_i("Home is running: %i, in endpos release mode: %i", isHoming, homeInEndposReleaseMode);
            Wire.write((uint8_t *)&homeState, sizeof(HomeState));
        }
        else
        {
            // Handle error: I2C controller type not supported
            log_e("Error: I2C request type not supported.");
            log_e("Current Request Code: %i", currentRequest);
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