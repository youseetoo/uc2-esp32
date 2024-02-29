#include "../../config.h"
#include "FocusMotor.h"
#include "../wifi/WifiController.h"
#include "../serial/SerialProcess.h"
#include "../state/State.h"
#include "../digitalout/DigitalOutController.h"
#include "driver/gpio.h"
#include "soc/gpio_struct.h"

void moveMotor(int stepPin, int dirPin, int steps, bool direction, int delayTimeStep)
{
	steps = abs(steps);
	FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);

	//  direction perhaps externally controlled
	if (pinConfig.I2C_SCL > -1)
	{
		motor->setExternalPin(dirPin, direction);
	}
	else
	{
		digitalWrite(dirPin, direction ? HIGH : LOW);
	}

	for (int i = 0; i < steps; ++i)
	{
		digitalWrite(stepPin, HIGH);
		ets_delay_us(delayTimeStep); // Adjust delay for speed
		digitalWrite(stepPin, LOW);
		ets_delay_us(delayTimeStep); // Adjust delay for speed
	}
}

void triggerOutput(int outputPin, int state = -1)
{
	// if state is -1 we alternate from 0,1,0
	DigitalOutController *digitalOut = (DigitalOutController *)moduleController.get(AvailableModules::digitalout);
	// Output trigger logic
	if (state == -1)
	{
		digitalOut->setPin(outputPin, 1, 0);
		ets_delay_us(1); // Adjust delay for speed
		digitalOut->setPin(outputPin, 0, 0);
	}
	else
	{
		digitalOut->setPin(outputPin, state, 0);
	}
}

// {"task": "/motor_act", "stagescan": {"nStepsLine": 100, "dStepsLine": 1, "nStepsPixel": 100, "dStepsPixel": 2, "delayTimeStep": 10, "stopped": 0, "nFrames": 50}}}
void stageScan(bool isThread = false)
{
	FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);

	// Turn on motors
	if (pinConfig.useFastAccelStepper)
	{
		motor->faccel.Enable(1);
	}
	else
	{
		motor->accel.Enable(1);
	}
	// Scanning logic
	int nStepsLine = motor->stageScanningData->nStepsLine;
	int dStepsLine = motor->stageScanningData->dStepsLine;
	int nStepsPixel = motor->stageScanningData->nStepsPixel;
	int dStepsPixel = motor->stageScanningData->dStepsPixel;
	int nTriggerPixel = motor->stageScanningData->nTriggerPixel;
	int delayTimeStep = motor->stageScanningData->delayTimeStep;
	int nFrames = motor->stageScanningData->nFrames;

	int pinDirPixel = motor->data[Stepper::X]->dirPin;
	int pinDirLine = motor->data[Stepper::Y]->dirPin;
	int pinStpPixel = motor->data[Stepper::X]->stpPin;
	int pinStpLine = motor->data[Stepper::Y]->stpPin;
	int pinTrigPixel = motor->data[Stepper::X]->triggerPin;
	int pinTrigLine = motor->data[Stepper::Y]->triggerPin;
	int pinTrigFrame = motor->data[Stepper::Z]->triggerPin;

	for (int iFrame = 0; iFrame < nFrames; iFrame++)
	{
		// frameclock

		int stepCounterPixel = 0;
		int stepCounterLine = 0;
		bool directionX = 0;
		int iPixel = 0;

		// Set pins high simultaenously at frame start
		uint32_t mask = (1 << pinConfig.DIGITAL_OUT_1) | (1 << pinConfig.DIGITAL_OUT_2) | (1 << pinConfig.DIGITAL_OUT_3);
		GPIO.out_w1ts = mask; // all high
		bool directionY = 0;
		for (int iLine = 0; iLine < nStepsLine; iLine += dStepsLine)
		{
			// for every line all high
			if (iLine > 0)
			{
				mask = (1 << pinConfig.DIGITAL_OUT_1) | (1 << pinConfig.DIGITAL_OUT_2);
				GPIO.out_w1ts = mask; // all high
			}

			for (iPixel = 0; iPixel < nStepsPixel; iPixel += dStepsPixel)
			{

				if (motor->stageScanningData->stopped)
				{
					break;
				}

				// pulses the pixel trigger (or switch off in the first round)
				mask = (1 << pinConfig.DIGITAL_OUT_1);
				GPIO.out_w1ts = mask; // all high

				// Move X motor forward at even steps, backward at odd steps
				// bool directionX = iLine % 2 == 0;
				moveMotor(pinStpPixel, pinDirPixel, dStepsPixel, directionX, delayTimeStep);
				// stepCounterPixel += (dStepsPixel * (directionX ? 1 : -1));

				// all low
				mask = (1 << pinConfig.DIGITAL_OUT_1) | (1 << pinConfig.DIGITAL_OUT_2) | (1 << pinConfig.DIGITAL_OUT_3);
				GPIO.out_w1tc = mask; // all low
			}

			// move back x stepper by step counter
			moveMotor(pinStpPixel, pinDirPixel, iPixel, !directionX, (2 + delayTimeStep) * 10);

			// Move Y motor after each line
			moveMotor(pinStpLine, pinDirLine, dStepsLine, directionY, delayTimeStep);
			stepCounterLine += (dStepsLine * (directionY ? 1 : -1));
		}

		// Reset Position and move back to origin
		/*
		motor->data[Stepper::X]->currentPosition += stepCounterPixel;
		motor->data[Stepper::Y]->currentPosition += stepCounterLine;
		moveMotor(pinStpPixel, pinDirPixel, stepCounterPixel, stepCounterLine > 0, delayTimeStep*10);
		*/
		// triggerOutput(pinTrigFrame, 0);

		moveMotor(pinStpLine, pinDirLine, nStepsLine, !directionY, (2 + delayTimeStep) * 10);
		motor->data[Stepper::X]->currentPosition -= stepCounterPixel;
		motor->data[Stepper::Y]->currentPosition -= stepCounterLine;
		ets_delay_us(10000); // Adjust delay for speed

		// print frame number as json
		Serial.println("{\"frame\": " + String(iFrame) + "}");
	}

	if (isThread)
		vTaskDelete(NULL);
}

