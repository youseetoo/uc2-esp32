#include "i2c_master.h"
#include <PinConfig.h>
#include "Wire.h"

#include "JsonKeys.h"
#include <Preferences.h>
#ifdef DIAL_CONTROLLER
#include "../dial/DialController.h"
#endif
#ifdef LASER_CONTROLLER
#include "../laser/LaserController.h"
#endif

namespace i2c_master
{

    // for A,X,Y,Z intialize the I2C addresses
    uint8_t i2c_addresses[] = {
        pinConfig.I2C_ADD_MOT_A,
        pinConfig.I2C_ADD_MOT_X,
        pinConfig.I2C_ADD_MOT_Y,
        pinConfig.I2C_ADD_MOT_Z};
    // keep track of the motor states
    MotorData a_dat;
    MotorData x_dat;
    MotorData y_dat;
    MotorData z_dat;
    MotorData *data[4];
    const int MAX_I2C_DEVICES = 20;     // Maximum number of expected devices
    byte i2cAddresses[MAX_I2C_DEVICES]; // Array to store found I2C addresses
    int numDevices = 0;                 // Variable to keep track of number of devices found
    bool waitForFirstRunI2CSlave[4] = {false, false, false, false};
    int i2cRescanTick = 0;         // Variable to keep track of number of devices found
    int i2cRescanAfterNTicks = -1; // Variable to keep track of number of devices found
    int pullMotorDataI2CTick[4] = {0, 0, 0, 0};
    Preferences preferences;
#ifdef DIAL_CONTROLLER
    DialData mDialData;
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
        data[Stepper::A] = &a_dat;
        data[Stepper::X] = &x_dat;
        data[Stepper::Y] = &y_dat;
        data[Stepper::Z] = &z_dat;

        const char *prefNamespace = "UC2";
        preferences.begin(prefNamespace, false);

