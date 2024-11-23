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
    MotorData a_dat;
    MotorData x_dat;
    MotorData y_dat;
    MotorData z_dat;
    MotorData *data[4];
    const int MAX_I2C_DEVICES = 20;     // Maximum number of expected devices
    byte i2cAddresses[MAX_I2C_DEVICES]; // Array to store found I2C addresses
    int numDevices = 0;                 // Variable to keep track of number of devices found
    int i2cRescanTick = 0;              // Variable to keep track of number of devices found
    int i2cRescanAfterNTicks = -1;      // Variable to keep track of number of devices found
    int pullMotorDataI2CDriverTick[4] = {0, 0, 0, 0};
    Preferences preferences;
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
        // TODO: @KillerInkare we still using data?
        data[Stepper::A] = &a_dat;
        data[Stepper::X] = &x_dat;
        data[Stepper::Y] = &y_dat;
        data[Stepper::Z] = &z_dat;

        const char *prefNamespace = "UC2";
        // TODO: Is this still true? >= if TCA is active wire doesn't work
        Wire.begin(pinConfig.I2C_SDA, pinConfig.I2C_SCL, 100000); // 400 Khz is necessary for the M5Dial
        i2c_scan();
        if (pinConfig.MOTOR_A_STEP >= 0)
        {
            updateMotorData(Stepper::A);
            log_i("Motor A position: %i", data[Stepper::A]->currentPosition);
        }
        if (pinConfig.MOTOR_X_STEP >= 0)
        {
            updateMotorData(Stepper::X);
            log_i("Motor X position: %i", data[Stepper::X]->currentPosition);
        }
        if (pinConfig.MOTOR_Y_STEP >= 0)
        {
            updateMotorData(Stepper::Y);
            log_i("Motor Y position: %i", data[Stepper::Y]->currentPosition);
        }
        if (pinConfig.MOTOR_Z_STEP >= 0)
        {
            updateMotorData(Stepper::Z);
            log_i("Motor Z position: %i", data[Stepper::Z]->currentPosition);
        }
    }

    void updateMotorData(int i)
    {
        MotorState mMotorState = pullMotorDataI2CDriver(i);
        data[i]->currentPosition = mMotorState.currentPosition;
    }

    long getMotorPosition(int i)
    {
        return data[i]->currentPosition;
    }

    int pullHomeStateFromI2CDriver(int axis)
    {
        // pull the home state from the I2C driver
        uint8_t slave_addr = axis2address(axis);
        Wire.requestFrom(slave_addr, sizeof(HomeData));
        HomeData homeData;
        Wire.readBytes((uint8_t *)&homeData, sizeof(HomeData));
        return homeData.qid;
    }

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