void stageScanThread(void *arg)
//{"task": "/motor_act", "stagescan": {"nStepsLine": 100, "dStepsLine": 1, "nStepsPixel": 100, "dStepsPixel": 2, "delayTimeStep": 10, "stopped": 0, "nFrames": 50}}}
{ 	stageScan(true);
}

void sendUpdateToClients(void *p)
{
	for (;;)
	{
		int arraypos = 0;
		FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
		for (int i = 0; i < motor->data.size(); i++)
		{
			if (!motor->data[i]->stopped)
			{
				motor->sendMotorPos(i, arraypos);
			}
		}
		vTaskDelay(1000 / portTICK_PERIOD_MS);
	}
}

bool externalPinCallback(uint8_t pin, uint8_t value)
{
	FocusMotor *m = (FocusMotor *)moduleController.get(AvailableModules::motor);
	return m->setExternalPin(pin, value);
}

FocusMotor::FocusMotor() : Module() { log_i("ctor"); }
FocusMotor::~FocusMotor() { log_i("~ctor"); }

int FocusMotor::act(cJSON *doc)
{
	log_i("motor act");

	// only enable/disable motors
	// {"task":"/motor_act", "isen":1, "isenauto":1}
	cJSON *isen = cJSON_GetObjectItemCaseSensitive(doc, key_isen);
	int qid = getJsonInt(doc, "qid");

	if (isen != NULL)
	{
		if (pinConfig.useFastAccelStepper)
		{
			faccel.Enable(isen->valueint);
		}
		else
		{
			accel.Enable(isen->valueint);
		}
	}

	// only set motors to autoenable
	cJSON *autoen = cJSON_GetObjectItemCaseSensitive(doc, key_isenauto);
	if (autoen != NULL)
	{
		if (pinConfig.useFastAccelStepper)
		{
			faccel.setAutoEnable(autoen->valueint);
		}
	}

	// set position
	cJSON *setpos = cJSON_GetObjectItem(doc, key_setposition);
	// {"task": "/motor_act", "setpos": {"steppers": [{"stepperid": 0, "posval": 100}, {"stepperid": 1, "posval": 0}, {"stepperid": 2, "posval": 0}, {"stepperid": 3, "posval": 0}]}}

	if (setpos != NULL)
	{
		log_d("setpos");
		cJSON *stprs = cJSON_GetObjectItem(setpos, key_steppers);
		if (stprs != NULL)
		{
			FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
			cJSON *stp = NULL;
			cJSON_ArrayForEach(stp, stprs)
			{
				Stepper s = static_cast<Stepper>(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint);
				motor->setPosition(s, cJSON_GetObjectItemCaseSensitive(stp, key_currentpos)->valueint);
				log_i("Setting motor position to %i", cJSON_GetObjectItemCaseSensitive(stp, key_currentpos)->valueint);
			}
		}
		return qid;
	}

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
			FocusMotor *motor = (FocusMotor *)moduleController.get(AvailableModules::motor);
			cJSON *stp = NULL;
			cJSON_ArrayForEach(stp, stprs)
			{
				Stepper s = static_cast<Stepper>(cJSON_GetObjectItemCaseSensitive(stp, key_stepperid)->valueint);
				motor->data[s]->triggerPin = cJSON_GetObjectItemCaseSensitive(stp, key_triggerpin)->valueint;
				motor->data[s]->offsetTrigger = cJSON_GetObjectItemCaseSensitive(stp, key_triggeroffset)->valueint;
				motor->data[s]->triggerPeriod = cJSON_GetObjectItemCaseSensitive(stp, key_triggerperiod)->valueint;
				log_i("Setting motor trigger offset to %i", cJSON_GetObjectItemCaseSensitive(stp, key_triggeroffset)->valueint);
				log_i("Setting motor trigger period to %i", cJSON_GetObjectItemCaseSensitive(stp, key_triggerperiod)->valueint);
				log_i("Setting motor trigger pin ID to %i", cJSON_GetObjectItemCaseSensitive(stp, key_triggerpin)->valueint);
			}
		}
		return qid;
	}

	// start independent stageScan
	//
	cJSON *stagescan = cJSON_GetObjectItem(doc, "stagescan");
	if (stagescan != NULL)
	{
		stageScanningData->stopped = getJsonInt(stagescan, "stopped");
		if (stageScanningData->stopped)
		{
			log_i("stagescan stopped");
			return qid;
		}
		stageScanningData->nStepsLine = getJsonInt(stagescan, "nStepsLine");
		stageScanningData->dStepsLine = getJsonInt(stagescan, "dStepsLine");
		stageScanningData->nTriggerLine = getJsonInt(stagescan, "nTriggerLine");
		stageScanningData->nStepsPixel = getJsonInt(stagescan, "nStepsPixel");
		stageScanningData->dStepsPixel = getJsonInt(stagescan, "dStepsPixel");
		stageScanningData->nTriggerPixel = getJsonInt(stagescan, "nTriggerPixel");
		stageScanningData->delayTimeStep = getJsonInt(stagescan, "delayTimeStep");
		stageScanningData->nFrames = getJsonInt(stagescan, "nFrames");
		// xTaskCreate(stageScanThread, "stageScan", pinConfig.STAGESCAN_TASK_STACKSIZE, NULL, 0, &TaskHandle_stagescan_t);
		stageScan();
		return qid;
	}

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
				data[s]->speed = getJsonInt(stp, key_speed);
				data[s]->qid = qid;
				data[s]->isEnable = getJsonInt(stp, key_isen);
				data[s]->targetPosition = getJsonInt(stp, key_position);
				data[s]->isforever = getJsonInt(stp, key_isforever);
				data[s]->absolutePosition = getJsonInt(stp, key_isabs);
				data[s]->acceleration = getJsonInt(stp, key_acceleration);
				data[s]->isaccelerated = getJsonInt(stp, key_isaccel);
				cJSON *cstop = cJSON_GetObjectItemCaseSensitive(stp, key_isstop);
				if (cstop != NULL)
					stopStepper(s);
				else
					startStepper(s);
				log_i("start stepper (act): motor:%i isforver:%i, speed: %i, maxSpeed: %i, target pos: %i, isabsolute: %i, isacceleration: %i, acceleration: %i",
					  s,
					  data[s]->isforever,
					  data[s]->speed,
					  data[s]->maxspeed,
					  data[s]->targetPosition,
					  data[s]->absolutePosition,
					  data[s]->isaccelerated,
					  data[s]->acceleration);
			}
		}
		else
			log_i("Motor steppers json is null");
	}
	else
		log_i("Motor json is null");
	return qid;
}