        if (pinConfig.MOTOR_A_STEP >= 0)
        {
            data[Stepper::A]->dirPin = pinConfig.MOTOR_A_DIR;
            data[Stepper::A]->stpPin = pinConfig.MOTOR_A_STEP;
            data[Stepper::A]->currentPosition = preferences.getLong(("motor" + String(Stepper::A)).c_str());
            log_i("Motor A position: %i", data[Stepper::A]->currentPosition);
        }
        if (pinConfig.MOTOR_X_STEP >= 0)
        {
            data[Stepper::X]->dirPin = pinConfig.MOTOR_X_DIR;
            data[Stepper::X]->stpPin = pinConfig.MOTOR_X_STEP;
            data[Stepper::X]->currentPosition = preferences.getLong(("motor" + String(Stepper::X)).c_str());
            log_i("Motor X position: %i", data[Stepper::X]->currentPosition);
        }
        if (pinConfig.MOTOR_Y_STEP >= 0)
        {
            data[Stepper::Y]->dirPin = pinConfig.MOTOR_Y_DIR;
            data[Stepper::Y]->stpPin = pinConfig.MOTOR_Y_STEP;
            data[Stepper::Y]->currentPosition = preferences.getLong(("motor" + String(Stepper::Y)).c_str());
            log_i("Motor Y position: %i", data[Stepper::Y]->currentPosition);
        }
        if (pinConfig.MOTOR_Z_STEP >= 0)
        {
            data[Stepper::Z]->dirPin = pinConfig.MOTOR_Z_DIR;
            data[Stepper::Z]->stpPin = pinConfig.MOTOR_Z_STEP;
            data[Stepper::Z]->currentPosition = preferences.getLong(("motor" + String(Stepper::Z)).c_str());
            log_i("Motor Z position: %i", data[Stepper::Z]->currentPosition);
        }
        preferences.end();
        // if TCA is active wire doesn't work
        Wire.begin(pinConfig.I2C_SDA, pinConfig.I2C_SCL, 100000); // 400 Khz is necessary for the M5Dial
        i2c_scan();
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
        mDialData.pos_a = getData()[0]->currentPosition;
        mDialData.pos_x = getData()[1]->currentPosition;
        mDialData.pos_y = getData()[2]->currentPosition;
        mDialData.pos_z = getData()[3]->currentPosition;
#ifdef LASER_CONTROLLER
        mDialData.intensity = LaserController::getLaserVal(1);
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

#ifdef DIAL_CONTROLLER
        // update the dial with the actual motor positions
        // the motor positions array is updated in the dial controller
        pushMotorPosToDial();
#endif

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
    }

    int axis2address(int axis)
    {
        if (axis >= 0 && axis < 4)
        {
            return i2c_addresses[axis];
        }
        return 0;
    }

    void sendMotorDataI2C(MotorData motorData, uint8_t axis)
    {
        uint8_t slave_addr = axis2address(axis);
        log_i("MotorData to axis: %i, isStop: %i ", axis, motorData.isStop);

        // TODO: should we have this inside the I2C controller?
        Wire.beginTransmission(slave_addr);

        // Cast the structure to a byte array
        uint8_t *dataPtr = (uint8_t *)&motorData;
        int dataSize = sizeof(MotorData);

        // Send the byte array over I2C
        Wire.write(dataPtr, dataSize);
        int err = Wire.endTransmission();
        if (err != 0)
        {
            log_e("Error sending motor data to I2C slave at address %i", slave_addr);
        }
        else
        {
            log_i("Motor data sent to I2C slave at address %i", slave_addr);
        }
    }

    void startStepper(int axis)
    {
        // Request data from the slave but only if inside i2cAddresses
        uint8_t slave_addr = axis2address(axis);
        if (!isAddressInI2CDevices(slave_addr))
        {
            data[axis]->stopped = true; // stop immediately, so that the return of serial gives the current position
            sendMotorPos(axis, 0);      // this is an exception. We first get the position, then the success
        }
        else
        {
            // we need to wait for the response from the slave to be sure that the motor is running (e.g. motor needs to run before checking if it is stopped)
            sendMotorDataI2C(*data[axis], axis); // TODO: This cannot send two motor information simultaenosly

            waitForFirstRunI2CSlave[axis] = true;
            data[axis]->stopped = false;
        }
    }

    void stopStepper(int i)
    {
        // only send motor data if it was running before
        if (!data[i]->stopped)
            sendMotorPos(i, 0);
        log_i("Stop Motor %i", i);
        data[i]->stopped = true;     // FIME: difference between stopped and isStop?
        data[i]->isStop = true;      // FIME: We should send only those bits that are relevant (e.g. start/stop + payload bytes)
        data[i]->targetPosition = 0; // weird bug, probably not interpreted correclty on slave: If we stop the motor it's taking the last position and moves twice
        data[i]->absolutePosition = false;
        sendMotorDataI2C(*data[i], i); // TODO: This cannot send two motor information simultaenosly
    }

    void toggleStepper(Stepper s, bool isStop)
    {
        if (isStop)
        {
            log_i("stop stepper from parseMotorDriveJson");
            stopStepper(s);
        }
        else
        {
            log_i("start stepper from parseMotorDriveJson");
            startStepper(s); // TODO: Need dual axis?
        }
    }

    void parseJsonI2C(cJSON *doc)
    {
        /*
        We parse the incoming JSON string to the motor struct and send it via I2C to the correpsonding motor driver
        // TODO: We could reuse the parseMotorDriveJson function and just add the I2C send function?
        */
        log_i("parseJsonI2C");
        cJSON *mot = cJSON_GetObjectItemCaseSensitive(doc, key_motor);
        if (mot != NULL)
        {
            cJSON *stprs = cJSON_GetObjectItemCaseSensitive(mot, key_steppers);
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
                    toggleStepper(s, data[s]->isStop);
                    log_i("Motor %i: Speed: %i, Position: %i, isStop: %i", s, data[s]->speed, data[s]->targetPosition, data[s]->isStop);
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
    }

    int act(cJSON *doc)
    {
        // parse json to motor struct and send over I2C
        parseJsonI2C(doc);
        return 0;
    }

    cJSON *get(cJSON *ob){
        // do nothing
        return NULL;
    }

    MotorState pullMotorDataI2C(int axis)
    {
        // we pull the data from the slave's register
        uint8_t slave_addr = axis2address(axis);

        // Request data from the slave but only if inside i2cAddresses
        if (!isAddressInI2CDevices(slave_addr))
        {
            // log_e("Error: I2C slave address %i not found in i2cAddresses", slave_addr);
            return MotorState();
        }
        Wire.requestFrom(slave_addr, sizeof(MotorState));
        MotorState motorState; // Initialize with default values
        // Check if the expected amount of data is received
        if (Wire.available() == sizeof(MotorState))
        {
            Wire.readBytes((uint8_t *)&motorState, sizeof(motorState));
        }
        else
        {
            log_e("Error: Incorrect data size received");
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
                // assign the dial state to the motor
                // if we run in forever mode we don't want to change the position as we likely use the ps4 controller
                // check if homeMotor is null

                // code below does nothing

                /*bool isMotorHoming = false;
                if (not(HomeMotor::hdata[iMotor] == nullptr))
                {
                    isMotorHoming = HomeMotor::hdata[iMotor]->homeIsActive;
                }
                bool isMotorForever = getData()[iMotor]->isforever;*/
                /*if (isMotorForever || isMotorHoming ||
                    getData()[iMotor]->currentPosition == position2go)
                    continue;*/
                log_i("Motor %i: Current position: %i, Dial position: %i", iMotor, getData()[iMotor]->currentPosition, position2go);

                // check if current position and position2go are within a reasonable range
                // if not we don't want to move the motor
                if (abs(getData()[iMotor]->currentPosition - position2go) > 10000)
                {
                    log_e("Error: Motor %i is too far away from dial position %i", iMotor, position2go);
                }
                // Here we drive the motor to the dial state
                Stepper mStepper = static_cast<Stepper>(iMotor);
                // we dont have that on master side
                // setAutoEnable(false);
                // setEnable(true);
                getData()[mStepper]->absolutePosition = 1;
                getData()[mStepper]->targetPosition = position2go;
                getData()[mStepper]->isforever = 0;
                getData()[mStepper]->isaccelerated = 1;
                getData()[mStepper]->acceleration = 10000;
                getData()[mStepper]->speed = 10000;
                getData()[mStepper]->isEnable = 1;
                getData()[mStepper]->qid = 0;
                getData()[mStepper]->isStop = 0;
                startStepper(mStepper);
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
            if (pullMotorDataI2CTick[i] > 2) // every second loop
            {
                // TODO: @killerink - should this be done in background to not block the main loop?
                MotorState mMotorState = pullMotorDataI2C(i);
                data[i]->currentPosition = mMotorState.currentPosition;
                pullMotorDataI2CTick[i] = 0;
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
                        // log_d("Sending motor pos %i", i);
                        // sendMotorPos(i, 0);
                        stopStepper(i);
                        data[i]->stopped = true;
                        preferences.begin("motpos", false);
                        preferences.putLong(("motor" + String(i)).c_str(), data[i]->currentPosition);
                        preferences.end();
                    }
                }
            }
            else
            {
                pullMotorDataI2CTick[i]++;
            }
        }
#ifdef DIAL_CONTROLLER
        if (ticksLastPosPulled >= ticksPosPullInterval && positionsPushedToDial)
        {
            ticksLastPosPulled = 0;
            // Here we want to pull the dial data from the I2C bus and assign it to the motors
            pullParamsFromDial();
        }
#endif
    }

    MotorData **getData()
    {
        return data;
    }

}