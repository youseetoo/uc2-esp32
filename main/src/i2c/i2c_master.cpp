#include "i2c_master.h"
#include <PinConfig.h>
#include "Wire.h"
#include "esp_log.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/semphr.h"

#include "esp_log.h"
#include "esp_debug_helpers.h"

#include "JsonKeys.h"
#include <Preferences.h>
#ifdef DIAL_CONTROLLER
#include "../dial/DialController.h"
#endif
#ifdef LASER_CONTROLLER
#include "../laser/LaserController.h"
#endif

using namespace FocusMotor;
namespace i2c_master
{

    // for A,X,Y,Z intialize the I2C addresses
    uint8_t i2c_addresses[] = {
        pinConfig.I2C_ADD_MOT_A,
        pinConfig.I2C_ADD_MOT_X,
        pinConfig.I2C_ADD_MOT_Y,
        pinConfig.I2C_ADD_MOT_Z};

    uint8_t i2c_laseraddresses[] = {
        pinConfig.I2C_ADD_LEX_PWM0,
        pinConfig.I2C_ADD_LEX_PWM1,
        pinConfig.I2C_ADD_LEX_PWM2,
        pinConfig.I2C_ADD_LEX_PWM3};

    // keep track of the motor states
    const int MAX_I2C_DEVICES = 20;     // Maximum number of expected devices
    byte i2cAddresses[MAX_I2C_DEVICES]; // Array to store found I2C addresses
    int numDevices = 0;                 // Variable to keep track of number of devices found
    int i2cRescanTick = 0;              // Variable to keep track of number of devices found
    int i2cRescanAfterNTicks = -1;      // Variable to keep track of number of devices found
    int pullMotorDataReducedDriverTick[4] = {0, 0, 0, 0};
#ifdef DIAL_CONTROLLER
    DialData mDialData;
    DialData mDialDataOld;
    bool positionsPushedToDial = false;
    int ticksLastPosPulled = 0;
    int ticksPosPullInterval = 5; // Pull position from slave every n-times
#endif

    void i2c_scan()
    {
        // Ensure we start with no stored addresses
        numDevices = 0;

        byte error, address;

        log_i("Scanning...");

        for (address = 1; address < 127; address++)
        {
            // Begin I2C transmission to check if a device acknowledges at this address
            Wire.beginTransmission(address);
            error = Wire.endTransmission();

            if (error == 0)
            {
                log_i("I2C device found at address 0x%02X  !", address);
                // Store the address if the device is found and space is available in the array
                if (numDevices < MAX_I2C_DEVICES)
                {
                    i2cAddresses[numDevices] = address;
                    numDevices++;
                }
                else
                {
                    log_i("Maximum number of I2C devices reached. Increase MAX_I2C_DEVICES if needed.");
                    break; // Exit the loop if we've reached the maximum storage capacity
                }
            }
            else if (error == 4)
            {
                log_e("Unknown device found at address 0x%02X  !", address);
            }
        }

        if (numDevices == 0)
            log_i("No I2C devices found\n");
        else
        {
            log_i("Found %d I2C devices.", numDevices);
            log_i("Addresses of detected devices:");
            for (int i = 0; i < numDevices; i++)
            {
                if (i2cAddresses[i] < 16)
                {
                    log_i("0x0%X", i2cAddresses[i]);
                }
                else
                {
                    log_i("0x%X", i2cAddresses[i]);
                }
            }
        }
    }

    void setup()
    {
        const char *prefNamespace = "UC2";
        // TODO: Is this still true? >= if TCA is active wire doesn't work
        Wire.begin(pinConfig.I2C_SDA, pinConfig.I2C_SCL, 100000); // 400 Khz is necessary for the M5Dial
        i2c_scan();
    }

    /**************************************
    MOTOR
    *************************************/

    MotorState getMotorState(int i)
    {
        MotorState mMotorState = pullMotorDataReducedDriver(i);
        return mMotorState;
    }

    int axis2address(int axis)
    {
        if (axis >= 0 && axis < 4)
        {
            return i2c_addresses[axis];
        }
        return 0;
    }