void FocusMotor::startStepper(int i)
{
	if (pinConfig.useFastAccelStepper)
		faccel.startFastAccelStepper(i);
	else
		accel.startAccelStepper(i);
}

cJSON *FocusMotor::get(cJSON *docin)
{
	// get the command queue id from serial if available and add that to the return json
	int qid = getJsonInt(docin, "qid");

	log_i("get motor");
	cJSON *doc = cJSON_CreateObject();
	setJsonInt(doc, keyQueueID, qid);
	cJSON *pos = cJSON_GetObjectItemCaseSensitive(docin, key_position);
	cJSON *stop = cJSON_GetObjectItemCaseSensitive(docin, key_stopped);
	cJSON *mot = cJSON_CreateObject();
	cJSON_AddItemToObject(doc, key_motor, mot);
	cJSON *stprs = cJSON_CreateArray();
	cJSON_AddItemToObject(mot, key_steppers, stprs);

	for (int i = 0; i < data.size(); i++)
	{
		if (pos != NULL)
		{
			// update position and push it to the json
			data[i]->currentPosition = 1;
			cJSON *aritem = cJSON_CreateObject();
			setJsonInt(aritem, key_stepperid, i);
			if (pinConfig.useFastAccelStepper)
				faccel.updateData(i);
			else
				accel.updateData(i);

			setJsonInt(aritem, key_position, data[i]->currentPosition);
			cJSON_AddItemToArray(stprs, aritem);
		}
		else if (stop != NULL)
		{
			cJSON *aritem = cJSON_CreateObject();
			setJsonInt(aritem, key_stopped, !data[i]->stopped);
			cJSON_AddItemToArray(stprs, aritem);
		}
		else // return the whole config
		{
			if (pinConfig.useFastAccelStepper)
				faccel.updateData(i);
			else
				accel.updateData(i);
			cJSON *aritem = cJSON_CreateObject();
			setJsonInt(aritem, key_stepperid, i);
			setJsonInt(aritem, key_position, data[i]->currentPosition);
			setJsonInt(aritem, "isActivated", data[i]->isActivated);
			setJsonInt(aritem, key_triggeroffset, data[i]->offsetTrigger);
			setJsonInt(aritem, key_triggerperiod, data[i]->triggerPeriod);
			setJsonInt(aritem, key_triggerpin, data[i]->triggerPin);
			cJSON_AddItemToArray(stprs, aritem);
		}
	}
	return doc;
}