#ifdef DIAL_CONTROLLER
    void pushMotorPosToDial()
    {
        // This is the Master pushing the data to the DIAL I2C slave (i.e. 4 motor positions) to sync the display with the motors
        uint8_t slave_addr = pinConfig.I2C_ADD_M5_DIAL;
        if (!isAddressInI2CDevices(slave_addr))
        {
            log_e("Error (push): Dial address not found in i2cAddresses");
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

    void sendMotorPos(int i, int arraypos)
    {
        log_i("sendMotorPos %i", i);
        // enforce position update on axis
        updateMotorData(i);
        // update on Motor, too, so that the dial is happy
        getData()[i]->currentPosition = data[i]->currentPosition;

        cJSON *root = cJSON_CreateObject();
        if (root == NULL)
            return; // Handle allocation failure

        cJSON *stprs = cJSON_CreateArray();
        if (stprs == NULL)
        {
            cJSON_Delete(root);
            return; // Handle allocation failure
        }
        cJSON_AddItemToObject(root, key_steppers, stprs);
        cJSON_AddNumberToObject(root, "qid", data[i]->qid);

        cJSON *item = cJSON_CreateObject();
        if (item == NULL)
        {
            cJSON_Delete(root);
            return; // Handle allocation failure
        }
        cJSON_AddItemToArray(stprs, item);
        cJSON_AddNumberToObject(item, key_stepperid, i);
        cJSON_AddNumberToObject(item, key_position, data[i]->currentPosition);
        cJSON_AddNumberToObject(item, "isDone", data[i]->stopped);
        arraypos++;

        // Print result - will that work in the case of an xTask?
        Serial.println("++");
        char *s = cJSON_Print(root);
        if (s != NULL)
        {
            Serial.println(s);
            free(s);
        }
        Serial.println("--");
        cJSON_Delete(root); // Free the root object, which also frees all nested objects

#ifdef DIAL_CONTROLLER
        // update the dial with the actual motor positions
        // the motor positions array is updated in the dial controller
        pushMotorPosToDial();
#endif
    }

    int axis2address(int axis)
    {
        if (axis >= 0 && axis < 4)
        {
            return i2c_addresses[axis];
        }
        return 0;
    }

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
            getData()[axis]->stopped = false;
            getData()[axis]->qid = homeData.qid;
            i2c_master::setMotorStopped(axis, false);
        }
    }

    void sendMotorDataToI2CDriver(MotorData motorData, uint8_t axis, bool reduced = false)
    {
        // send motor data to slave via I2C
        uint8_t slave_addr = axis2address(axis);
        Wire.beginTransmission(slave_addr);
        int err = 0;
        log_i("MotorData to axis: %i, at address %i, isStop: %i, speed: %i, targetPosition:%i, reduced %i, stopped %i", axis, slave_addr, motorData.isStop, motorData.speed, motorData.targetPosition, reduced, motorData.stopped);
        if (reduced)
        {
            // if we only want to send position, etc. we can reduce the size of the data
            MotorDataI2C reducedData;
            reducedData.targetPosition = motorData.targetPosition;
            reducedData.isforever = motorData.isforever;
            reducedData.absolutePosition = motorData.absolutePosition;
            reducedData.speed = motorData.speed;
            reducedData.isStop = motorData.isStop;

            uint8_t *dataPtr = (uint8_t *)&reducedData;
            int dataSize = sizeof(MotorDataI2C);
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
            log_i("MotorData to axis: %i, at address %i, isStop: %i, speed: %i, targetPosition:%i, reduced %i, stopped %i", axis, slave_addr, motorData.isStop, motorData.speed, motorData.targetPosition, reduced, motorData.stopped);
        }
    }

    void startStepper(int axis, bool reduced = false, bool external = false)
    {
        if (external)
        {
            // Use external motor data
            MotorData *data_ext = FocusMotor::getData()[axis];
            if (data_ext)
            {
                data[axis] = data_ext; // Sync external state to internal
            }
            else
            {
                ESP_LOGE("I2C_MASTER", "Invalid external motor data for axis %d", axis);
                return;
            }
        }

        if (data != nullptr && data[axis] != nullptr)
        {
            positionsPushedToDial = false;
            log_i("data[axis]->stopped %i, FocusMotor::getData()[axis]->stopped %i, axis %i",
                  data[axis]->stopped, FocusMotor::getData()[axis]->stopped, axis);
            waitForFirstRunI2CSlave[axis] = true;
            MotorData *m = data[axis];
            sendMotorDataToI2CDriver(*m, axis, reduced);
        }
        else
        {
            ESP_LOGE("I2C_MASTER", "Invalid motor data for axis %d", axis);
        }
    }

    void stopStepper(int i)
    {
        // only send motor data if it was running before
        log_i("Stop Motor in I2C Master %i", i);
        // if (!data[i]->stopped)
        data[i]->isStop = true;      // FIXME: We should send only those bits that are relevant (e.g. start/stop + payload bytes)
        data[i]->targetPosition = 0; // weird bug, probably not interpreted correclty on slave: If we stop the motor it's taking the last position and moves twice
        data[i]->absolutePosition = false;
        data[i]->stopped = true;                     // FIXME: difference between stopped and isStop?
        sendMotorDataToI2CDriver(*data[i], i, true); // TODO: This cannot send two motor information simultaenosly
        sendMotorPos(i, 0);
        waitForFirstRunI2CSlave[i] = false; // reset the flag
    }

    void toggleStepper(Stepper s, bool isStop, bool reduced = false)
    {
        if (isStop)
        {
            log_i("stop stepper from parseMotorDriveJson");
            stopStepper(s);
        }
        else
        {
            log_i("start stepper from parseMotorDriveJson");
            startStepper(s, reduced, false);
        }
    }

    void setPosition(Stepper s, int pos)
    {
        data[s]->currentPosition = pos;
        setPositionI2CDriver(s, 0);
    }

    void setMotorStopped(int axis, bool stopped)
    {
        // manipulate the motor data
        data[axis]->stopped = stopped;
    }

    void setPositionI2CDriver(Stepper s, long pos)
    {
        // send the position to the slave via I2C
        uint8_t slave_addr = axis2address(s);
        log_i("Setting position on I2C driver to %i on axis %i", pos, s);
        Wire.beginTransmission(slave_addr);
        Wire.write((uint8_t *)&pos, sizeof(long));
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

    void parseHomeJsonI2C(cJSON *doc)
    {
        /*
        We parse the incoming JSON string to the motor struct and send it via I2C to the correpsonding motor driver
        for homing
        */
        log_i("parseHomeJsonI2C");
    }

    void parseMotorJsonI2C(cJSON *doc)
    {
#ifdef I2C_MASTER
        /*
        We parse the incoming JSON string to the motor struct and send it via I2C to the correpsonding motor driver
        // TODO: We could reuse the parseMotorDriveJson function and just add the I2C send function?
        */
        log_i("parseMotorJsonI2C");
        cJSON *mot = cJSON_GetObjectItemCaseSensitive(doc, key_motor);
        if (mot != NULL)
        {
            // move steppers
            cJSON *stprs = cJSON_GetObjectItemCaseSensitive(mot, key_steppers);
            // set position
            cJSON *setpos = cJSON_GetObjectItem(doc, key_setposition);

            cJSON *stp = NULL;
            if (stprs != NULL)
            {
                cJSON_ArrayForEach(stp, stprs)
                {
                    Stepper s = static_cast<Stepper>(cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)));
                    data[s]->qid = cJsonTool::getJsonInt(doc, "qid");
                    data[s]->speed = cJsonTool::getJsonInt(stp, key_speed);
                    data[s]->isEnable = cJsonTool::getJsonInt(stp, key_isen);
                    data[s]->targetPosition = cJsonTool::getJsonInt(stp, key_position);
                    data[s]->isforever = cJsonTool::getJsonInt(stp, key_isforever);
                    data[s]->absolutePosition = cJsonTool::getJsonInt(stp, key_isabs);
                    data[s]->acceleration = cJsonTool::getJsonInt(stp, key_acceleration);
                    data[s]->isaccelerated = cJsonTool::getJsonInt(stp, key_isaccel);
                    // if cJSON_GetObjectItemCaseSensitive(stp, key_isstop); is not null or true stop the motor
                    cJSON *cstop = cJSON_GetObjectItemCaseSensitive(stp, key_isstop);
                    data[s]->isStop = (cstop != NULL) ? cstop->valueint : false;
                    log_i("Motor %i: Speed: %i, Position: %i, isStop: %i, stopped %i", s, data[s]->speed, data[s]->targetPosition, data[s]->isStop, data[s]->stopped);
                    toggleStepper(s, data[s]->isStop);
                    data[s]->stopped = data[s]->isStop; // TODO: This is a bit confusing, we should have a clear distinction between stopped and isStop - stopped is: not running (but need the previous state), isStop is for stopping a motor that is runnign
                }
            }
            else if (setpos != NULL)
            {
                log_d("setpos");
                cJSON *stprs = cJSON_GetObjectItem(setpos, key_steppers);
                if (stprs != NULL)
                {
                    cJSON *stp = NULL;
                    cJSON_ArrayForEach(stp, stprs)
                    {
                        Stepper s = static_cast<Stepper>(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint);
                        setPosition(s, cJSON_GetObjectItemCaseSensitive(stp, key_currentpos)->valueint);
                        log_i("Setting motor position to %i", cJSON_GetObjectItemCaseSensitive(stp, key_currentpos)->valueint);
                    }
                }
            }
            else
            {
                log_i("Motor steppers json is null");
            }
        }
        else
        {
            log_i("Motor json is null");
        }
