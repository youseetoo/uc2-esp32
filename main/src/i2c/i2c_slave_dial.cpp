#include "i2c_slave_dial.h"
#include "Wire.h"
#include "PinConfig.h"
#include "../dial/DialController.h"

namespace i2c_slave_dial
{
    void parseDialEvent(int numBytes)
    {
        // We will receive the array of 4 positions and one intensity from the master and have to update the dial display

        if (numBytes == sizeof(DialController::mDialData))
        {
            // log_i("Received DialData from I2C");

            // Read the data into the mDialData struct
            Wire.readBytes((uint8_t *)&DialController::mDialData, sizeof(DialController::mDialData));

            /*
            // Now you can access the motor positions/intensity and update the dial display
            int pos_a = DialController::mDialData.pos_a;
            int pos_x = DialController::mDialData.pos_x;
            int pos_y = DialController::mDialData.pos_y;
            int pos_z = DialController::mDialData.pos_z;
            int intensity = DialController::mDialData.intensity;
            */
            // Perform your specific actions here, such as updating the dial display.
            DialController::setDialValues(DialController::mDialData);
        }
        else
        {
            // Handle error: received data size does not match expected size
            log_e("Error: Received data size does not match DialData size.");
        }
    }

    void receiveEvent(int numBytes)
    {
        // Master and Slave
        // log_i("Receive Event");
        if (pinConfig.I2C_CONTROLLER_TYPE == I2CControllerType::mMOTOR)
        {
        }
        else if (pinConfig.I2C_CONTROLLER_TYPE == I2CControllerType::mLASER)
        {
            // Handle incoming data for the laser controller
            // parseLaserEvent(numBytes);
        }
        else if (pinConfig.I2C_CONTROLLER_TYPE == I2CControllerType::mLED)
        {
            // Handle incoming data for the LED controller
            // parseLEDEvent(numBytes);
        }
        else if (pinConfig.I2C_CONTROLLER_TYPE == I2CControllerType::mDIAL)
        {
            // Handle incoming data for the motor controller
            parseDialEvent(numBytes);
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
        if (pinConfig.I2C_CONTROLLER_TYPE == I2CControllerType::mDIAL)
        {
            // The master request data from the slave
            DialData dialDataMotor;

            // @KillerInk is there a smarter way to do this?
            // make a copy of the current dial data
            dialDataMotor = DialController::getDialValues();
            Wire.write((uint8_t *)&dialDataMotor, sizeof(DialData));
            // WARNING!! The log_i causes confusion in the I2C communication, but the values are correct
            // log_i("DialData sent to I2C master: %i, %i, %i, %i", dialDataMotor.pos_abs[0], dialDataMotor.pos_abs[1], dialDataMotor.pos_abs[2], dialDataMotor.pos_abs[3]);
        }
        else
        {
            // Handle error: I2C controller type not supported
            log_e("Error: I2C controller type not supported.");
            Wire.write(0);
        }
    }

}