int FocusMotor::getExternalPinValue(uint8_t pin)
{
	// read register?
	//_tca9535->TCA9535WriteOutput(&outRegister);
	_tca9535->TCA9535ReadInput(&inRegister);

	int value = -1;
	pin = pin & ~PIN_EXTERNAL_FLAG;
	if (pin == 105) // endstop X
		value = inRegister.Port.P0.bit.Bit5;
	if (pin == 106) // endstop Y
		value = inRegister.Port.P0.bit.Bit6;
	if (pin == 107) // endstop Z
		value = inRegister.Port.P0.bit.Bit7;
	return value;
}

bool FocusMotor::setExternalPin(uint8_t pin, uint8_t value)
{
	// This example returns the previous value of the output.
	// Consequently, FastAccelStepper needs to call setExternalPin twice
	// in order to successfully change the output value.
	pin = pin & ~PIN_EXTERNAL_FLAG;
	if (pin == 100) // enable
		outRegister.Port.P0.bit.Bit0 = value;
	if (pin == 101) // x
		outRegister.Port.P0.bit.Bit1 = value;
	if (pin == 102) // y
		outRegister.Port.P0.bit.Bit2 = value;
	if (pin == 103) // z
		outRegister.Port.P0.bit.Bit3 = value;
	if (pin == 104) // a
		outRegister.Port.P0.bit.Bit4 = value;
	// log_i("external pin cb for pin:%d value:%d", pin, value);
	if (ESP_OK != _tca9535->TCA9535WriteOutput(&outRegister))
		// if (ESP_OK != _tca9535->TCA9535WriteSingleRegister(TCA9535_INPUT_REG0,outRegister.Port.P0.asInt))
		log_e("i2c write failed");

	return value;
}