#endif
    }

    int act(cJSON *doc)
    {
        // parse json to motor struct and send over I2C
        parseMotorJsonI2C(doc);
        return 0;
    }

    cJSON *get(cJSON *ob)
    {
        // do nothing
        return NULL;
    }

    MotorState pullMotorDataI2CDriver(int axis)
    {
        // we pull the data from the slave's register
        uint8_t slave_addr = axis2address(axis);

        // Request data from the slave but only if inside i2cAddresses
        if (!isAddressInI2CDevices(slave_addr))
        {
            // log_e("Error: I2C slave address %i not found in i2cAddresses", slave_addr);
            return MotorState();
        }

        // read the data from the slave
        const int maxRetries = 2;
        int retryCount = 0;
        bool success = false;
        MotorState motorState; // Initialize with default values
        while (retryCount < maxRetries && !success)
        {
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
            motorState.currentPosition = FocusMotor::getData()[axis]->currentPosition; // Fallback-Wert setzen
            log_e("Error: Failed to read correct data size from address %i after %i attempts", slave_addr, maxRetries);
        }

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
            log_e("Error (pull): Dial address not found in i2cAddresses");
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
                pushMotorPosToDial();
                return;
            }

            // compare the current position of the motor with the dial state and drive the motor to the dial state
            for (int iMotor = 0; iMotor < 4; iMotor++)
            {
                long position2go = 0;
                if (iMotor == 0)
                    position2go = mDialData.pos_a;
                if (iMotor == 1)
                    position2go = mDialData.pos_x;
                if (iMotor == 2)
                    position2go = mDialData.pos_y;
                if (iMotor == 3)
                    position2go = mDialData.pos_z;

                // check if current position and position2go are within a reasonable range
                // if not we don't want to move the motor
                // log_i("Motor %i: Current position: %i, Dial position: %i", iMotor, FocusMotor::getData()[iMotor]->currentPosition, position2go);
                if (false & abs(FocusMotor::getData()[iMotor]->currentPosition - position2go) > 10000)
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

                // compare old dial data; if same value, don't move the motor
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

                // Here we drive the motor to the dial state
                Stepper mStepper = static_cast<Stepper>(iMotor);
                // we dont have that on master side
                // setAutoEnable(false);
                // setEnable(true);
                // we can send the data to the slave directly
                // #TODO: @KillerInk - for some reason, the motor never triggers a sendPos after we start the motor from here - isstop is always true..
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
                data[iMotor]->stopped = false;
                // we want to start a motor no matter if it's connected via I2C or natively
                log_i("Motor %i: Drive to position %i, at speed %i, stopped %i", mStepper, FocusMotor::getData()[mStepper]->targetPosition, FocusMotor::getData()[mStepper]->speed, FocusMotor::getData()[mStepper]->stopped);
                FocusMotor::startStepper(mStepper, true);
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

    void loop()
    {
        for (int i = 0; i < 4; i++)
        {
            // if motor is connected via I2C, we have to pull the data from the slave's register
            if (pullMotorDataI2CDriverTick[i] > 2) // every second loop
            {
                // TODO: @killerink - should this be done in background to not block the main loop?
                MotorState mMotorState = pullMotorDataI2CDriver(i);
                FocusMotor::getData()[i]->currentPosition = mMotorState.currentPosition;
                pullMotorDataI2CDriverTick[i] = 0;
                // log_i("Motor %i: Current position: %i, isRunning: %i, firstRun: %i, stopped: %i", i, mMotorState.currentPosition, mMotorState.isRunning, waitForFirstRunI2CSlave[i], data[i]->stopped);
                if (waitForFirstRunI2CSlave[i])
                { // we need to wait for the response from the slave to be sure that the motor is running (e.g. motor needs to run before checking if it is stopped)
                    if (mMotorState.isRunning)
                    {
                        waitForFirstRunI2CSlave[i] = false;
                    }
                }
                else
                {
                    // TODO: check if motor is still running and if not, report position to serial
                    if (!mMotorState.isRunning && !data[i]->stopped)
                    {
                        // TODO: REadout register on slave side and check if destination
                        // Only send the information when the motor is halting
                        log_i("Stop Motor %i in loop, isRunning %i, data[i]->stopped %i", i, mMotorState.isRunning, data[i]->stopped);
                        sendMotorPos(i, 0);
                        data[i]->stopped = true;
                        const char *prefNamespace = "UC2";
                        preferences.begin(prefNamespace, false);
                        preferences.putLong(("motor" + String(i)).c_str(), FocusMotor::getData()[i]->currentPosition); // tODO @KillerInk: This is not the current position - which one to put?
                        preferences.end();
                    }
                }
            }
            else
            {
                pullMotorDataI2CDriverTick[i]++;
            }
        }
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