#include "DigitalOutController.h"

DigitalOutController::DigitalOutController(/* args */){};
DigitalOutController::~DigitalOutController(){};

// Custom function accessible by the API
int DigitalOutController::act(cJSON*  jsonDocument)
{
	// here you can do something
	log_d("digitalout_act_fct");
	isBusy = true;
	int triggerdelay = 10;

	// set default parameters
	// is trigger active?
	is_digital_trigger_1 = getJsonInt(jsonDocument, keyDigitalOut1IsTrigger);
	is_digital_trigger_2=getJsonInt(jsonDocument, keyDigitalOut2IsTrigger);
	is_digital_trigger_3 =getJsonInt(jsonDocument, keyDigitalOut3IsTrigger);
	
	// on-delay
	digitalout_trigger_delay_1_on =getJsonInt(jsonDocument, keyDigitalOut1TriggerDelayOn);
	digitalout_trigger_delay_2_on=getJsonInt(jsonDocument, keyDigitalOut2TriggerDelayOn);
	digitalout_trigger_delay_3_on =getJsonInt(jsonDocument, keyDigitalOut3TriggerDelayOn);
	
	// off-delay
	digitalout_trigger_delay_1_off = getJsonInt(jsonDocument, keyDigitalOut1TriggerDelayOff);
	digitalout_trigger_delay_2_off = getJsonInt(jsonDocument, keyDigitalOut2TriggerDelayOff);
	digitalout_trigger_delay_3_off = getJsonInt(jsonDocument, keyDigitalOut3TriggerDelayOff);
	int reset = getJsonInt(jsonDocument, keyDigitalOutIsTriggerReset);
	
	// reset trigger
	if(reset == 1){
		is_digital_trigger_1 = false;
		is_digital_trigger_2 = false;
		is_digital_trigger_3 = false;
	}

	// 
	int digitaloutid=getJsonInt(jsonDocument, keyDigitalOutid);
	int digitaloutval=getJsonInt(jsonDocument, keyDigitalOutVal);
		
	// print debugging information
	log_d("digitaloutid %d", digitaloutid);
	log_d("digitaloutval %d", digitaloutval);
	log_d("digitalout_trigger_delay_1_on %d", digitalout_trigger_delay_1_on);
	log_d("digitalout_trigger_delay_1_off %d", digitalout_trigger_delay_1_off);
	log_d("digitalout_trigger_delay_2_on %d", digitalout_trigger_delay_2_on);
	log_d("digitalout_trigger_delay_2_off %d", digitalout_trigger_delay_2_off);
	log_d("digitalout_trigger_delay_3_on %d", digitalout_trigger_delay_3_on);
	log_d("digitalout_trigger_delay_3_off %d", digitalout_trigger_delay_3_off);
	log_d("is_digital_trigger_1 %d", is_digital_trigger_1);
	log_d("is_digital_trigger_2 %d", is_digital_trigger_2);
	log_d("is_digital_trigger_3 %d", is_digital_trigger_3);

	setPin(digitaloutid, digitaloutval, triggerdelay);

	return 1;
}


void DigitalOutController::setPin(int digitaloutid, int digitaloutval, int triggerdelay){

	/*
	setting a pin off a certain value 

	digitaloutid: 1,2,3
	digitaloutval: 0, 	LOW
				   1,	HIGH
				   -1, 	trigger
	triggerdelay: delay in ms for trigger
	*/
	if (digitaloutid == 1)
	{
		if (digitaloutval == -1)
		{
			// perform trigger
			digitalWrite(pinConfig.DIGITAL_OUT_1, HIGH);
			delay(triggerdelay);
			digitalWrite(pinConfig.DIGITAL_OUT_1, LOW);
		}
		else
		{
			digitalWrite(pinConfig.DIGITAL_OUT_1, digitaloutval);
		}
	}
		else if (digitaloutid == 2)
	{
		if (digitaloutval == -1)
		{
			// perform trigger
			digitalWrite(pinConfig.DIGITAL_OUT_2, HIGH);
			delay(triggerdelay);
			digitalWrite(pinConfig.DIGITAL_OUT_2, LOW);
		}
		else
		{
			digitalWrite(pinConfig.DIGITAL_OUT_2, digitaloutval);
		}
	}
	else if (digitaloutid == 3)
	{
		if (digitaloutval == -1)
		{
			// perform trigger
			digitalWrite(pinConfig.DIGITAL_OUT_3, HIGH);
			delay(triggerdelay);
			digitalWrite(pinConfig.DIGITAL_OUT_3, LOW);
		}
		else
		{
			digitalWrite(pinConfig.DIGITAL_OUT_3, digitaloutval);
		}
	}
	else
	{
		log_d("digitaloutid not found");
	}
}