void FocusMotor::dumpRegister(const char *name, TCA9535_Register configRegister)
{
	log_i("%s port0 b0:%i, b1:%i, b2:%i, b3:%i, b4:%i, b5:%i, b6:%i, b7:%i",
		  name,
		  configRegister.Port.P0.bit.Bit0,
		  configRegister.Port.P0.bit.Bit1,
		  configRegister.Port.P0.bit.Bit2,
		  configRegister.Port.P0.bit.Bit3,
		  configRegister.Port.P0.bit.Bit4,
		  configRegister.Port.P0.bit.Bit5,
		  configRegister.Port.P0.bit.Bit6,
		  configRegister.Port.P0.bit.Bit7);
	/*log_i("%s port1 b0:%i, b1:%i, b2:%i, b3:%i, b4:%i, b5:%i, b6:%i, b7:%i",
		  name,
		  configRegister.Port.P1.bit.Bit0,
		  configRegister.Port.P1.bit.Bit1,
		  configRegister.Port.P1.bit.Bit2,
		  configRegister.Port.P1.bit.Bit3,
		  configRegister.Port.P1.bit.Bit4,
		  configRegister.Port.P1.bit.Bit5,
		  configRegister.Port.P1.bit.Bit6,
		  configRegister.Port.P1.bit.Bit7);*/
}

static QueueHandle_t gpio_evt_queue = NULL;

static void IRAM_ATTR gpio_isr_handler(void *arg)
{
	uint32_t gpio_num = (uint32_t)arg;
	xQueueSendFromISR(gpio_evt_queue, &gpio_num, NULL);
}

static void gpio_task_example(void *arg)
{
	uint32_t io_num;
	for (;;)
	{
		if (xQueueReceive(gpio_evt_queue, &io_num, portMAX_DELAY))
		{
			printf("GPIO[%" PRIu32 "] intr, val: %d\n", io_num, gpio_get_level(pinConfig.I2C_INT));
		}
	}
}

void FocusMotor::init_tca()
{
	gpio_pad_select_gpio((gpio_num_t)pinConfig.I2C_INT);
	gpio_set_direction((gpio_num_t)pinConfig.I2C_INT, GPIO_MODE_INPUT_OUTPUT);
	gpio_pulldown_en((gpio_num_t)pinConfig.I2C_INT);
	gpio_pullup_dis((gpio_num_t)pinConfig.I2C_INT);
	gpio_set_intr_type((gpio_num_t)pinConfig.I2C_INT, GPIO_INTR_ANYEDGE);

	// create a queue to handle gpio event from isr
	gpio_evt_queue = xQueueCreate(10, sizeof(uint32_t));
	// start gpio task
	xTaskCreate(gpio_task_example, "gpio_task_example", 2048, NULL, 10, NULL);

	// install gpio isr service
	_tca9535 = new tca9535();

	if (ESP_OK != _tca9535->TCA9535Init(pinConfig.I2C_SCL, pinConfig.I2C_SDA, pinConfig.I2C_ADD))
		log_e("failed to init tca9535");
	else
		log_i("tca9535 init!");
	TCA9535_Register configRegister;
	_tca9535->TCA9535ReadInput(&configRegister);
	dumpRegister("Input", configRegister);
	_tca9535->TCA9535ReadOutput(&configRegister);
	dumpRegister("Output", configRegister);
	_tca9535->TCA9535ReadPolarity(&configRegister);
	dumpRegister("Polarity", configRegister);
	_tca9535->TCA9535ReadConfig(&configRegister);
	dumpRegister("Config", configRegister);

	configRegister.Port.P0.bit.Bit0 = 0; // motor enable
	configRegister.Port.P0.bit.Bit1 = 0; // x
	configRegister.Port.P0.bit.Bit2 = 0; // y
	configRegister.Port.P0.bit.Bit3 = 0; // z
	configRegister.Port.P0.bit.Bit4 = 0; // a
	configRegister.Port.P0.bit.Bit5 = 1;
	configRegister.Port.P0.bit.Bit6 = 1;
	configRegister.Port.P0.bit.Bit7 = 1;

	/*configRegister.Port.P1.bit.Bit0 = 1; // motor enable
	configRegister.Port.P1.bit.Bit1 = 1; // x
	configRegister.Port.P1.bit.Bit2 = 1; // y
	configRegister.Port.P1.bit.Bit3 = 1; // z
	configRegister.Port.P1.bit.Bit4 = 1; // a
	configRegister.Port.P1.bit.Bit5 = 0;
	configRegister.Port.P1.bit.Bit6 = 0;
	configRegister.Port.P1.bit.Bit7 = 0;*/
	if (ESP_OK != _tca9535->TCA9535WriteConfig(&configRegister))
		log_e("failed to write config to tca9535");
	else
		log_i("tca9535 config written!");

	_tca9535->TCA9535ReadConfig(&configRegister);
	dumpRegister("Config", configRegister);
}

