#include "MotorJsonParser.h"
#include "FocusMotor.h"
#ifdef STAGE_SCAN
#include "StageScan.h"
#include "FocusScan.h"
#endif

#ifdef STAGE_SCAN
#include "StageScan.h"
#endif

namespace MotorJsonParser
{

	cJSON *get(cJSON *docin)
	{
		/*
		{"task": "/motor_get", "qid": 1}
		returns
{
		"motor":        {
				"ste:   [{
								"stepperid":    0,
								"position":     -157500,
								"trigOff":      0,
								"trigPer"                               "isDualAxisZ":  0,
								"motorAddress": 67
						}]
		},
		"qid":  1
}		}*/

		log_i("get motor");
		cJSON *doc = cJSON_CreateObject();
		int qid = cJsonTool::getJsonInt(docin, "qid");
		cJSON *pos = cJSON_GetObjectItemCaseSensitive(docin, key_position);
		cJSON *stop = cJSON_GetObjectItemCaseSensitive(docin, key_stepperstopped);
		cJSON *mot = cJSON_CreateObject();
		cJSON_AddItemToObject(doc, key_motor, mot);
		cJSON *stprs = cJSON_CreateArray();
		cJSON_AddItemToObject(mot, key_steppers, stprs);

		for (int i = 0; i < MOTOR_AXIS_COUNT; i++)
		{
			if (i == 0 and pinConfig.MOTOR_A_STEP < 0)
				continue;
			if (i == 1 and pinConfig.MOTOR_X_STEP < 0)
				continue;
			if (i == 2 and pinConfig.MOTOR_Y_STEP < 0)
				continue;
			if (i == 3 and pinConfig.MOTOR_Z_STEP < 0)
				continue;
			if (i == 4 and pinConfig.MOTOR_B_STEP < 0)
				continue;
			if (i == 5 and pinConfig.MOTOR_C_STEP < 0)
				continue;
			if (i == 6 and pinConfig.MOTOR_D_STEP < 0)
				continue;
			if (i == 7 and pinConfig.MOTOR_E_STEP < 0)
				continue;
			if (i == 8 and pinConfig.MOTOR_F_STEP < 0)
				continue;
			if (i == 9 and pinConfig.MOTOR_G_STEP < 0)
				continue;

			if (pos != NULL)
			{
				log_i("get motor position");
				// update position and push it to the json
				cJSON *aritem = cJSON_CreateObject();
				cJsonTool::setJsonInt(aritem, key_stepperid, i);
				FocusMotor::updateData(i);
				cJsonTool::setJsonInt(aritem, key_position, FocusMotor::getData()[i]->currentPosition);
				cJSON_AddItemToArray(stprs, aritem);
			}
			else if (stop != NULL)
			{
				log_i("stop motor");
				cJSON *aritem = cJSON_CreateObject();
				cJsonTool::setJsonInt(aritem, key_stepperstopped, !FocusMotor::getData()[i]->stopped);
				cJSON_AddItemToArray(stprs, aritem);
			}
			else // return the whole config
			{
				// update position and push it to the json
				log_i("get motor config");
				FocusMotor::updateData(i);
				cJSON *aritem = cJSON_CreateObject();
				cJsonTool::setJsonInt(aritem, key_stepperid, i);
				cJsonTool::setJsonInt(aritem, key_position, FocusMotor::getData()[i]->currentPosition);
				cJsonTool::setJsonInt(aritem, key_triggeroffset, FocusMotor::getData()[i]->offsetTrigger);
				cJsonTool::setJsonInt(aritem, key_triggerperiod, FocusMotor::getData()[i]->triggerPeriod);
				cJsonTool::setJsonInt(aritem, key_triggerpin, FocusMotor::getData()[i]->triggerPin);
				cJsonTool::setJsonInt(aritem, key_stepperisstop, FocusMotor::getData()[i]->isStop);
				cJsonTool::setJsonInt(aritem, key_stepperisrunning, FocusMotor::isRunning(i));
				cJsonTool::setJsonInt(aritem, key_stepperisDualAxisZ, pinConfig.isDualAxisZ);
				cJsonTool::setJsonInt(aritem, key_stepperisforever, FocusMotor::getData()[i]->isforever);
				cJsonTool::setJsonInt(aritem, key_stepperisen, FocusMotor::getData()[i]->softLimitEnabled);
				cJsonTool::setJsonInt(aritem, key_stepperstopped, FocusMotor::getData()[i]->stopped);

#ifdef I2C_SLAVE_MOTOR
				cJsonTool::setJsonInt(aritem, "motorAddress", i2c_slave_motor::getI2CAddress());
#endif
				cJSON_AddItemToArray(stprs, aritem);
			}
		}
		cJSON_AddItemToObject(doc, "qid", cJSON_CreateNumber(qid));
		return doc;
	}

#ifdef STAGE_SCAN
    void parseStageScan(cJSON *doc)
    {
        // set trigger
        cJSON *settrigger = cJSON_GetObjectItem(doc, key_settrigger);
        // {"task": "/motor_act", "setTrig": {"steppers": [{"stepperid": 1, "trigPin": 1, "trigOff":0, "trigPer":1}]}}
        // {"task": "/motor_act", "setTrig": {"steppers": [{"stepperid": 2, "trigPin": 2, "trigOff":0, "trigPer":1}]}}
        // {"task":"/motor_act","motor":{"steppers": [{ "stepperid": 1, "position": 5000, "speed": 100000, "isabs": 0, "isaccel":0}]}}
        // {"task":"/motor_act","motor":{"steppers": [{ "stepperid": 2, "position": 5000, "speed": 100000, "isabs": 0, "isaccel":0}]}}
        // {"task": "/motor_get"}
        if (settrigger != NULL)
        {
            log_d("settrigger");
            cJSON *stprs = cJSON_GetObjectItem(settrigger, key_steppers);
            if (stprs != NULL)
            {

                cJSON *stp = NULL;
                cJSON_ArrayForEach(stp, stprs)
                {
                    Stepper s = static_cast<Stepper>(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint);
                    FocusMotor::getData()[s]->triggerPin = cJSON_GetObjectItemCaseSensitive(stp, key_triggerpin)->valueint;
                    FocusMotor::getData()[s]->offsetTrigger = cJSON_GetObjectItemCaseSensitive(stp, key_triggeroffset)->valueint;
                    FocusMotor::getData()[s]->triggerPeriod = cJSON_GetObjectItemCaseSensitive(stp, key_triggerperiod)->valueint;
                    log_i("Setting motor trigger offset to %i", cJSON_GetObjectItemCaseSensitive(stp, key_triggeroffset)->valueint);
                    log_i("Setting motor trigger period to %i", cJSON_GetObjectItemCaseSensitive(stp, key_triggerperiod)->valueint);
                    log_i("Setting motor trigger pin ID to %i", cJSON_GetObjectItemCaseSensitive(stp, key_triggerpin)->valueint);
                }
            }
        }

        // start independent stageScan
        //
        cJSON *stagescan = cJSON_GetObjectItem(doc, "stagescan");
        if (stagescan != NULL)
        {
            StageScan::getStageScanData()->stopped = cJsonTool::getJsonInt(stagescan, "stopped");
            if (StageScan::getStageScanData()->stopped)
            {
                log_i("stagescan stopped");
                return;
            }
#if defined CAN_CONTROLLER && !defined CAN_SLAVE_MOTOR
            // CAN-based stage scanning with grid parameters
            // {"task": "/motor_act", "stagescan": {"xStart": 0, "yStart": 0, "xStep": 500, "yStep": 500, "nX": 5, "nY": 5, "tPre": 50, "tPost": 50}}
            StageScan::getStageScanData()->xStart = cJsonTool::getJsonInt(stagescan, "xStart");
            StageScan::getStageScanData()->yStart = cJsonTool::getJsonInt(stagescan, "yStart");
            StageScan::getStageScanData()->xStep = cJsonTool::getJsonInt(stagescan, "xStep");
            StageScan::getStageScanData()->yStep = cJsonTool::getJsonInt(stagescan, "yStep");
            StageScan::getStageScanData()->nX = cJsonTool::getJsonInt(stagescan, "nX");
            StageScan::getStageScanData()->nY = cJsonTool::getJsonInt(stagescan, "nY");
            StageScan::getStageScanData()->delayTimePreTrigger = cJsonTool::getJsonInt(stagescan, "tPre");
            StageScan::getStageScanData()->delayTimePostTrigger = cJsonTool::getJsonInt(stagescan, "tPost");
            StageScan::getStageScanData()->delayTimeTrigger = cJsonTool::getJsonInt(stagescan, "tTrig");
            StageScan::getStageScanData()->speed = max(cJsonTool::getJsonInt(stagescan, "speed"), 20000);
            StageScan::getStageScanData()->acceleration = max(cJsonTool::getJsonInt(stagescan, "acceleration"), 1000000);
            StageScan::getStageScanData()->qid = cJsonTool::getJsonInt(stagescan, "qid");
            StageScan::getStageScanData()->nFrames = cJsonTool::getJsonInt(stagescan, "nFrames");
            
            // Check for coordinate-based scanning
			// {"task": "/motor_act", "stagescan": {"coordinates": [{"x": 100, "y": 200}, {"x": 300, "y": 400}, {"x": 500, "y": 600}], "tPre": 50, "tPost": 50, "led": 100, "illumination": [50, 75, 100, 125], "stopped": 0}}
            cJSON *coordinates = cJSON_GetObjectItem(stagescan, "coordinates");
            if (coordinates != NULL && cJSON_IsArray(coordinates))
            {
				log_i("coordinates array found");
                int coordinateCount = cJSON_GetArraySize(coordinates);
                if (coordinateCount > 0)
                {
					log_i("Using coordinate-based scanning with %d positions", coordinateCount);
                    StageScan::StagePosition* positions = new StageScan::StagePosition[coordinateCount];
                    
                    for (int i = 0; i < coordinateCount; i++)
                    {
                        cJSON *coord = cJSON_GetArrayItem(coordinates, i);
						log_i("Coordinate %d: x=%d, y=%d", i, cJsonTool::getJsonInt(coord, "x"), cJsonTool::getJsonInt(coord, "y"));
                        if (coord != NULL)
                        {
                            positions[i].x = cJsonTool::getJsonInt(coord, "x");
                            positions[i].y = cJsonTool::getJsonInt(coord, "y");
                        }
                    }
                    
                    StageScan::setCoordinates(positions, coordinateCount);
                    delete[] positions; // setCoordinates makes its own copy
                    log_i("Coordinate-based scanning enabled with %d positions", coordinateCount);
                }
				else{
					log_i("No coordinates found, using grid-based scanning");
				}
			}
            else
            {
                // Clear any existing coordinates to use grid-based scanning
                StageScan::clearCoordinates();
				log_i("Using grid-based scanning");
            }
            
            bool shouldStop = cJsonTool::getJsonInt(stagescan, "stopped");
            if (shouldStop)
            {
                log_i("stagescan stopped");
                StageScan::getStageScanData()->stopped = 1;
                return;
            }
            if (StageScan::isRunning)
            {
                log_i("stagescan already running");
                return;
            }
            StageScan::getStageScanData()->stopped = 0;
            
            // Parse illumination array
            cJSON *illumination = cJSON_GetObjectItem(stagescan, "illumination");
            if (illumination != NULL)
            {
                log_i("illumination array found");
                for (int i = 0; i < 4; i++)
                {
                    cJSON *item = cJSON_GetArrayItem(illumination, i);
                    if (item != NULL)
                    {
                        StageScan::getStageScanData()->lightsourceIntensities[i] = item->valueint;
                        log_i("illumination %i: %i", i, StageScan::getStageScanData()->lightsourceIntensities[i]);
                    }
                    else
                    {
                        log_i("illumination %i not found", i);
                        StageScan::getStageScanData()->lightsourceIntensities[i] = 0;
                    }
                }
            }
            
            // Parse LED array
            StageScan::getStageScanData()->ledarrayIntensity = cJsonTool::getJsonInt(stagescan, "led");
            
            log_i("StageScan xStart: %d, yStart: %d, xStep: %d, yStep: %d, nX: %d, nY: %d, tPre: %d, tPost: %d, speed: %d, acceleration: %d, ledarray: %d, stopped: %d, isrunning: %d, useCoordinates: %d",
                  StageScan::getStageScanData()->xStart,
                  StageScan::getStageScanData()->yStart,
                  StageScan::getStageScanData()->xStep,
                  StageScan::getStageScanData()->yStep,
                  StageScan::getStageScanData()->nX,
                  StageScan::getStageScanData()->nY,
                  StageScan::getStageScanData()->delayTimePreTrigger,
                  StageScan::getStageScanData()->delayTimePostTrigger,
                  StageScan::getStageScanData()->speed,
                  StageScan::getStageScanData()->acceleration,
                  StageScan::getStageScanData()->ledarrayIntensity,
                  StageScan::getStageScanData()->stopped, 
                  StageScan::isRunning,
                  StageScan::getStageScanData()->useCoordinates);

            xTaskCreate(StageScan::stageScanThread, "stageScan", pinConfig.STAGESCAN_TASK_STACKSIZE, NULL, 0, NULL);
#else
            // Non-CAN stage scanning with traditional grid parameters
            StageScan::getStageScanData()->nStepsLine = cJsonTool::getJsonInt(stagescan, "nStepsLine");
            StageScan::getStageScanData()->dStepsLine = cJsonTool::getJsonInt(stagescan, "dStepsLine");
            StageScan::getStageScanData()->nTriggerLine = cJsonTool::getJsonInt(stagescan, "nTriggerLine");
            StageScan::getStageScanData()->nStepsPixel = cJsonTool::getJsonInt(stagescan, "nStepsPixel");
            StageScan::getStageScanData()->dStepsPixel = cJsonTool::getJsonInt(stagescan, "dStepsPixel");
            StageScan::getStageScanData()->nTriggerPixel = cJsonTool::getJsonInt(stagescan, "nTriggerPixel");
            StageScan::getStageScanData()->delayTimeStep = cJsonTool::getJsonInt(stagescan, "delayTimeStep");
            StageScan::getStageScanData()->nFrames = cJsonTool::getJsonInt(stagescan, "nFrames");
            
            // Check for coordinate-based scanning (also supported in non-CAN mode)
            cJSON *coordinates = cJSON_GetObjectItem(stagescan, "coordinates");
            if (coordinates != NULL && cJSON_IsArray(coordinates))
            {
                int coordinateCount = cJSON_GetArraySize(coordinates);
                if (coordinateCount > 0)
                {
                    StageScan::StagePosition* positions = new StageScan::StagePosition[coordinateCount];
                    
                    for (int i = 0; i < coordinateCount; i++)
                    {
                        cJSON *coord = cJSON_GetArrayItem(coordinates, i);
                        if (coord != NULL)
                        {
                            positions[i].x = cJsonTool::getJsonInt(coord, "x");
                            positions[i].y = cJsonTool::getJsonInt(coord, "y");
                        }
                    }
                    
                    StageScan::setCoordinates(positions, coordinateCount);
                    delete[] positions; // setCoordinates makes its own copy
                    log_i("Coordinate-based scanning enabled with %d positions", coordinateCount);
                }
            }
            else
            {
                // Clear any existing coordinates to use grid-based scanning
                StageScan::clearCoordinates();
            }
            
            xTaskCreate(StageScan::stageScanThread, "stageScan", pinConfig.STAGESCAN_TASK_STACKSIZE, NULL, 0, NULL);
#endif
		}

		#if defined CAN_CONTROLLER && !defined CAN_SLAVE_MOTOR
		// start independent focusScan
		cJSON *focusscan = cJSON_GetObjectItem(doc, "focusscan");
		if (focusscan != NULL)
		{
			/*
			{ "task": "/motor_act", "focusscan": { "zStart": 0, "zStep": 50, "nZ": 20, "tPre": 50, "tTrig": 20, "tPost": 50, "led": 255, "illumination": [0, 0, 0, 0], "speed": 20000, "acceleration": 1000000, "qid": 42 }}
			 { "task": "/motor_act", "focusscan": { "zStart": 0, "zStep": 50, "nZ": 20, "tPre": 80, "tTrig": 20, "tPost": 0, "led": 0, "illumination": [0, 255, 0, 0], "speed": 20000, "acceleration": 1000000, "qid": 42 }}
			*/ 
			auto *fsd = FocusScan::getFocusScanData();

			fsd->stopped = cJsonTool::getJsonInt(focusscan, "stopped");
			if (fsd->stopped)          { log_i("focusscan stopped"); return; }
			if (FocusScan::isRunning)  { log_i("focusscan already running"); return; }

			fsd->zStart = cJsonTool::getJsonInt(focusscan, "zStart");
			fsd->zStep  = cJsonTool::getJsonInt(focusscan, "zStep");
			fsd->nZ     = cJsonTool::getJsonInt(focusscan, "nZ");

			fsd->delayTimePreTrigger  = cJsonTool::getJsonInt(focusscan, "tPre");
			fsd->delayTimeTrigger     = cJsonTool::getJsonInt(focusscan, "tTrig");
			fsd->delayTimePostTrigger = cJsonTool::getJsonInt(focusscan, "tPost");

			fsd->speed        = max(cJsonTool::getJsonInt(focusscan, "speed"),        20000);
			fsd->acceleration = max(cJsonTool::getJsonInt(focusscan, "acceleration"), 1000000);
			fsd->qid          = cJsonTool::getJsonInt(focusscan, "qid");

			cJSON *illum = cJSON_GetObjectItem(focusscan, "illumination");
			for (int i = 0; i < 4; ++i)
			{
				fsd->lightsourceIntensities[i] = 0;
				if (illum)
				{
					cJSON *item = cJSON_GetArrayItem(illum, i);
					if (item) fsd->lightsourceIntensities[i] = item->valueint;
				}
			}
			fsd->ledarrayIntensity = cJsonTool::getJsonInt(focusscan, "led");
			fsd->stopped = 0;

			log_i("FocusScan zStart:%d zStep:%d nZ:%d tPre:%d tTrig:%d tPost:%d speed:%d accel:%d led:%d",
				fsd->zStart, fsd->zStep, fsd->nZ,
				fsd->delayTimePreTrigger, fsd->delayTimeTrigger, fsd->delayTimePostTrigger,
				fsd->speed, fsd->acceleration, fsd->ledarrayIntensity);

			xTaskCreate(FocusScan::focusScanThread,
						"focusScan",
						pinConfig.STAGESCAN_TASK_STACKSIZE,
						nullptr,
						0,
						nullptr);
		}
		#endif


	}

#endif