// Custom function accessible by the API
cJSON*  DigitalOutController::get(cJSON*  jsonDocument)
{
	// GET SOME PARAMETERS HERE
	int digitaloutid = getJsonInt(jsonDocument, "digitaloutid");
	
	int digitaloutpin = 0;
	int digitaloutval = 0;

	if (digitaloutid == 1)
	{
		if (DEBUG)
			log_d("digitalout 1");
		digitaloutpin = pinConfig.DIGITAL_OUT_1;
		digitaloutval = digitalout_val_1;
	}
	else if (digitaloutid == 2)
	{
		if (DEBUG)
			log_d("AXIS 2");
		if (DEBUG)
			log_d("digitalout 2");
		digitaloutpin = pinConfig.DIGITAL_OUT_2;
		digitaloutval = digitalout_val_2;
	}
	else if (digitaloutid == 3)
	{
		if (DEBUG)
			log_d("AXIS 3");
		if (DEBUG)
			log_d("digitalout 1");
		digitaloutpin = pinConfig.DIGITAL_OUT_3;
		digitaloutval = digitalout_val_3;
	}

	jsonDocument = cJSON_CreateObject();
	setJsonInt(jsonDocument,"digitaloutid",digitaloutid);
	setJsonInt(jsonDocument,"digitaloutval",digitaloutval);
	setJsonInt(jsonDocument,"digitaloutpin",digitaloutpin);
	return jsonDocument;
}

void DigitalOutController::setup()
{
	log_d("Setting Up digitalout");
	/* setup the output nodes and reset them to 0*/
	pinMode(pinConfig.DIGITAL_OUT_1, OUTPUT);
	digitalWrite(pinConfig.DIGITAL_OUT_1, HIGH);
	delay(20);
	digitalWrite(pinConfig.DIGITAL_OUT_1, LOW);

	pinMode(pinConfig.DIGITAL_OUT_2, OUTPUT);
	digitalWrite(pinConfig.DIGITAL_OUT_2, HIGH);
	delay(20);
	digitalWrite(pinConfig.DIGITAL_OUT_2, LOW);

	pinMode(pinConfig.DIGITAL_OUT_3, OUTPUT);
	digitalWrite(pinConfig.DIGITAL_OUT_3, HIGH);
	delay(20);
	digitalWrite(pinConfig.DIGITAL_OUT_3, LOW);
}

void DigitalOutController::loop(){

	// run trigger table based on previously set parameters
	if (is_digital_trigger_1){
		digitalWrite(pinConfig.DIGITAL_OUT_1, HIGH);
		//log_i("DIGITAL_OUT_1 HIGH");
		delay(digitalout_trigger_delay_1_on);
		digitalWrite(pinConfig.DIGITAL_OUT_1, LOW);
		//log_i("DIGITAL_OUT_1 LOW");
		delay(digitalout_trigger_delay_1_off);
	}
	if (is_digital_trigger_2){
		digitalWrite(pinConfig.DIGITAL_OUT_2, HIGH);
		//log_i("DIGITAL_OUT_2 HIGH");
		delay(digitalout_trigger_delay_2_on);
		digitalWrite(pinConfig.DIGITAL_OUT_2, LOW);
		//log_i("DIGITAL_OUT_2 LOW");
		delay(digitalout_trigger_delay_2_off);
	}
	if (is_digital_trigger_3){
		digitalWrite(pinConfig.DIGITAL_OUT_3, HIGH);
		//log_i("DIGITAL_OUT_3 HIGH");
		delay(digitalout_trigger_delay_3_on);
		digitalWrite(pinConfig.DIGITAL_OUT_3, LOW);
		//log_i("DIGITAL_OUT_3 LOW");
		delay(digitalout_trigger_delay_3_off);
	}

}