void FocusMotor::setup()
{
	// setup the pins
	for (int i = 0; i < data.size(); i++)
	{
		data[i] = new MotorData();
	}
	stageScanningData = new StageScanningData();

	log_i("Setting Up Motor A,X,Y,Z");
	preferences.begin("motor-positions", false);
	if (pinConfig.MOTOR_A_DIR > 0)
	{
		data[Stepper::A]->dirPin = pinConfig.MOTOR_A_DIR;
		data[Stepper::A]->stpPin = pinConfig.MOTOR_A_STEP;
		data[Stepper::A]->currentPosition = preferences.getLong(("motor" + String(Stepper::A)).c_str());
		log_i("Motor A position: %i", data[Stepper::A]->currentPosition);
	}
	if (pinConfig.MOTOR_X_DIR > 0)
	{
		data[Stepper::X]->dirPin = pinConfig.MOTOR_X_DIR;
		data[Stepper::X]->stpPin = pinConfig.MOTOR_X_STEP;
		data[Stepper::X]->currentPosition = preferences.getLong(("motor" + String(Stepper::X)).c_str());
		log_i("Motor X position: %i", data[Stepper::X]->currentPosition);
	}
	if (pinConfig.MOTOR_Y_DIR > 0)
	{
		data[Stepper::Y]->dirPin = pinConfig.MOTOR_Y_DIR;
		data[Stepper::Y]->stpPin = pinConfig.MOTOR_Y_STEP;
		data[Stepper::Y]->currentPosition = preferences.getLong(("motor" + String(Stepper::Y)).c_str());
		log_i("Motor Y position: %i", data[Stepper::Y]->currentPosition);
	}
	if (pinConfig.MOTOR_Z_DIR > 0)
	{
		data[Stepper::Z]->dirPin = pinConfig.MOTOR_Z_DIR;
		data[Stepper::Z]->stpPin = pinConfig.MOTOR_Z_STEP;
		data[Stepper::Z]->currentPosition = preferences.getLong(("motor" + String(Stepper::Z)).c_str());
		log_i("Motor Z position: %i", data[Stepper::Z]->currentPosition);
	}
	preferences.end();

	// setup trigger pins
	if (pinConfig.DIGITAL_OUT_1 > 0)
		data[Stepper::X]->triggerPin = 1; // pixel^
	if (pinConfig.DIGITAL_OUT_2 > 0)
		data[Stepper::Y]->triggerPin = 2; // line^
	if (pinConfig.DIGITAL_OUT_3 > 0)
		data[Stepper::Z]->triggerPin = 3; // frame^

	if (pinConfig.useFastAccelStepper)
	{
		if (pinConfig.I2C_SCL > 0)
		{
			init_tca();
			faccel.setExternalCallForPin(externalPinCallback);
		}
		faccel.data = data;
		faccel.setupFastAccelStepper();
	}
	else
	{
		if (pinConfig.I2C_SCL > 0)
		{
			init_tca();
			accel.setExternalCallForPin(externalPinCallback);
		}
		accel.data = data;
		accel.setupAccelStepper();
	}
	// log_i("Creating Task sendUpdateToClients");
	//  xTaskCreate(sendUpdateToClients, "sendUpdateToClients", 4096, NULL, 6, NULL);
}

