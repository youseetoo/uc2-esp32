// T4.1: pure C (was the C++ MotorJsonParser namespace).
static const char *TAG = "MotorJsonParser";
#include <stdlib.h>
#include <string.h>
#include <PinConfig.h>
#include "MotorJsonParser.h"
#include "FocusMotor.h"
#include "QidRegistry.h"
#include "cJsonTool.h"
#include "nvs.h"
#ifdef STAGE_SCAN
#include "StageScan.h"
#include "FocusScan.h"
#endif

cJSON *motorjson_get(cJSON *docin)
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
								"trigPer"
								"motorAddress": 67
						}]
		},
		"qid":  1
}		}*/

	log_i("get motor");
	cJSON *doc = cJSON_CreateObject();
	int qid = cjson_tool_get_int(docin, "qid", 0);
	cJSON *pos = cJSON_GetObjectItemCaseSensitive(docin, key_position);
	cJSON *stop = cJSON_GetObjectItemCaseSensitive(docin, key_stepperstopped);
	cJSON *mot = cJSON_CreateObject();
	cJSON_AddItemToObject(doc, key_motor, mot);
	cJSON *stprs = cJSON_CreateArray();
	cJSON_AddItemToObject(mot, key_steppers, stprs);

	for (int i = 0; i < MOTOR_AXIS_COUNT; i++)
	{
		if (i == 0 && pinConfig.MOTOR_A_STEP < 0)
			continue;
		if (i == 1 && pinConfig.MOTOR_X_STEP < 0)
			continue;
		if (i == 2 && pinConfig.MOTOR_Y_STEP < 0)
			continue;
		if (i == 3 && pinConfig.MOTOR_Z_STEP < 0)
			continue;
		if (i == 4 && pinConfig.MOTOR_B_STEP < 0)
			continue;
		if (i == 5 && pinConfig.MOTOR_C_STEP < 0)
			continue;
		if (i == 6 && pinConfig.MOTOR_D_STEP < 0)
			continue;
		if (i == 7 && pinConfig.MOTOR_E_STEP < 0)
			continue;
		if (i == 8 && pinConfig.MOTOR_F_STEP < 0)
			continue;
		if (i == 9 && pinConfig.MOTOR_G_STEP < 0)
			continue;

		if (pos != NULL)
		{
			log_i("get motor position");
			// update position and push it to the json
			cJSON *aritem = cJSON_CreateObject();
			cjson_tool_set_int(aritem, key_stepperid, i);
			focusmotor_update_data(i);

			cjson_tool_set_int(aritem, key_position, focusmotor_get_data()[i]->currentPosition);
			cJSON_AddItemToArray(stprs, aritem);
		}
		else if (stop != NULL)
		{
			log_i("stop motor");
			cJSON *aritem = cJSON_CreateObject();
			cjson_tool_set_int(aritem, key_stepperstopped, !focusmotor_get_data()[i]->stopped);
			cJSON_AddItemToArray(stprs, aritem);
		}
		else // return the whole config
		{
			// update position and push it to the json
			log_i("get motor config");
			focusmotor_update_data(i);
			cJSON *aritem = cJSON_CreateObject();
			cjson_tool_set_int(aritem, key_stepperid, i);

			cjson_tool_set_int(aritem, key_position, focusmotor_get_data()[i]->currentPosition);
		cjson_tool_set_int(aritem, key_triggeroffset, focusmotor_get_data()[i]->offsetTrigger);
		cjson_tool_set_int(aritem, key_triggerperiod, focusmotor_get_data()[i]->triggerPeriod);
		cjson_tool_set_int(aritem, key_triggerpin, focusmotor_get_data()[i]->triggerPin);
		cjson_tool_set_int(aritem, key_stepperisstop, focusmotor_get_data()[i]->isStop);
		cjson_tool_set_int(aritem, key_stepperisrunning, focusmotor_is_running(i));
		cjson_tool_set_int(aritem, key_stepperisforever, focusmotor_get_data()[i]->isforever);
		cjson_tool_set_int(aritem, key_stepperstopped, focusmotor_get_data()[i]->stopped);
		// Hard limit settings
		cjson_tool_set_int(aritem, "hardLimitEnabled", focusmotor_get_data()[i]->hardLimitEnabled);
		cjson_tool_set_int(aritem, "hardLimitPolarity", focusmotor_get_data()[i]->hardLimitPolarity);
		cjson_tool_set_int(aritem, "hardLimitTriggered", focusmotor_get_data()[i]->hardLimitTriggered);
		cJSON_AddItemToArray(stprs, aritem);
		}
	}
	cJSON_AddItemToObject(doc, "qid", cJSON_CreateNumber(qid));
	return doc;
}

