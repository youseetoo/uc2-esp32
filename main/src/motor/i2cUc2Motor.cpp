#include "i2cUc2Motor.h"
#include <PinConfig.h>
using namespace i2cUc2Motor;
namespace i2cUc2Motor
{
    void starti2cUc2Stepper(int i)
    {
    }

    void setupi2cUc2Stepper()
    {
        // setup individual I2C devices with their own addresses
        // setupi2cUc2Stepper(Stepper stepper, int i2c_address);

        // pull the remote motor's positions
    }

    void setupi2cUc2Stepper(Stepper stepper, int i2c_address)
    {
    }

    void stopi2cUc2Stepper(int i)
    {
        // send stop signal to the motor on I2C
    }

    void updateData(int i)
    {
        // update the motor's position
    }

    void setAutoEnable(bool enable)
    {
        // send enable signal to the motor on I2C
    }
    void Enable(bool enable)
    {
        // send enable signal to the motor on I2C
    }

    void setPosition(Stepper s, int val)
    {
        // set the motor's position on I2C device
    }

    bool isRunning(int i)
    {
        // check if the motor is running
        return 1; // true
    }

    void move(Stepper s, int steps, bool blocking)
    {
        // move the motor by the given steps - not necessary since the motor is controlled by I2C
    }
}