    void sendMotorDataToI2CDriver(MotorData motorData, uint8_t axis, bool reduced = false)
    {
        // send motor data to slave via I2C
        uint8_t slave_addr = axis2address(axis);
        Wire.beginTransmission(slave_addr);
        int err = 0;
        if (reduced)
        {
            // if we only want to send position, etc. we can reduce the size of the data
            MotorDataReduced reducedData;
            reducedData.targetPosition = motorData.targetPosition;
            reducedData.isforever = motorData.isforever;
            reducedData.absolutePosition = motorData.absolutePosition;
            reducedData.speed = motorData.speed;
            reducedData.isStop = motorData.isStop;

            uint8_t *dataPtr = (uint8_t *)&reducedData;
            int dataSize = sizeof(MotorDataReduced);
            // Send the byte array over I2C
            Wire.write(dataPtr, dataSize);
            err = Wire.endTransmission();
        }
        else
        {
            // Cast the structure to a byte array
            uint8_t *dataPtr = (uint8_t *)&motorData;
            int dataSize = sizeof(MotorData);
            // Send the byte array over I2C
            Wire.write(dataPtr, dataSize);
            err = Wire.endTransmission();
        }
        if (err != 0)
        {
            log_e("Error sending motor data to I2C slave at address %i", slave_addr);
        }
        else
        {
            log_i("MotorData to axis: %i, at address %i, isStop: %i, speed: %i, targetPosition:%i, reduced %i, stopped %i, isaccel: %i, accel: %i, isEnable: %i, isForever %i", axis, slave_addr, motorData.isStop, motorData.speed, motorData.targetPosition, reduced, motorData.stopped, motorData.isaccelerated, motorData.acceleration, motorData.isEnable, motorData.isforever);
        }
    }

    void startStepper(MotorData *data, int axis, bool reduced)
    {
        if (data != nullptr)
        {
            positionsPushedToDial = false;
            data->isStop = false; // ensure isStop is false
            data->stopped = false;
            sendMotorDataToI2CDriver(*data, axis, reduced);
        }
        else
        {
            ESP_LOGE("I2C_MASTER", "Invalid motor data for axis %d", axis);
        }
    }

    void stopStepper(MotorData *data, int axis)
    {
        //esp_backtrace_print(10);
        //  only send motor data if it was running before
        log_i("Stop Motor %i in I2C Master", axis);
        sendMotorDataToI2CDriver(*data, axis, false);
    }

    void setPosition(Stepper s, int pos)
    {
        // TODO: Change!
        setPositionI2CDriver(s, 0);
    }

    void setPositionI2CDriver(Stepper s, uint32_t pos)
    {
        // send the position to the slave via I2C
        uint8_t slave_addr = axis2address(s);
        log_i("Setting position on I2C driver to %i on axis %i", pos, s);
        Wire.beginTransmission(slave_addr);
        Wire.write((uint8_t *)&pos, sizeof(uint32_t));
        int err = Wire.endTransmission();
        if (err != 0)
        {
            log_e("Error sending position to I2C slave at address %i", slave_addr);
        }
        else
        {
            log_i("Position set to %i on axis %i", pos, s);
        }
    }

    /***************************************
    HOME
    ***************************************/

    void sendHomeDataI2C(HomeData homeData, uint8_t axis)
    {
        // send home data to slave via I2C and initiate homing
        uint8_t slave_addr = axis2address(axis);
        log_i("HomeData to axis: %i ", axis);

        Wire.beginTransmission(slave_addr);

        // cast the structure to a byte array
        uint8_t *dataPtr = (uint8_t *)&homeData;
        int dataSize = sizeof(HomeData);

        // send the byte array over I2C
        Wire.write(dataPtr, dataSize);
        int err = Wire.endTransmission();
        if (err != 0)
        {
            log_e("Error sending home data to I2C slave at address %i", slave_addr);
        }
        else
        {
            log_i("Home data sent to I2C slave at address %i", slave_addr);
            // need to put the motor into driving state so that a stop can be identified
            // getData()[axis]->stopped = false; // TODO: FIXME
            getData()[axis]->qid = homeData.qid;
            // TODO: Renew i2c_master::setMotorStopped(axis, false);
        }
    }