	void parseEnableMotor(cJSON *doc)
	{
		cJSON *isen = cJSON_GetObjectItemCaseSensitive(doc, key_isen);
		if (isen != NULL)
		{
			FocusMotor::setEnable(isen->valueint);
		}
	}

	void parseSetAxis(cJSON *doc)
	{
#ifdef I2C_SLAVE_MOTOR
		cJSON *setaxis = cJSON_GetObjectItem(doc, key_setaxis);
		if (setaxis != NULL)
		{
			cJSON *stprs = cJSON_GetObjectItem(setaxis, key_steppers);
			if (stprs != NULL)
			{
				cJSON *stp = NULL;
				cJSON_ArrayForEach(stp, stprs)
				{
					Stepper s = static_cast<Stepper>(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint);
					int axis = cJSON_GetObjectItemCaseSensitive(stp, key_stepperaxis)->valueint;
					int motorAddress = i2c_addresses[axis];
					// set the I2C address of the motor
					log_i("Setting motor axis %i to %i, address:", s, axis, i2c_slave_motor::getI2CAddress());
					i2c_slave_motor::setI2CAddress(motorAddress);
				}
			}
		}
#endif
	}

	void parseAutoEnableMotor(cJSON *doc)
	{
		cJSON *autoen = cJSON_GetObjectItemCaseSensitive(doc, key_isenauto);
#ifdef USE_FASTACCEL
		if (autoen != NULL)
		{
			FocusMotor::setAutoEnable(autoen->valueint);
		}
#endif
	}

