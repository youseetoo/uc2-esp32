// T4.1: pure C (was the C++ namespace DigitalOutController).
#include "PinConfigDefault.h"
#include "DigitalOutController.h"
// T1.3: direct IDF (no Arduino) — uc2 façade for gpio/time/log.
// JsonKeys.h dropped (Arduino/pgmspace): literal key strings.
#include "uc2_hal/uc2_log.h"   // log_* (C-safe; the umbrella is C++-only)
#include "uc2_hal/uc2_time.h"  // delay()
#include "uc2_hal/uc2_gpio.h"  // digitalWrite()
#include "cJsonTool.h"

static bool is_debug = false;

static int digitalout_val_1 = 0;
static int digitalout_val_2 = 0;
static int digitalout_val_3 = 0;

/// trigger settings
static long digitalout_trigger_delay_1_on = 0;
static long digitalout_trigger_delay_2_on = 0;
static long digitalout_trigger_delay_3_on = 0;
static long digitalout_trigger_delay_1_off = 0;
static long digitalout_trigger_delay_2_off = 0;
static long digitalout_trigger_delay_3_off = 0;

static bool is_digital_trigger_1 = false;
static bool is_digital_trigger_2 = false;
static bool is_digital_trigger_3 = false;

// Custom function accessible by the API
int digitalout_act(cJSON *jsonDocument)
{
	// here you can do something
	log_d("digitalout_act_fct");
	int qid = cjson_tool_get_int(jsonDocument, "qid", 0);
	int triggerdelay = 10;

	// set default parameters
	// is trigger active?
	is_digital_trigger_1 = cjson_tool_get_int(jsonDocument, "digitalout1IsTrigger", 0);
	is_digital_trigger_2 = cjson_tool_get_int(jsonDocument, "digitalout2IsTrigger", 0);
	is_digital_trigger_3 = cjson_tool_get_int(jsonDocument, "digitalout3IsTrigger", 0);

	// on-delay
	digitalout_trigger_delay_1_on = cjson_tool_get_int(jsonDocument, "digitalout1TriggerDelayOn", 0);
	digitalout_trigger_delay_2_on = cjson_tool_get_int(jsonDocument, "digitalout2TriggerDelayOn", 0);
	digitalout_trigger_delay_3_on = cjson_tool_get_int(jsonDocument, "digitalout3TriggerDelayOn", 0);

	// off-delay
	digitalout_trigger_delay_1_off = cjson_tool_get_int(jsonDocument, "digitalout1TriggerDelayOff", 0);
	digitalout_trigger_delay_2_off = cjson_tool_get_int(jsonDocument, "digitalout2TriggerDelayOff", 0);
	digitalout_trigger_delay_3_off = cjson_tool_get_int(jsonDocument, "digitalout3TriggerDelayOff", 0);
	int reset = cjson_tool_get_int(jsonDocument, "digitaloutistriggerreset", 0);

	// reset trigger
	if (reset == 1)
	{
		is_digital_trigger_1 = false;
		is_digital_trigger_2 = false;
		is_digital_trigger_3 = false;
	}

	//
	int digitaloutid = cjson_tool_get_int(jsonDocument, "digitaloutid", 0);
	int digitaloutval = cjson_tool_get_int(jsonDocument, "digitaloutval", 0);

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

	if (digitaloutid == 1)
	{
		digitalout_val_1 = digitaloutval;
		if (digitaloutval == -1)
		{
			// perform trigger
			digitalWrite((uint8_t)pinConfig.DIGITAL_OUT_1, HIGH);
			delay((uint32_t)triggerdelay);
			digitalWrite((uint8_t)pinConfig.DIGITAL_OUT_1, LOW);
		}
		else
		{
			digitalWrite((uint8_t)pinConfig.DIGITAL_OUT_1, (uint8_t)digitalout_val_1);
			log_d("DIGITAL_OUT %d", digitaloutid);
		}
	}
	else if (digitaloutid == 2)
	{
		digitalout_val_2 = digitaloutval;
		if (digitaloutval == -1)
		{
			// perform trigger
			digitalWrite((uint8_t)pinConfig.DIGITAL_OUT_2, HIGH);
			delay((uint32_t)triggerdelay);
			digitalWrite((uint8_t)pinConfig.DIGITAL_OUT_2, LOW);
		}
		else
		{
			digitalWrite((uint8_t)pinConfig.DIGITAL_OUT_2, (uint8_t)digitalout_val_2);
			log_d("DIGITAL_OUT %d", digitaloutid);
		}
	}
	else if (digitaloutid == 3)
	{
		digitalout_val_3 = digitaloutval;
		if (digitaloutval == -1)
		{
			// perform trigger
			digitalWrite((uint8_t)pinConfig.DIGITAL_OUT_3, HIGH);
			delay((uint32_t)triggerdelay);
			digitalWrite((uint8_t)pinConfig.DIGITAL_OUT_3, LOW);
		}
		else
		{
			digitalWrite((uint8_t)pinConfig.DIGITAL_OUT_3, (uint8_t)digitalout_val_3);
			log_d("DIGITAL_OUT %d", digitaloutid);
		}
	}
	return qid;
}

