#include "i2c_slave_laser.h"
#include "Wire.h"
#include "PinConfig.h"
#include "../laser/LaserController.h"
#include "Preferences.h"
using namespace LaserController;

namespace i2c_slave_laser
{

    void parseLaserEvent(int numBytes)
    {
        // incoming command from I2C master will be converted to a laser action
        if (numBytes == sizeof(LaserData))
        {
            log_i("parseLaserEvent");
            LaserData receivedLaserData;
            uint8_t *dataPtr = (uint8_t *)&receivedLaserData;
            for (int i = 0; i < numBytes; i++)
            {
                dataPtr[i] = Wire.read();
            }
            // assign the received data to the laser to laserData *data[4];
            int LASERid = receivedLaserData.LASERid;
            int LASERval = receivedLaserData.LASERval;
            int LASERdespeckle = receivedLaserData.LASERdespeckle;
            int LASERdespecklePeriod = receivedLaserData.LASERdespecklePeriod;
            
            // assign the received data to the laser controller
            LaserController::setLaserVal(LASERid, LASERval);
        }
        else
        {
            // Handle error: received data size does not match expected size
            log_e("Error: Received data size does not match LaserDAta size.");
        }
    }

    void receiveEvent(int numBytes)
    {
        // Master and Slave
        if (pinConfig.I2C_CONTROLLER_TYPE == I2CControllerType::mLASER)
        {
            // Handle incoming data for the laser controller
            log_i("Received %i bytes", numBytes);
            parseLaserEvent(numBytes);
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
        // for the laser we return the full data
        if (pinConfig.I2C_CONTROLLER_TYPE == I2CControllerType::mLASER)
        {
            // The master request data from the slave
            LaserData laserData;
            laserData = LaserController::getLaserData();
            Wire.write((uint8_t *)&laserData, sizeof(LaserData));
            
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
#ifdef I2C_SLAVE_LASER
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
#ifdef I2C_SLAVE_LASER
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