	bool parseSetPosition(cJSON *doc)
	{
		// set position
		cJSON *setpos = cJSON_GetObjectItem(doc, key_setposition);
		// {"task": "/motor_act", "setpos": {"steppers": [{"stepperid": 0, "posval": 100}, {"stepperid": 1, "posval": 0}, {"stepperid": 2, "posval": 0}, {"stepperid": 3, "posval": 0}]}}

		// set dual axis if necessary
		cJSON *dualaxisZ = cJSON_GetObjectItemCaseSensitive(doc, key_home_isDualAxis);
		if (dualaxisZ != NULL)
		{
			// {"task": "/motor_act", "isDualAxisZ": 1}
			// store this in the preferences
			Preferences preferences;
			FocusMotor::setDualAxisZ(dualaxisZ->valueint);
			;
			const char *prefNamespace = "UC2";
			preferences.begin(prefNamespace, false);
			preferences.putBool("dualAxZ", dualaxisZ->valueint);
			log_i("isDualAxisZ is set to: %i", dualaxisZ->valueint);
			preferences.end();
		}
		// {"task": "/motor_act", "setpos": {"steppers": [{"stepperid": 0, "posval": 100}, {"stepperid": 1, "posval": 0}, {"stepperid": 2, "posval": 0}, {"stepperid": 3, "posval": 0}]}}
		if (setpos != NULL)
		{
			log_d("setpos");
			cJSON *stprs = cJSON_GetObjectItem(setpos, key_steppers);
			if (stprs != NULL)
			{
				cJSON *stp = NULL;
				cJSON_ArrayForEach(stp, stprs)
				{
					Stepper s = static_cast<Stepper>(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint);
					FocusMotor::setPosition(s, cJSON_GetObjectItemCaseSensitive(stp, key_currentpos)->valueint);
					log_i("Setting motor position to %i", cJSON_GetObjectItemCaseSensitive(stp, key_currentpos)->valueint);
				}
			}
			return true; // motor will return pos per socket or serial inside loop. act is fire and forget
		}
		return false;
	}