    HomeState pullHomeStateFromI2CDriver(int axis)
    {
        
        // we pull the data from the slave's register
        uint8_t slave_addr = axis2address(axis);

        // Request data from the slave but only if inside i2cAddresses
        if (!isAddressInI2CDevices(slave_addr))
        {
            // log_e("Error: I2C slave address %i not found in i2cAddresses", slave_addr);
            return HomeState();
        }

        // read the data from the slave
        const int maxRetries = 2;
        int retryCount = 0;
        bool success = false;
        HomeState homeState; // Initialize with default values
        while (retryCount < maxRetries && !success)
        {
            // First send the request-code to the slave
            Wire.beginTransmission(slave_addr);
            Wire.write(REQUEST_HOMESTATE);
            Wire.endTransmission();

            // temporarily disable watchdog in case of slow I2C communication
            Wire.requestFrom(slave_addr, sizeof(HomeState));
            if (Wire.available() == sizeof(HomeState))
            {
                Wire.readBytes((uint8_t *)&homeState, sizeof(homeState));
                success = true;
            }
            else
            {
                retryCount++;
                log_w("Warning: Incorrect data size received from address %i. Data size is %i. Retry %i/%i", slave_addr, Wire.available(), retryCount, maxRetries);
                delay(20); // Wait a bit before retrying
            }
        }
        return homeState;
    }

    /***************************************
     * I2C
     ***************************************/

    bool isAddressInI2CDevices(byte addressToCheck)
    {
        for (int i = 0; i < numDevices; i++) // Iterate through the array
        {
            if (i2cAddresses[i] == addressToCheck) // Check if the current element matches the address
            {
                return true; // Address found in the array
            }
        }
        return false; // Address not found
    }

    /***************************************
     * Dial
     ***************************************/

#ifdef DIAL_CONTROLLER
    void pushMotorPosToDial()
    {
        // This is the Master pushing the data to the DIAL I2C slave (i.e. 4 motor positions) to sync the display with the motors
        uint8_t slave_addr = pinConfig.I2C_ADD_M5_DIAL;
        if (!isAddressInI2CDevices(slave_addr))
        {
            //log_e("Error (push): Dial address not found in i2cAddresses");
            return;
        }
        Wire.beginTransmission(slave_addr);
        mDialData.pos_a = FocusMotor::getData()[0]->currentPosition;
        mDialData.pos_x = FocusMotor::getData()[1]->currentPosition;
        mDialData.pos_y = FocusMotor::getData()[2]->currentPosition;
        mDialData.pos_z = FocusMotor::getData()[3]->currentPosition;

        // sync old values
        mDialDataOld.pos_a = mDialData.pos_a;
        mDialDataOld.pos_x = mDialData.pos_x;
        mDialDataOld.pos_y = mDialData.pos_y;
        mDialDataOld.pos_z = mDialData.pos_z;

#ifdef LASER_CONTROLLER
        mDialData.intensity = LaserController::getLaserVal(1);
        mDialDataOld.intensity = mDialData.intensity;
#else
        mDialData.intensity = 0;
#endif
        log_i("Motor positions sent to dial: %i, %i, %i, %i, %i", mDialData.pos_a, mDialData.pos_x, mDialData.pos_y, mDialData.pos_z, mDialData.intensity);
        Wire.write((uint8_t *)&mDialData, sizeof(DialData));
        Wire.endTransmission();
        positionsPushedToDial = true; // otherwise it will probably always go to 0,0,0,0  on start
    }
#endif

    /***************************************
     * Laser
     ***************************************/

    int laserid2address(int id)
    {
        // we need to check if the id is in the range of the laser addresses
        if (id >= 0 && id < numDevices)
        {
            return i2c_laseraddresses[id];
        }
        return 0;
    }