// Custom function accessible by the API
cJSON *digitalout_get(cJSON *jsonDocument)
{
	// GET SOME PARAMETERS HERE
	int digitaloutid = cjson_tool_get_int(jsonDocument, "digitaloutid", 0);

	int digitaloutpin = 0;
	int digitaloutval = 0;

	if (digitaloutid == 1)
	{
		if (is_debug)
			log_d("digitalout 1");
		digitaloutpin = pinConfig.DIGITAL_OUT_1;
		digitaloutval = digitalout_val_1;
	}
	else if (digitaloutid == 2)
	{
		if (is_debug)
			log_d("AXIS 2");
		if (is_debug)
			log_d("digitalout 2");
		digitaloutpin = pinConfig.DIGITAL_OUT_2;
		digitaloutval = digitalout_val_2;
	}
	else if (digitaloutid == 3)
	{
		if (is_debug)
			log_d("AXIS 3");
		if (is_debug)
			log_d("digitalout 1");
		digitaloutpin = pinConfig.DIGITAL_OUT_3;
		digitaloutval = digitalout_val_3;
	}

	jsonDocument = cJSON_CreateObject();
	cJSON *douth = cJSON_CreateObject();
	cJSON_AddItemToObject(jsonDocument, "digitalout", douth);
	cjson_tool_set_int(douth, "digitaloutid", digitaloutid);
	cjson_tool_set_int(douth, "digitaloutval", digitaloutval);
	cjson_tool_set_int(douth, "digitaloutpin", digitaloutpin);
	return jsonDocument;
}

void digitalout_setup(void)
{
	log_d("Setting Up digitalout");
	/* setup the output nodes and reset them to 0*/
	pinMode((uint8_t)pinConfig.DIGITAL_OUT_1, OUTPUT);

	digitalWrite((uint8_t)pinConfig.DIGITAL_OUT_1, HIGH);
	delay(50);
	digitalWrite((uint8_t)pinConfig.DIGITAL_OUT_1, LOW);

	pinMode((uint8_t)pinConfig.DIGITAL_OUT_2, OUTPUT);
	digitalWrite((uint8_t)pinConfig.DIGITAL_OUT_2, HIGH);
	delay(50);
	digitalWrite((uint8_t)pinConfig.DIGITAL_OUT_2, LOW);

	pinMode((uint8_t)pinConfig.DIGITAL_OUT_3, OUTPUT);
	digitalWrite((uint8_t)pinConfig.DIGITAL_OUT_3, HIGH);
	delay(50);
	digitalWrite((uint8_t)pinConfig.DIGITAL_OUT_3, LOW);
}

void digitalout_loop(void)
{
	// run trigger table based on previously set parameters
	if (is_digital_trigger_1)
	{
		digitalWrite((uint8_t)pinConfig.DIGITAL_OUT_1, HIGH);
		// log_i("DIGITAL_OUT_1 HIGH");
		delay((uint32_t)digitalout_trigger_delay_1_on);
		digitalWrite((uint8_t)pinConfig.DIGITAL_OUT_1, LOW);
		// log_i("DIGITAL_OUT_1 LOW");
		delay((uint32_t)digitalout_trigger_delay_1_off);
	}
	if (is_digital_trigger_2)
	{
		digitalWrite((uint8_t)pinConfig.DIGITAL_OUT_2, HIGH);
		// log_i("DIGITAL_OUT_2 HIGH");
		delay((uint32_t)digitalout_trigger_delay_2_on);
		digitalWrite((uint8_t)pinConfig.DIGITAL_OUT_2, LOW);
		// log_i("DIGITAL_OUT_2 LOW");
		delay((uint32_t)digitalout_trigger_delay_2_off);
	}
	if (is_digital_trigger_3)
	{
		digitalWrite((uint8_t)pinConfig.DIGITAL_OUT_3, HIGH);
		// log_i("DIGITAL_OUT_3 HIGH");
		delay((uint32_t)digitalout_trigger_delay_3_on);
		digitalWrite((uint8_t)pinConfig.DIGITAL_OUT_3, LOW);
		// log_i("DIGITAL_OUT_3 LOW");
		delay((uint32_t)digitalout_trigger_delay_3_off);
	}
}