	static void parseSetSoftLimits(cJSON *doc)
	{

		/*
		{"task": "/motor_act", "softlimits": {"steppers": [{"stepperid": 1, "min": -100000, "max": 10000, "isen": 1}]}}
		*/

		cJSON *softObj = cJSON_GetObjectItemCaseSensitive(doc, "softlimits");
		if (!softObj)
		{
			return; // no "softlimits" key => nothing to do
		}
		cJSON *stprs = cJSON_GetObjectItemCaseSensitive(softObj, key_steppers);
		if (!stprs)
		{
			return; // no "steppers" array => nothing to do
		}
		cJSON *stp = nullptr;
		cJSON_ArrayForEach(stp, stprs)
		{
			log_i("Set softlimits: stepperid %i", cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint);
			// check if all required items are present and valid
			Preferences preferences;
			cJSON *idItem = cJSON_GetObjectItemCaseSensitive(stp, key_stepperid);
			cJSON *minItem = cJSON_GetObjectItemCaseSensitive(stp, key_steppermin);
			cJSON *maxItem = cJSON_GetObjectItemCaseSensitive(stp, key_steppermax);
			cJSON *isEnabled = cJSON_GetObjectItemCaseSensitive(stp, key_stepperisen);

			// storing the values in preferences
			const char *prefNamespace = "motpos";
			preferences.begin(prefNamespace, false);
			preferences.putInt(("min" + String(idItem->valueint)).c_str(), idItem->valueint);
			preferences.putInt(("max" + String(idItem->valueint)).c_str(), idItem->valueint);
			preferences.putInt(("isen" + String(idItem->valueint)).c_str(), idItem->valueint);
			preferences.end();
			if (!cJSON_IsNumber(idItem) || !cJSON_IsNumber(minItem) || !cJSON_IsNumber(maxItem))
			{
				continue; // skip invalid
			}
			int axis = idItem->valueint;
			int32_t mn = minItem->valueint;
			int32_t mx = maxItem->valueint;
			bool isEnabledVal = isEnabled ? isEnabled->valueint : false;
			log_i("Set softlimits: stepperid %i, min %i, max %i, isEnabled %i", axis, mn, mx, isEnabledVal);

			FocusMotor::setSoftLimits(axis, mn, mx, isEnabledVal);
		}
	}

