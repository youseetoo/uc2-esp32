#include "i2cUc2Motor.h"
#include <PinConfig.h>
using namespace i2cUc2Motor;
namespace i2cUc2Motor
{
    void starti2cUc2Stepper()
    {
        // setup individual I2C devices with their own addresses
        // setupi2cUc2Stepper(Stepper stepper, int i2c_address);
        
        // pull the remote motor's positions
    }

    void stopi2cUc2Stepper(int i)
    {
        // send stop signal to the motor on I2C
    }

    void updateData(int i)
    {
        // update the motor's position
    }

    void setAutoEnable(bool enable, int axis)
    {
        // send enable signal to the motor on I2C
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "task", "/motor_act");
        cJSON_AddNumberToObject(root, "isenauto", enable ? 1 : 0);
        char *jsonString = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);  
        // send the json string to the I2C device on the given axis
        sendJsonString(jsonString, axis2address(axis));              
    }
    void Enable(bool enable, int axis)
    {
        // send enable signal to the motor on I2C
        cJSON *root = cJSON_CreateObject();
        cJSON_AddStringToObject(root, "task", "/motor_act");
        cJSON_AddNumberToObject(root, "isen", enable ? 1 : 0);
        char *jsonString = cJSON_PrintUnformatted(root);
        cJSON_Delete(root);  
        // send the json string to the I2C device on the given axis
        sendJsonString(jsonString, axis2address(axis));      
    }

    void sendJsonString(String jsonString, int axis)
    {
        // send the json string to the I2C device
        uint8_t slave_addr = axis2address(axis);
        I2C::sendJsonString(jsonString, slave_addr);
    }

    int axis2address(int axis)
    {
        if (axis >= 0 && axis < 4)
        {
            return addresses[axis];
        }
        return 0;
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