    void sendLaserDataI2C(LaserData laserdata, uint8_t id)
    {
        // we send the laser data to the slave via I2C
        uint8_t slave_addr = laserid2address(1); // TODO: Hardcoded for now since we only have one laserboard - typically we have multiple lasers on one board
        log_i("LaserData to id: %i", id);

        Wire.beginTransmission(slave_addr);

        // cast the structure to a byte array
        uint8_t *dataPtr = (uint8_t *)&laserdata;
        int dataSize = sizeof(LaserData);

        // send the byte array over I2C
        Wire.write(dataPtr, dataSize);
        int err = Wire.endTransmission();
        if (err != 0)
        {
            log_e("Error sending laser data to I2C slave at address %i", slave_addr);
        }
        else
        {
            log_i("Laser data sent to I2C slave at address %i", slave_addr);
        }
    }

    /***************************************
     * TMC
     ***************************************/

    #ifdef TMC_CONTROLLER
    void sendTMCDataI2C(TMCData tmcData, uint8_t axis)
    {
        // we send the TMC data to the slave via I2C
        uint8_t slave_addr = axis2address(axis);
        log_i("TMCData to axis: %i ", axis);

        Wire.beginTransmission(slave_addr);

        // cast the structure to a byte array
        uint8_t *dataPtr = (uint8_t *)&tmcData;
        int dataSize = sizeof(TMCData);

        // send the byte array over I2C
        Wire.write(dataPtr, dataSize);
        int err = Wire.endTransmission();
        if (err != 0)
        {
            log_e("Error sending TMC data to I2C slave at address %i", slave_addr);
        }
        else
        {
            log_i("TMC data sent to I2C slave at address %i", slave_addr);
        }
    }
    #endif

    int act(cJSON *doc)
    {
        // do nothing
        return 0;
    }

    cJSON *get(cJSON *ob)
    {
        // return a list of all I2C devices {"task":"/i2c_get"}
        cJSON *i2cDevices = cJSON_CreateArray();
        for (int i = 0; i < numDevices; i++)
        {
            cJSON *i2cDevice = cJSON_CreateObject();
            cJSON_AddNumberToObject(i2cDevice, "address", i2cAddresses[i]);
            cJSON_AddItemToArray(i2cDevices, i2cDevice);
        }
        cJSON_AddItemToObject(ob, "i2cDevices", i2cDevices);
        return ob;
    }