	int countKeysExcludingQID(cJSON *obj)
	{
		int count = 0;
		for (cJSON *child = obj->child; child != nullptr; child = child->next)
		{
			// "qid" (oder andere Keys) ausschließen
			if (strcmp(child->string, "qid") == 0)
			{
				continue;
			}
			count++;
		}
		return count;
	}

	void parseMotorDriveJson(cJSON *doc)
	{
		/*
		We receive a JSON string e.g. in the form:
		{"task": "/motor_act", "motor": {"steppers": [{"stepperid": 1, "position": -10000, "speed": 20000, "isabs": 0.0, "isaccel": 1, "accel":10000, "isen": true}]}, "qid": 5}
		And assign it to the different motors by sending the converted MotorData to the corresponding motor driver via I2C
		// for single value
		// {"task": "/motor_act", "motor": {"steppers": [{"stepperid": 2, "speed": 10000, "redu":2}]}, "qid": 5}
		// {"task": "/motor_act", "motor": {"steppers": [{"stepperid": 2, "position": 10000, "redu":2}]}, "qid": 5}

		*/
		cJSON *mot = cJSON_GetObjectItemCaseSensitive(doc, key_motor);
		if (mot != NULL)
		{
			cJSON *stprs = cJSON_GetObjectItemCaseSensitive(mot, key_steppers);
			cJSON *stp = NULL;
			if (stprs != NULL)
			{
				cJSON_ArrayForEach(stp, stprs)
				{
					log_i("start stepper from parseMotorDriveJson");
					Stepper s = static_cast<Stepper>(cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)));

					/*
					#ifdef CAN_CONTROLLER
					// CORE IDEA: if we have a single value, only send thos value to the CAN bus
					// compute the number of keys - if we have a single element (excluding qid), we can use can_controller::sendMotorSingleValue(s, offsetof(MotorData, speed), (int)motorSpeed);
					int nKeyse = countKeysExcludingQID(stp);
					if (nKeyse == 1)
					{
						// only one key, so we can use the single value function
						log_i("Only one key in the JSON object, using single value function");
						log_i("position: %d", cJSON_GetJsonInt(stp, key_position));
						// send value to CAN
						//can_controller::sendMotorSingleValue(s, offsetof(MotorData, targetPosition), cJSON_GetJsonInt(stp, key_position));
						// start the motor
						//can_controller::sendMotorSingleValue(s, offsetof(MotorData, stopped), false);
						continue; // skip this motor
					}
					*/
					FocusMotor::getData()[s]->qid = cJsonTool::getJsonInt(doc, "qid");
					FocusMotor::getData()[s]->speed = cJsonTool::getJsonInt(stp, key_speed);
					FocusMotor::getData()[s]->isEnable = cJsonTool::getJsonInt(stp, key_isen);
					FocusMotor::getData()[s]->targetPosition = cJsonTool::getJsonInt(stp, key_position);
					FocusMotor::getData()[s]->isforever = cJsonTool::getJsonInt(stp, key_isforever);
					FocusMotor::getData()[s]->absolutePosition = cJsonTool::getJsonInt(stp, key_isabs);
					FocusMotor::getData()[s]->acceleration = cJsonTool::getJsonInt(stp, key_acceleration);
					FocusMotor::getData()[s]->isaccelerated = cJsonTool::getJsonInt(stp, key_isaccel);
					// FocusMotor::getData()[s]->isStop = cJsonTool::getJsonInt(stp, key_isstop);
					int isReduced = cJsonTool::getJsonInt(stp, key_isReduced);

					// check if soft limits are enabled and if the target position is within the limits
					if (FocusMotor::getData()[s]->softLimitEnabled)
					{
						int32_t pos = FocusMotor::getData()[s]->targetPosition;
						int32_t minPos = FocusMotor::getData()[s]->minPos;
						int32_t maxPos = FocusMotor::getData()[s]->maxPos;

						// for absolute position, we need to check the absolute value
						if (FocusMotor::getData()[s]->absolutePosition)
						{
							if (pos < minPos || pos > maxPos)
							{
								log_i("Motor %d target position %d outside soft limits (%d, %d) => STOP", s, pos, minPos, maxPos);
								FocusMotor::stopStepper(s);
								continue; // skip this motor
							}
						}
						else // relative position
						{
							// check if the target position is within the limits
							if (pos + FocusMotor::getData()[s]->currentPosition < minPos || pos + FocusMotor::getData()[s]->currentPosition > maxPos)
							{
								log_i("Motor %d target position %d outside soft limits (%d, %d) => STOP", s, pos, minPos, maxPos);
								FocusMotor::stopStepper(s);
								continue; // skip this motor
							}
						}
					}

					cJSON *cstop = cJSON_GetObjectItemCaseSensitive(stp, key_isstop);
					bool isStop = (cstop != NULL) ? cstop->valueint : false;
					FocusMotor::toggleStepper(s, isStop, isReduced); // not reduced
				}
			}
			else
				log_i("Motor steppers json is null");
		}
		else
			log_i("Motor json is null");
	}

	int act(cJSON *doc)
	{
		log_i("motor act");
		int qid = cJsonTool::getJsonInt(doc, "qid");

		// only enable/disable motors
		// {"task":"/motor_act", "isen":1, "isenauto":1}
		parseEnableMotor(doc);

		// only set motors to autoenable
		// {"task":"/motor_act", "isen":1, "isenauto":1, "qid":1}
		parseAutoEnableMotor(doc);

		// set position of motors in eeprom
		// {"task": "/motor_act", "setpos": {"steppers": [{"stepperid": 0, "posval": 1000}]}, "qid": 37}
		parseSetPosition(doc);

		// set axis of motors
		parseSetAxis(doc);

		// set soft limits of motors
		// {"task": "/motor_act", "softlimits": {"steppers": [{"stepperid": 1, "min": -100000, "max": 10000, "isen": 1}]}}
		parseSetSoftLimits(doc);

		// move motor drive
		// {"task": "/motor_act", "motor": {"steppers": [{"stepperid": 1, "position": 0, "speed": 20000, "isabs": 1, "isaccel": 1, "accel":20000, "isen": true}]}, "qid": 5}
		parseMotorDriveJson(doc);

#ifdef STAGE_SCAN
		parseStageScan(doc);
#endif
		return qid;
	}
}