#ifdef STAGE_SCAN
    void motorjson_parse_stage_scan(cJSON *doc)
    {
        // start independent stageScan
        //
		log_i("Parsing stage scan command");
        cJSON *stagescan = cJSON_GetObjectItem(doc, "stagescan");
        if (stagescan != NULL)
        { // ACTIVELY USED
            StageScanningData *sd = stage_scan_get_data();
            sd->stopped = cjson_tool_get_int(stagescan, "stopped", 0);
            if (sd->stopped)
            {
                log_i("stagescan stopped");
                return;
            }
            // Stage scanning with grid parameters
            // {"task": "/motor_act", "stagescan": {"xStart": 0, "yStart": 0, "xStep": 5000, "yStep": 5000, "nX": 5, "nY": 5, "tPre": 50, "tPost": 50, "illumination": [50, 75, 100, 125], "zicZac":0}}
            // {"task": "/motor_act", "stagescan": {"coordinates": [{"x": 100, "y": 200}, {"x": 300, "y": 400}, {"x": 500, "y": 600}], "tPre": 50, "tPost": 50, "led": 100, "illumination": [50, 75, 100, 125], "stopped": 0}}
			// With XYZ scanning: {"task": "/motor_act", "stagescan": {"xStart": 0, "yStart": 0, "zStart":0, "xStep": 500, "yStep": 500, "zStep":100, "nX": 5, "nY": 5, "nZ":3, "tPre": 50, "tPost": 50}}
            sd->xStart = cjson_tool_get_int(stagescan, "xStart", 0);
            sd->yStart = cjson_tool_get_int(stagescan, "yStart", 0);
            sd->xStep = cjson_tool_get_int(stagescan, "xStep", 0);
            sd->yStep = cjson_tool_get_int(stagescan, "yStep", 0);
			sd->zStart = cjson_tool_get_int(stagescan, "zStart", 0);
			sd->zStep = cjson_tool_get_int(stagescan, "zStep", 0);
            sd->nX = (cjson_tool_get_int(stagescan, "nX", 1) > 1) ? cjson_tool_get_int(stagescan, "nX", 1) : 1;
            sd->nY = (cjson_tool_get_int(stagescan, "nY", 1) > 1) ? cjson_tool_get_int(stagescan, "nY", 1) : 1;
			sd->nZ = (cjson_tool_get_int(stagescan, "nZ", 1) > 1) ? cjson_tool_get_int(stagescan, "nZ", 1) : 1;
            sd->delayTimePreTrigger = cjson_tool_get_int(stagescan, "tPre", 0);
            sd->delayTimePostTrigger = cjson_tool_get_int(stagescan, "tPost", 0);
            // Trigger pulse width in ms; the firmware floors it at 1 ms.
            sd->delayTimeTrigger = cjson_tool_get_int(stagescan, "tTrig", 1);
            // max() here used to *raise* any requested speed/acceleration to the
            // default, so a slow scan was impossible. Now: value if given, else default.
            {
                int speed = cjson_tool_get_int(stagescan, "speed", 0);
                int accel = cjson_tool_get_int(stagescan, "acceleration", 0);
                if (accel <= 0) accel = cjson_tool_get_int(stagescan, "accel", 0); // UC2-REST < 2026-09 sent "accel"
                sd->speed = speed > 0 ? speed : 20000;
                sd->acceleration = accel > 0 ? accel : 1000000;
            }
            // UC2-REST puts the qid on the outer document; echo it in the completion message.
            sd->qid = cjson_tool_get_int(stagescan, "qid",
                                         cjson_tool_get_int(doc, "qid", -1));
            sd->nFrames = cjson_tool_get_int(stagescan, "nFrames", 0);
            sd->nonstop = cjson_tool_get_int(stagescan, "nonstop", 0);
			sd->zicZac = cjson_tool_get_int(stagescan, "zicZac", 1);

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
                    StagePosition *positions = (StagePosition *)malloc(sizeof(StagePosition) * coordinateCount);

                    for (int i = 0; i < coordinateCount; i++)
                    {
                        cJSON *coord = cJSON_GetArrayItem(coordinates, i);
						log_i("Coordinate %d: x=%d, y=%d", i, cjson_tool_get_int(coord, "x", 0), cjson_tool_get_int(coord, "y", 0));
                        if (coord != NULL)
                        {
                            positions[i].x = cjson_tool_get_int(coord, "x", 0);
                            positions[i].y = cjson_tool_get_int(coord, "y", 0);
                            // no "z": leave the Z axis alone (it used to move to an uninitialised value)
                            positions[i].z = cjson_tool_get_int(coord, "z", STAGE_SCAN_KEEP_AXIS);
                        }
                        else
                        {
                            positions[i].x = 0;
                            positions[i].y = 0;
                            positions[i].z = STAGE_SCAN_KEEP_AXIS;
                        }
                    }

                    stage_scan_set_coordinates(positions, coordinateCount);
                    free(positions); // set_coordinates makes its own copy
                    log_i("Coordinate-based scanning enabled with %d positions", coordinateCount);
                }
				else{
					log_i("No coordinates found, using grid-based scanning");
				}
			}
            else
            {
                // Clear any existing coordinates to use grid-based scanning
                stage_scan_clear_coordinates();
				log_i("Using grid-based scanning");
            }

            bool shouldStop = cjson_tool_get_int(stagescan, "stopped", 0) != 0;
            if (shouldStop)
            {
                log_i("stagescan stopped");
                sd->stopped = 1;
                return;
            }
            if (stage_scan_is_running)
            {
                log_i("stagescan already running");
                return;
            }
            sd->stopped = 0;

            // Parse illumination array
            cJSON *illumination = cJSON_GetObjectItem(stagescan, "illumination");
            if (illumination != NULL)
            {
                log_i("illumination array found");
                for (int i = 0; i < 5; i++)
                {
                    cJSON *item = cJSON_GetArrayItem(illumination, i);
                    if (item != NULL)
                    {
                        sd->lightsourceIntensities[i] = item->valueint;
                        log_i("illumination %i: %i", i, sd->lightsourceIntensities[i]);
                    }
                    else
                    {
                        log_i("illumination %i not found", i);
                        sd->lightsourceIntensities[i] = 0;
                    }
                }
            }

            // Parse LED array
            sd->ledarrayIntensity = cjson_tool_get_int(stagescan, "led", 0);

            log_i("StageScan xStart: %d, yStart: %d,  zStart: %d, xStep: %d, yStep: %d, zStep: %d, nX: %d, nY: %d, nZ: %d, tPre: %d, tPost: %d, speed: %d, acceleration: %d, ledarray: %d, stopped: %d, isrunning: %d, useCoordinates: %d",
                  sd->xStart,
                  sd->yStart,
				  sd->zStart,
                  sd->xStep,
                  sd->yStep,
				  sd->zStep,
                  sd->nX,
                  sd->nY,
				  sd->nZ,
                  sd->delayTimePreTrigger,
                  sd->delayTimePostTrigger,
                  sd->speed,
                  sd->acceleration,
                  sd->ledarrayIntensity,
                  sd->stopped,
                  stage_scan_is_running,
                  sd->useCoordinates);

            // Priority 1 (not 0): the scan must not share the idle priority with
            // everything else that yields, or its ms delays stretch unpredictably.
            xTaskCreate(stage_scan_thread, "stageScan", pinConfig.STAGESCAN_TASK_STACKSIZE, NULL, 1, NULL);

		}

		// start independent focusScan
		cJSON *focusscan = cJSON_GetObjectItem(doc, "focusscan");
		if (focusscan != NULL)
		{
			/*
			{ "task": "/motor_act", "focusscan": { "zStart": 0, "zStep": 50, "nZ": 20, "tPre": 50, "tTrig": 20, "tPost": 50, "led": 255, "illumination": [0, 0, 0, 0], "speed": 20000, "acceleration": 1000000, "qid": 42 }}
			 { "task": "/motor_act", "focusscan": { "zStart": 0, "zStep": 50, "nZ": 20, "tPre": 80, "tTrig": 20, "tPost": 0, "led": 0, "illumination": [0, 255, 0, 0], "speed": 20000, "acceleration": 1000000, "qid": 42 }}
			*/
			FocusScanningData *fsd = focus_scan_get_data();

			fsd->stopped = cjson_tool_get_int(focusscan, "stopped", 0) != 0;
			if (fsd->stopped)          { log_i("focusscan stopped"); return; }
			if (focus_scan_is_running)  { log_i("focusscan already running"); return; }

			fsd->zStart = cjson_tool_get_int(focusscan, "zStart", 0);
			fsd->zStep  = cjson_tool_get_int(focusscan, "zStep", 0);
			fsd->nZ     = cjson_tool_get_int(focusscan, "nZ", 0);

			fsd->delayTimePreTrigger  = cjson_tool_get_int(focusscan, "tPre", 0);
			fsd->delayTimeTrigger     = cjson_tool_get_int(focusscan, "tTrig", 0);
			fsd->delayTimePostTrigger = cjson_tool_get_int(focusscan, "tPost", 0);

			fsd->speed        = (cjson_tool_get_int(focusscan, "speed", 0) > 20000) ? cjson_tool_get_int(focusscan, "speed", 0) : 20000;
			fsd->acceleration = (cjson_tool_get_int(focusscan, "acceleration", 0) > 1000000) ? cjson_tool_get_int(focusscan, "acceleration", 0) : 1000000;
			fsd->qid          = (uint32_t)cjson_tool_get_int(focusscan, "qid", 0);

			cJSON *illum = cJSON_GetObjectItem(focusscan, "illumination");
			for (int i = 0; i < 4; ++i)
			{
				fsd->lightsourceIntensities[i] = 0;
				if (illum)
				{
					cJSON *item = cJSON_GetArrayItem(illum, i);
					if (item) fsd->lightsourceIntensities[i] = (uint8_t)item->valueint;
				}
			}
			fsd->ledarrayIntensity = (uint8_t)cjson_tool_get_int(focusscan, "led", 0);
			fsd->stopped = 0;

			log_i("FocusScan zStart:%d zStep:%d nZ:%d tPre:%d tTrig:%d tPost:%d speed:%d accel:%d led:%d",
				fsd->zStart, fsd->zStep, (int)fsd->nZ,
				(int)fsd->delayTimePreTrigger, (int)fsd->delayTimeTrigger, (int)fsd->delayTimePostTrigger,
				fsd->speed, fsd->acceleration, (int)fsd->ledarrayIntensity);

			xTaskCreate(focus_scan_thread,
						"focusScan",
						pinConfig.STAGESCAN_TASK_STACKSIZE,
						NULL,
						0,
						NULL);
		}

	}