// dont use it, it get no longer triggered from modulehandler
void FocusMotor::loop()
{
	// checks if a stepper is still running
	for (int i = 0; i < data.size(); i++)
	{
		// check if motor is registered
		if (!data[i]->isActivated)
			continue;
		bool isRunning = true;
		if (pinConfig.useFastAccelStepper)
			isRunning = faccel.isRunning(i);
		else
			isRunning = accel.isRunning(i);

		// should only send a response if there is nothing else is sent
		State *state = (State *)moduleController.get(AvailableModules::state);
		bool isSending = state->isSending;
		/*
		// Implement an output trigger for a camera that is triggered if the stage has moved n-steps periodically
		if (isRunning and data[i]->triggerPeriod > 0)
		{
				Stepper s = static_cast<Stepper>(i);
				data[i]->currentPosition = getCurrentPosition(s);
				log_i("Current position %i", data[i]->currentPosition);
				if ((data[i]->currentPosition - data[i]->offsetTrigger) % data[i]->triggerPeriod == 0)
				{
					data[i]->isTriggered = true;
					log_i("Triggering pin %i, current Pos %i, trigger period %i", data[i]->triggerPin, data[i]->currentPosition, data[i]->triggerPeriod);
					DigitalOutController *digitalOut = (DigitalOutController *)moduleController.get(AvailableModules::digitalout);
					digitalOut->setPin(data[i]->triggerPin, data[i]->isTriggered, 0);
				}
				else
				{
					if (data[i]->isTriggered)
					{
						data[i]->isTriggered = false;
						log_i("Triggering pin %i", data[i]->triggerPin);
						DigitalOutController *digitalOut = (DigitalOutController *)moduleController.get(AvailableModules::digitalout);
						digitalOut->setPin(data[i]->triggerPin, data[i]->isTriggered, 0);
					}
				}

		}
		*/
		if (((!isRunning && !data[i]->stopped) or (!pinConfig.useFastAccelStepper and !isRunning and data[i]->wasRunning)) & !isSending)
		{
			// Only send the information when the motor is halting
			// log_d("Sending motor pos %i", i);
			stopStepper(i);
			sendMotorPos(i, 0);
			preferences.begin("motor-positions", false);
			preferences.putLong(("motor" + String(i)).c_str(), data[i]->currentPosition);
			log_i("Motor %i position: %i", i, data[i]->currentPosition);
			preferences.end();
		}
		data[i]->wasRunning = isRunning;
	}
}

bool FocusMotor::isRunning(int i)
{
	if (pinConfig.useFastAccelStepper)
	{
		return faccel.isRunning(i);
	}
	else
	{
		return accel.isRunning(i);
	}
}

void FocusMotor::sendMotorPos(int i, int arraypos)
{
	if (pinConfig.useFastAccelStepper)
		faccel.updateData(i);
	else
		accel.updateData(i);
	cJSON *root = cJSON_CreateObject();
	setJsonInt(root, keyQueueID, data[i]->qid);
	cJSON *stprs = cJSON_CreateArray();
	cJSON_AddItemToObject(root, key_steppers, stprs);
	cJSON *item = cJSON_CreateObject();
	cJSON_AddItemToArray(stprs, item);
	cJSON_AddNumberToObject(item, key_stepperid, i);
	cJSON_AddNumberToObject(item, key_position, data[i]->currentPosition);
	cJSON_AddNumberToObject(item, "isDone", data[i]->stopped);

	arraypos++;

	if (moduleController.get(AvailableModules::wifi) != nullptr)
	{
		WifiController *w = (WifiController *)moduleController.get(AvailableModules::wifi);
		w->sendJsonWebSocketMsg(root);
	}

	// print result - will that work in the case of an xTask?
	Serial.println("++");
	char *s = cJSON_Print(root);
	Serial.println(s);
	free(s);
	Serial.println("--");
	cJSON_Delete(root);
}

void FocusMotor::stopStepper(int i)
{
	if (pinConfig.useFastAccelStepper)
		faccel.stopFastAccelStepper(i);
	else
		accel.stopAccelStepper(i);
}

void FocusMotor::disableEnablePin(int i)
{
	/*
	if (data[Stepper::A]->stopped &&
		data[Stepper::X]->stopped &&
		data[Stepper::Y]->stopped &&
		data[Stepper::Z]->stopped &&
		power_enable)
	{
		pinMode(pinConfig.MOTOR_ENABLE, OUTPUT);
		digitalWrite(pinConfig.MOTOR_ENABLE, LOW ^ pinConfig.MOTOR_ENABLE_INVERTED);
		power_enable = false;
	}
	*/
}

void FocusMotor::enableEnablePin(int i)
{
	/*
	if (!power_enable)
	{
		pinMode(pinConfig.MOTOR_ENABLE, OUTPUT);
		digitalWrite(pinConfig.MOTOR_ENABLE, HIGH ^ pinConfig.MOTOR_ENABLE_INVERTED);
		power_enable = true;
	}
	*/
}

void FocusMotor::setPosition(Stepper s, int pos)
{
	if (pinConfig.useFastAccelStepper)
	{
		faccel.setPosition(s, pos);
	}
}

long FocusMotor::getCurrentPosition(Stepper s)
{
	if (pinConfig.useFastAccelStepper)
	{
		return faccel.getCurrentPosition(s);
	}
	else
	{
		return accel.getCurrentPosition(s);
	}

	return 0;
}

void FocusMotor::move(Stepper s, int steps, bool blocking)
{
	if (pinConfig.useFastAccelStepper)
	{
		faccel.move(s, steps, blocking);
	}
}