    MotorState pullMotorDataReducedDriver(int axis)
    {
        // log_i("Pulling motor data from I2C driver for axis %i", axis);
        //  we pull the data from the slave's register
        uint8_t slave_addr = axis2address(axis);

        // Request data from the slave but only if inside i2cAddresses
        if (!isAddressInI2CDevices(slave_addr))
        {
            // log_e("Error: I2C slave address %i not found in i2cAddresses", slave_addr);
            return MotorState();
        }

        // First send the request-code to the slave
        Wire.beginTransmission(slave_addr);
        Wire.write(REQUEST_MOTORSTATE);
        Wire.endTransmission();

        // read the data from the slave
        const int maxRetries = 2;
        int retryCount = 0;
        bool success = false;
        MotorState motorState; // Initialize with default values
        while (retryCount < maxRetries && !success)
        {
            // First we need to tell the slave which data to send
            Wire.beginTransmission(slave_addr);
            Wire.write(I2C_REQUESTS::REQUEST_MOTORSTATE); // Send a code to the slave to request the data
            Wire.endTransmission();
            delay(10); // Wait a bit before requesting the data

            // temporarily disable watchdog in case of slow I2C communication
            Wire.requestFrom(slave_addr, sizeof(MotorState));
            if (Wire.available() == sizeof(MotorState))
            {
                Wire.readBytes((uint8_t *)&motorState, sizeof(motorState));
                success = true;
            }
            else
            {
                retryCount++;
                log_w("Warning: Incorrect data size received from address %i. Data size is %i. Retry %i/%i", slave_addr, Wire.available(), retryCount, maxRetries);
                delay(20); // Wait a bit before retrying
            }
        }

        if (!success)
        {
            motorState.currentPosition = FocusMotor::getData()[axis]->currentPosition; // Fallback-Value
            log_e("Error: Failed to read correct data size from address %i after %i attempts", slave_addr, maxRetries);
        }
        // log_i("Motor State: position: %i, isRunning: %i, axis %i", motorState.currentPosition, motorState.isRunning, axis);
        return motorState;
    }

#ifdef DIAL_CONTROLLER
    void pullParamsFromDial()
    {
        // This is the MASTER pulling the data from the DIAL I2C slave (i.e. 4 motor positions)
        uint8_t slave_addr = pinConfig.I2C_ADD_M5_DIAL;

        // Request data from the slave but only if inside i2cAddresses
        if (!isAddressInI2CDevices(slave_addr))
        {
            //log_e("Error (pull): Dial address not found in i2cAddresses");
            return;
        }

        // REQUEST DATA FROM DIAL
        DialData mDialData;
        Wire.requestFrom(slave_addr, sizeof(DialData));
        int dataSize = Wire.available();
        if (dataSize == sizeof(DialData))
        {
            Wire.readBytes((char *)&mDialData, sizeof(DialData));
            // check of all elelements are zero, if so we don't want to assign them to the motors as we likly catch the startup of the dial
            if (mDialData.pos_a == 0 and mDialData.pos_x == 0 and mDialData.pos_y == 0 and mDialData.pos_z == 0 and mDialData.intensity == 0)
            {
                // correct position on Dial
                pushMotorPosToDial();
                return;
            }

            // only one axis can be moved at a time
            if (((mDialDataOld.pos_a - mDialData.pos_a) != 0) + ((mDialDataOld.pos_x - mDialData.pos_x) != 0) + ((mDialDataOld.pos_y - mDialData.pos_y) != 0) + ((mDialDataOld.pos_z - mDialData.pos_z) != 0) > 1)
            {
                // correct position on Dial
                log_i("old: %i %i %i %i; new: %i %i %i %i", mDialDataOld.pos_a, mDialDataOld.pos_x, mDialDataOld.pos_y, mDialDataOld.pos_z, mDialData.pos_a, mDialData.pos_x, mDialData.pos_y, mDialData.pos_z);
                // update old pos on dial is done in pushMotorPosToDial
                pushMotorPosToDial();
                return;
            }

            // compare the current position of the motor with the dial state and drive the motor to the dial state
            for (int iMotor = 0; iMotor < 4; iMotor++)
            {
                // compare old dial data; if same value, don't move the motor
                // This is zero-cost so we do it first!
                uint32_t position2go = 0;
                if (iMotor == 0)
                    position2go = mDialData.pos_a;
                if (iMotor == 1)
                    position2go = mDialData.pos_x;
                if (iMotor == 2)
                    position2go = mDialData.pos_y;
                if (iMotor == 3)
                    position2go = mDialData.pos_z;

                if (iMotor == 0 && mDialDataOld.pos_a == position2go)
                {
                    continue;
                }
                if (iMotor == 1 && mDialDataOld.pos_x == position2go)
                {
                    continue;
                }
                if (iMotor == 2 && mDialDataOld.pos_y == position2go)
                {
                    continue;
                }
                if (iMotor == 3 && mDialDataOld.pos_z == position2go)
                {
                    continue;
                }

                // eventually we need to pull the position from the motor depending on the motor type
                FocusMotor::updateData(iMotor);
                uint32_t mCurrentPosition = FocusMotor::getData()[iMotor]->currentPosition;

                // check if current position and position2go are within a reasonable range
                // if not we don't want to move the motor
                // log_i("Motor %i: Current position: %i, Dial position: %i", iMotor, FocusMotor::getData()[iMotor]->currentPosition, position2go);
                if (abs(mCurrentPosition - position2go) > 20000)
                {
                    log_e("Error: Motor %i is too far away from dial position %i", iMotor, position2go);
                    continue;
                }
                // if there is no position change we don't want to move the motor
                if (FocusMotor::getData()[iMotor]->currentPosition == position2go)
                {
                    log_e("Error: Motor %i is already at dial position %i", iMotor, position2go);
                    continue;
                }

                // Here we drive the motor to the dial state
                Stepper mStepper = static_cast<Stepper>(iMotor);
                // we dont have that on master side
                // setAutoEnable(false);
                // setEnable(true);
                // we can send the data to the slave directly
                // #TODO: Alternavly, we could compute the delta w.r.t. last pull and use this as a relative position - this would be more robust
                // Prepare Motor struct
                FocusMotor::getData()[mStepper]->absolutePosition = 1;
                FocusMotor::getData()[mStepper]->targetPosition = position2go;
                FocusMotor::getData()[mStepper]->isforever = 0;
                FocusMotor::getData()[mStepper]->isaccelerated = 1;
                FocusMotor::getData()[mStepper]->acceleration = 5000;
                FocusMotor::getData()[mStepper]->speed = 10000;
                FocusMotor::getData()[mStepper]->isEnable = 1;
                FocusMotor::getData()[mStepper]->qid = 0;
                FocusMotor::getData()[mStepper]->isStop = 0;
                FocusMotor::getData()[mStepper]->stopped = false;
                // we want to start a motor no matter if it's connected via I2C or natively
                log_i("Motor %i: Drive to position %i, at speed %i, stopped %i", mStepper, FocusMotor::getData()[mStepper]->targetPosition, FocusMotor::getData()[mStepper]->speed, FocusMotor::getData()[mStepper]->stopped);
                // start the motor depending on the motor type
                FocusMotor::startStepper(mStepper, false);
                // update the old dial data
                if (iMotor == 0)
                    mDialDataOld.pos_a = position2go;
                if (iMotor == 1)
                    mDialDataOld.pos_x = position2go;
                if (iMotor == 2)
                    mDialDataOld.pos_y = position2go;
                if (iMotor == 3)
                    mDialDataOld.pos_z = position2go;
            }
#ifdef LASER_CONTROLLER
            // for intensity only
            int intensity = mDialData.intensity;
            if (lastIntensity != intensity)
            {
                lastIntensity = intensity;
                LaserController::setLaserVal(LaserController::PWM_CHANNEL_LASER_1, intensity);
            }
#endif
        }
        else
        {
            log_e("Error: Incorrect data size received in dial from address %i. Data size is %i", slave_addr, dataSize);
        }
    }
#endif