#endif

	void motorjson_parse_enable_motor(cJSON *doc)
	{
		cJSON *isen = cJSON_GetObjectItemCaseSensitive(doc, key_isen);
		if (isen != NULL)
		{
			focusmotor_set_enable(isen->valueint);
		}
	}

	void motorjson_parse_motor_pin_direction(cJSON *doc)
	{
		// {"task": "/motor_act", "setdir": {"steppers": [{"stepperid": 0, "dir": 1}]}}
		cJSON *setdir = cJSON_GetObjectItem(doc, key_setdir);
		if (setdir != NULL)
		{
			cJSON *stprs = cJSON_GetObjectItem(setdir, key_steppers);
			if (stprs != NULL)
			{
				cJSON *stp = NULL;
				cJSON_ArrayForEach(stp, stprs)
				{
					Stepper s = (Stepper)cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint;
					int dir = cJSON_GetObjectItemCaseSensitive(stp, key_setdir)->valueint;
					log_i("Setting motor %i direction to %i", s, dir);
					focusmotor_get_data()[s]->directionPinInverted = dir;

					// TODO: Store this permanently in pref for each motor axis
					nvs_handle_t h = 0;
					if (nvs_open("UC2", NVS_READWRITE, &h) == ESP_OK)
					{
						switch (s)
						{
						case A:
							nvs_set_u8(h, "motainv", (uint8_t)dir);
							break;
						case X:
							nvs_set_u8(h, "motxinv", (uint8_t)dir);
							break;
						case Y:
							nvs_set_u8(h, "motyinv", (uint8_t)dir);
							break;
						case Z:
							nvs_set_u8(h, "motzinv", (uint8_t)dir);
							break;
						default:
							log_w("Unknown motor axis %i, not storing direction in preferences", s);
							break;
						}
						nvs_commit(h);
						nvs_close(h);
					}
					log_i("Stored motor %i direction %i in preferences", s, dir);
				}
			}
		}
	}

	void motorjson_parse_auto_enable_motor(cJSON *doc)
	{
		cJSON *autoen = cJSON_GetObjectItemCaseSensitive(doc, key_isenauto);
#ifdef USE_FASTACCEL
		if (autoen != NULL)
		{
			focusmotor_set_auto_enable(autoen->valueint);
		}
#endif
	}

	bool motorjson_parse_set_position(cJSON *doc)
	{
		// set position
		cJSON *setpos = cJSON_GetObjectItem(doc, key_setposition);
		// {"task": "/motor_act", "setpos": {"steppers": [{"stepperid": 0, "posval": 100}, {"stepperid": 1, "posval": 0}, {"stepperid": 2, "posval": 0}, {"stepperid": 3, "posval": 0}]}}

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
					Stepper s = (Stepper)cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint;
					focusmotor_set_position(s, cJSON_GetObjectItemCaseSensitive(stp, key_currentpos)->valueint);
					log_i("Setting motor position to %i", cJSON_GetObjectItemCaseSensitive(stp, key_currentpos)->valueint);
				}
			}
			return true; // motor will return pos per socket or serial inside loop. act is fire and forget
		}
		return false;
	}

	void motorjson_parse_set_joystick_direction(cJSON *doc)
	{
		/*
		{"task": "/motor_act", "joystickdir": {"steppers": [{"stepperid": 1, "inverted": 1}]}}
		*/
		cJSON *joyDirObj = cJSON_GetObjectItemCaseSensitive(doc, "joystickdir");
		if (!joyDirObj)
		{
			return;
		}
		cJSON *stprs = cJSON_GetObjectItemCaseSensitive(joyDirObj, key_steppers);
		if (!stprs)
		{
			return;
		}
		cJSON *stp = NULL;
		cJSON_ArrayForEach(stp, stprs)
		{
			cJSON *idItem = cJSON_GetObjectItemCaseSensitive(stp, key_stepperid);
			cJSON *invertedItem = cJSON_GetObjectItemCaseSensitive(stp, "inverted");

			if (!cJSON_IsNumber(idItem) || !cJSON_IsNumber(invertedItem))
			{
				continue;
			}
			int axis = idItem->valueint;
			bool inverted = invertedItem->valueint;

			// Store in preferences
			char key[16];
			snprintf(key, sizeof key, "joyDir%d", axis);
			nvs_handle_t h = 0;
			if (nvs_open("UC2", NVS_READWRITE, &h) == ESP_OK)
			{
				nvs_set_u8(h, key, inverted ? 1 : 0);
				nvs_commit(h);
				nvs_close(h);
			}
			log_i("Set joystick direction: stepperid %i, inverted %i", axis, inverted);

			focusmotor_get_data()[axis]->joystickDirectionInverted = inverted;
		}
	}

	void motorjson_parse_set_joystick_speed(cJSON *doc)
	{
		/*
		Per-axis joystick speed-scaling multiplier — same "speedmult"/"multiplier"
		interface as JoystickRouter::act() on the USB-host bridge node, so the
		serial API is consistent regardless of which node applies the scaling.
		{"task": "/motor_act", "speedmult": {"steppers": [{"stepperid": 1, "multiplier": 75}]}}
		*/
		log_i("Parsing joystick speed multiplier");
		char *dump = cJSON_PrintUnformatted(doc);
		if (dump) { log_i("JSON: %s", dump); free(dump); }
		cJSON *speedMultObj = cJSON_GetObjectItemCaseSensitive(doc, "speedmult");
		if (!speedMultObj)
		{
			log_w("No 'speedmult' object found in JSON, skipping joystick speed multiplier parsing");
			return;
		}
		cJSON *stprs = cJSON_GetObjectItemCaseSensitive(speedMultObj, key_steppers);
		if (!stprs)
		{
			log_w("No 'steppers' array found in 'speedmult' object, skipping joystick speed multiplier parsing");
			return;
		}
		cJSON *stp = NULL;
		cJSON_ArrayForEach(stp, stprs)
		{
			cJSON *idItem = cJSON_GetObjectItemCaseSensitive(stp, key_stepperid);
			cJSON *multItem = cJSON_GetObjectItemCaseSensitive(stp, "multiplier");

			if (!cJSON_IsNumber(idItem) || !cJSON_IsNumber(multItem))
			{
				continue;
			}
			int axis = idItem->valueint;
			float multiplier = (float)multItem->valuedouble;

			// Store in preferences
			char key[16];
			snprintf(key, sizeof key, "joySpd%d", axis);
			nvs_handle_t h = 0;
			if (nvs_open("UC2", NVS_READWRITE, &h) == ESP_OK)
			{
				nvs_set_f32(h, key, multiplier);
				nvs_commit(h);
				nvs_close(h);
			}
			log_i("Set joystick speed multiplier: stepperid %i, multiplier %.2f", axis, multiplier);

			focusmotor_get_data()[axis]->joystickSpeedMultiplier = multiplier;
		}
	}

	void motorjson_parse_set_hard_limits(cJSON *doc)
	{
		/*
		Configure hard limits (emergency stop on endstop hit during normal operation, not during homing)
		{"task": "/motor_act", "hardlimits": {"steppers": [{"stepperid": 1, "enabled": 1, "polarity": 0}]}}

		Parameters:
		- enabled: 1 = hard limit protection enabled (default), 0 = disabled
		- polarity: 0 = normally open (NO, endstop is LOW when not pressed),
		            1 = normally closed (NC, endstop is HIGH when not pressed)

		When hard limit is triggered:
		- Motor stops immediately
		- Position is set to 999999 (error state indicator)
		- User must perform homing to clear the error state

		Clear hard limit triggered flag:
		{"task": "/motor_act", "hardlimits": {"steppers": [{"stepperid": 1, "clear": 1}]}}
		*/

		cJSON *hardObj = cJSON_GetObjectItemCaseSensitive(doc, "hardlimits");
		if (!hardObj)
		{
			return; // no "hardlimits" key => nothing to do
		}
		cJSON *stprs = cJSON_GetObjectItemCaseSensitive(hardObj, key_steppers);
		if (!stprs)
		{
			return; // no "steppers" array => nothing to do
		}
		cJSON *stp = NULL;
		cJSON_ArrayForEach(stp, stprs)
		{
			cJSON *idItem = cJSON_GetObjectItemCaseSensitive(stp, key_stepperid);
			if (!cJSON_IsNumber(idItem))
			{
				continue; // skip invalid
			}
			int axis = idItem->valueint;
			log_i("Processing hardlimits for stepperid %i", axis);

			// Check for clear flag (to clear the hardLimitTriggered state after homing)
			cJSON *clearItem = cJSON_GetObjectItemCaseSensitive(stp, "clear");
			if (clearItem && cJSON_IsNumber(clearItem) && clearItem->valueint)
			{
				focusmotor_clear_hard_limit_triggered(axis);
				log_i("Cleared hard limit triggered flag for axis %i", axis);
				continue;
			}

			// Parse enabled and polarity settings
			cJSON *enabledItem = cJSON_GetObjectItemCaseSensitive(stp, "enabled");
			cJSON *polarityItem = cJSON_GetObjectItemCaseSensitive(stp, "polarity");

			// Default values: enabled = true, polarity = 0 (normally open)
			bool enabled = enabledItem ? enabledItem->valueint : false;
			bool polarity = polarityItem ? polarityItem->valueint : false;

			log_i("Set hardlimits: stepperid %i, enabled %i, polarity %i (0=NO, 1=NC)", axis, enabled, polarity);

			// Apply hard limits - this also saves to preferences and notifies CAN slaves
			focusmotor_set_hard_limit(axis, enabled, polarity);
		}
	}

	void motorjson_parse_motor_drive_json(cJSON *doc)
	{
		/*
		We receive a JSON string e.g. in the form:
		{"task": "/motor_act", "motor": {"steppers": [{"stepperid": 1, "position": 10000, "speed": 20000, "isabs": 0, "isaccel": 1, "accel":100000, "isen": true}]}, "qid": 5}
		And assign it to the different motors by sending the converted MotorData to the corresponding motor driver via I2C
		// for single value
		// {"task": "/motor_act", "motor": {"steppers": [{"stepperid": 1, "speed": 10000, "redu":2}]}, "qid": 5}
		// {"task": "/motor_act", "motor": {"steppers": [{"stepperid": 1, "position": 10000, "redu":2}]}, "qid": 5}
		// precise motion with encoder feedback

		{"task": "/motor_act", "motor": {"steppers": [{"stepperid": 1, "position": 5000, "speed":30000, "isabs": 1, "precise": 1, "kpFar": 80.0, "kpNear": 100.0, "ki": 5.0, "kd": 8.0}]}, "qid": 6}
		{"task": "/motor_act", "motor": {"steppers": [{"stepperid": 1, "position": 000, "speed":25000, "isabs": 1, "precise": 1, "kpFar": 80.0, "kpNear": 100.0, "ki": 5.0, "kd": 8.0}]}, "qid": 6}
		1558 == 10000 steps == 3,125mm => 0,002005776637 mm per count => 2µm per count => fits for 2µm precision as noted in datasheet of AS5311
		{"task":"/motor_get"}
		TODO:
		- 2nd Return of stepper when doing precise
		- check if works with can
		- ratio steps vs encoder counts
		- Homing works with same interface?
		- {"task":"/motor_get"} should return also the encoder position if available = > should output both actually position and posprecise
		- if motor stalls it will count more steps than encoder counts => need to resync at some point?
		- either precise OR normal motion per motor controller, not both?
		*/
		cJSON *mot = cJSON_GetObjectItemCaseSensitive(doc, key_motor);
		if (mot != NULL)
		{
			cJSON *stprs = cJSON_GetObjectItemCaseSensitive(mot, key_steppers);
			cJSON *stp = NULL;

			// Register QID with number of steppers for completion tracking
			int motorQid = cjson_tool_get_int(doc, "qid", 0);
			if (motorQid > 0 && stprs != NULL)
			{
				int numSteppers = cJSON_GetArraySize(stprs);
				qid_register(motorQid, numSteppers);
			}

			// TODO: Check if motor is in available motors and skip if not
			// e.g. if we have only 4 motors (A, X, Y, Z) and get a command for motor E, we should skip it
			// This is especially important for CAN bus, where we might have multiple motor controllers with
			if (stprs != NULL)
			{
				cJSON_ArrayForEach(stp, stprs)
				{
					log_i("start stepper from parseMotorDriveJson");
					Stepper s = (Stepper)cJSON_GetNumberValue(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid));

					focusmotor_get_data()[s]->qid = cjson_tool_get_int(doc, "qid", 0);
					focusmotor_get_data()[s]->speed = cjson_tool_get_int(stp, key_speed, 0);
					focusmotor_get_data()[s]->isEnable = cjson_tool_get_int(stp, key_isen, 0);
					focusmotor_get_data()[s]->targetPosition = cjson_tool_get_int(stp, key_position, 0);
					focusmotor_get_data()[s]->isforever = cjson_tool_get_int(stp, key_isforever, 0);
					focusmotor_get_data()[s]->absolutePosition = cjson_tool_get_int(stp, key_isabs, 0);
					focusmotor_get_data()[s]->acceleration = cjson_tool_get_int(stp, key_acceleration, 0);
					focusmotor_get_data()[s]->isaccelerated = cjson_tool_get_int(stp, key_isaccel, 0);
					// focusmotor_get_data()[s]->isStop = cjson_tool_get_int(stp, key_isstop);
					int isReduced = cjson_tool_get_int(stp, key_isReduced, 0);

					cJSON *cstop = cJSON_GetObjectItemCaseSensitive(stp, key_isstop);
					bool isStop = (cstop != NULL) ? cstop->valueint : false;
					focusmotor_clear_hard_limit_triggered(s);// TODO: Is this a good spot to clear the hard limit flag here? Otherwise the motor won't recognize the stop event
					focusmotor_toggle_stepper(s, isStop, isReduced); // not reduced

				}
			}
			else
				log_i("Motor steppers json is null");
		}
		else
			log_i("Motor json is null");
	}

	int motorjson_act(cJSON *doc)
	{
		log_i("motor act");
		int qid = cjson_tool_get_int(doc, "qid", 0);

		// only enable/disable motors
		// {"task":"/motor_act", "isen":1, "isenauto":1}
		motorjson_parse_enable_motor(doc);

		// only set motors to autoenable
		// {"task":"/motor_act", "isen":1, "isenauto":1, "qid":1}
		motorjson_parse_auto_enable_motor(doc);

		// set position of motors in eeprom
		// {"task": "/motor_act", "setpos": {"steppers": [{"stepperid": 1, "posval": 1000}]}, "qid": 37}
		motorjson_parse_set_position(doc);

		// set motor pin direction (either 0 or 1)
		// {"task": "/motor_act", "setdir": {"steppers": [{"stepperid": 0, "dir": 1}]}, "qid": 37}
		motorjson_parse_motor_pin_direction(doc);

		// set hard limits (emergency stop on endstop hit during normal operation)
		// {"task": "/motor_act", "hardlimits": {"steppers": [{"stepperid": 1, "enabled": 1, "polarity": 0}]}}
		// Clear hard limit triggered flag: {"task": "/motor_act", "hardlimits": {"steppers": [{"stepperid": 1, "clear": 1}]}}
		motorjson_parse_set_hard_limits(doc);

		// set joystick direction inversion
		// {"task": "/motor_act", "joystickdir": {"steppers": [{"stepperid": 1, "inverted": 1}]}}
		motorjson_parse_set_joystick_direction(doc);

		// set joystick speed-scaling multiplier
		// {"task": "/motor_act", "speedmult": {"steppers": [{"stepperid": 1, "multiplier": 75}]}}
		motorjson_parse_set_joystick_speed(doc);

		// move motor drive
		// {"task": "/motor_act", "motor": {"steppers": [{"stepperid": 1, "position": 0, "speed": 20000, "isabs": 1, "isaccel": 1, "accel":20000, "isen": true}]}, "qid": 5}
		// {"task": "/motor_act", "motor": {"steppers": [{"stepperid": 1, "position": -500, "speed": 20000, "accel":120000, "isabs": 0, "isen": true, "precise": 1, "kpFar": 80.0}]}, "qid": 5}
		motorjson_parse_motor_drive_json(doc);

#ifdef STAGE_SCAN
		motorjson_parse_stage_scan(doc);
#endif
		return qid;
	}

// Helper function to add JSON parameter if present
void motorjson_add_json_float_if_present(cJSON *source, cJSON *target, const char *key)
{
	cJSON *param = cJSON_GetObjectItemCaseSensitive(source, key);
	if (param != NULL)
	{
		cJSON_AddNumberToObject(target, key, param->valuedouble);
	}
}

void motorjson_add_json_int_if_present(cJSON *source, cJSON *target, const char *key)
{
	cJSON *param = cJSON_GetObjectItemCaseSensitive(source, key);
	if (param != NULL)
	{
		cJSON_AddNumberToObject(target, key, param->valueint);
	}
}