    /******************************+
     * MISC
     ******************************/

    void startOTA(int axis)
    {
        // we need to send a signal to the slave to start the OTA process
        if (axis < 0)
        {
            // send to all available I2C addresses
            for (int i = 0; i < numDevices; i++)
            {
                Wire.beginTransmission(i2cAddresses[i]);
                Wire.write(I2C_REQUESTS::REQUEST_OTAUPDATE);
                Wire.endTransmission();
            }
            }
            else
            {
                uint8_t slave_addr = axis2address(axis);
                log_i("Start OTA on axis %i", axis);
                Wire.beginTransmission(slave_addr);
                Wire.write(I2C_REQUESTS::REQUEST_OTAUPDATE);
                Wire.endTransmission();
            }
        }

        void reboot(){
            // we need to send a signal to the slave to reboot the device
            for (int i = 0; i < numDevices; i++)
            {
                log_i("Send reboot to I2C address %i", i2cAddresses[i]);
                Wire.beginTransmission(i2cAddresses[i]);
                Wire.write(I2C_REQUESTS::REQUEST_REBOOT);
                Wire.endTransmission();
            }
        }

        void loop()
        {
            // add anything that would eventually require a pull from the I2C bus
#ifdef DIAL_CONTROLLER
            if (ticksLastPosPulled >= ticksPosPullInterval)
            {
                ticksLastPosPulled = 0;
                // Here we want to pull the dial data from the I2C bus and assign it to the motors
                pullParamsFromDial();
            }
            else
            {
                ticksLastPosPulled++;
            }
#endif
        }
    }