#include "LedController.h"

LedController::LedController() : Module() { log_i("ctor"); }
LedController::~LedController() { log_i("~ctor"); }

void LedController::setup()
{
	log_i("LED_ARRAY_PIN: %i", pinConfig.LED_PIN);
	// LED Matrix
	matrix = new Adafruit_NeoPixel(pinConfig.LED_COUNT, pinConfig.LED_PIN, NEO_GRB + NEO_KHZ800);

	// setup Adfruit neopixel
	matrix->begin();
	matrix->setBrightness(255);

	// setup Adfruit I2C
	/*
	matrixI2C = new Adafruit_IS31FL3741();
	matrixI2C->begin();
	matrixI2C->setLEDscaling(0xFF);
	matrixI2C->setGlobalCurrent(0xFF);
	Serial.print("Global current set to: ");
	Serial.println(matrixI2C->getGlobalCurrent());
	matrixI2C->enable(true); // bring out of shutdown
	*/
	// test led array
	set_all(100, 100, 100);
	delay(1);
	set_all(0, 0, 0);

	if (!isOn)
		set_all(0, 0, 0);
	else
		set_all(255, 255, 255);
	matrix->show(); //  Update strip to match
}

void LedController::loop()
{
}

bool LedController::TurnedOn()
{
	return isOn;
}

// Custom function accessible by the API
int LedController::act(cJSON *ob)
{
	// serializeJsonPretty(ob, Serial);
	cJSON *led = cJSON_GetObjectItemCaseSensitive(ob, keyLed);
	int qid = getJsonInt(ob, "qid");
	if (led != NULL)
	{
		LedModes LEDArrMode = static_cast<LedModes>(cJSON_GetObjectItemCaseSensitive(led, keyLEDArrMode)->valueint);
		cJSON *ledarr = cJSON_GetObjectItemCaseSensitive(led, key_led_array);
		if (LEDArrMode == LedModes::array || LEDArrMode == LedModes::multi)
		{
			log_d("LED: array/multi");
			cJSON *arri = NULL;
			cJSON_ArrayForEach(arri, ledarr)
			{

				if (cJSON_HasObjectItem(arri, keyid) &&
					cJSON_HasObjectItem(arri, keyRed) &&
					cJSON_HasObjectItem(arri, keyGreen) &&
					cJSON_HasObjectItem(arri, keyBlue))
				{
					int mID = cJSON_GetObjectItemCaseSensitive(arri, keyid)->valueint;
					int mR = cJSON_GetObjectItemCaseSensitive(arri, keyRed)->valueint;
					int mG = cJSON_GetObjectItemCaseSensitive(arri, keyGreen)->valueint;
					int mB = cJSON_GetObjectItemCaseSensitive(arri, keyBlue)->valueint;
					set_led_RGB(mID, mR, mG, mB);
				}
			}
			matrix->show(); //  Update strip to match
		}
		else if (LEDArrMode == LedModes::full || LEDArrMode == LedModes::single || LEDArrMode == LedModes::left || LEDArrMode == LedModes::right || LEDArrMode == LedModes::top || LEDArrMode == LedModes::bottom)
		{
			cJSON *item = cJSON_GetArrayItem(ledarr, 0);
			u_int8_t id = cJSON_GetObjectItemCaseSensitive(item, keyid)->valueint;
			u_int8_t r = cJSON_GetObjectItemCaseSensitive(item, keyRed)->valueint;
			u_int8_t g = cJSON_GetObjectItemCaseSensitive(item, keyGreen)->valueint;
			u_int8_t b = cJSON_GetObjectItemCaseSensitive(item, keyBlue)->valueint;
			isOn = r == 0 && g == 0 && b == 0 ? false : true;
			if (LEDArrMode == LedModes::full)
				set_all(r, g, b);
			else if (LEDArrMode == LedModes::single)
				set_led_RGB(id, r, g, b);
			else if (LEDArrMode == LedModes::left)
				set_left(pinConfig.LED_COUNT, r, g, b);
			else if (LEDArrMode == LedModes::right)
				set_right(pinConfig.LED_COUNT, r, g, b);
			else if (LEDArrMode == LedModes::top)
				set_top(pinConfig.LED_COUNT, r, g, b);
			else if (LEDArrMode == LedModes::bottom)
				set_bottom(pinConfig.LED_COUNT, r, g, b);
		}
		else if (LEDArrMode == LedModes::off)
		{
			log_d("LED: all off");
			matrix->clear();
			matrix->show(); //  Update strip to match
		}
	}
	return qid;
}

// Custom function accessible by the API
cJSON *LedController::get(cJSON *ob)
{
	cJSON *j = cJSON_CreateObject();
	int qid = getJsonInt(ob, "qid");

	setJsonInt(j, keyLEDCount, pinConfig.LED_COUNT);
	setJsonInt(j, keyLEDPin, pinConfig.LED_PIN);
	setJsonInt(j, key_led_isOn, isOn);
	setJsonInt(j, keyQueueID, qid);

	cJSON *arr = cJSON_CreateArray();
	cJSON_AddItemToArray(arr, cJSON_CreateNumber(0));
	cJSON_AddItemToArray(arr, cJSON_CreateNumber(1));
	cJSON_AddItemToArray(arr, cJSON_CreateNumber(2));
	cJSON_AddItemToArray(arr, cJSON_CreateNumber(3));
	cJSON_AddItemToArray(arr, cJSON_CreateNumber(4));
	cJSON_AddItemToArray(arr, cJSON_CreateNumber(5));
	cJSON_AddItemToArray(arr, cJSON_CreateNumber(6));
	cJSON_AddItemToArray(arr, cJSON_CreateNumber(7));
	cJSON_AddItemToObject(j, keyLEDArrMode, arr);
	return j;
}

/***************************************************************************************************/
/*******************************************  LED Array  *******************************************/
/***************************************************************************************************/

void LedController::set_led_RGB(u_int8_t iLed, u_int8_t R, u_int8_t G, u_int8_t B)
{
	// log_d("setting led %i, color %i, %i, %i", iLed, R, G, B);
	matrix->setPixelColor(iLed, matrix->Color(R, G, B)); //  Set pixel's color (in RAM)
	matrix->show();

	int x = iLed % 8;
	int y = iLed / 8;

	// matrixI2C->setLEDvalue(0, iLed, matrix->Color(R, G, B));
}

void LedController::set_all(u_int8_t R, u_int8_t G, u_int8_t B)
{
	for (int i = 0; i < matrix->numPixels(); i++)
	{
		set_led_RGB(i, R, G, B);
	}
	matrix->show(); //  Update strip to match
}

void LedController::set_left(u_int8_t NLed, u_int8_t R, u_int8_t G, u_int8_t B)
{
	if (NLed == NLED4x4)
	{
		for (int i = 0; i < (NLED4x4); i++)
		{
			set_led_RGB(i, LED_PATTERN_DPC_LEFT_4x4[i] * R, LED_PATTERN_DPC_LEFT_4x4[i] * G, LED_PATTERN_DPC_LEFT_4x4[i] * B);
		}
	}
	if (NLed == NLED8x8)
	{
		for (int i = 0; i < (NLED8x8); i++)
		{
			set_led_RGB(i, LED_PATTERN_DPC_LEFT_8x8[i] * R, LED_PATTERN_DPC_LEFT_8x8[i] * G, LED_PATTERN_DPC_LEFT_8x8[i] * B);
		}
	}
	matrix->show(); //  Update strip to match
}

void LedController::set_right(u_int8_t NLed, u_int8_t R, u_int8_t G, u_int8_t B)
{
	if (NLed == NLED4x4)
	{
		for (int i = 0; i < (NLED4x4); i++)
		{
			set_led_RGB(i, (1 - LED_PATTERN_DPC_LEFT_4x4[i]) * R, (1 - LED_PATTERN_DPC_LEFT_4x4[i]) * G, (1 - LED_PATTERN_DPC_LEFT_4x4[i]) * B);
		}
	}
	if (NLed == NLED8x8)
	{
		for (int i = 0; i < (NLED8x8); i++)
		{
			set_led_RGB(i, (1 - LED_PATTERN_DPC_LEFT_8x8[i]) * R, (1 - LED_PATTERN_DPC_LEFT_8x8[i]) * G, (1 - LED_PATTERN_DPC_LEFT_8x8[i]) * B);
		}
	}
	matrix->show(); //  Update strip to match
}

void LedController::set_top(u_int8_t NLed, u_int8_t R, u_int8_t G, u_int8_t B)
{
	if (NLed == NLED4x4)
	{
		for (int i = 0; i < (NLED4x4); i++)
		{
			set_led_RGB(i, (LED_PATTERN_DPC_TOP_4x4[i]) * R, (LED_PATTERN_DPC_TOP_4x4[i]) * G, (LED_PATTERN_DPC_TOP_4x4[i]) * B);
		}
	}
	if (NLed == NLED8x8)
	{
		for (int i = 0; i < (NLED8x8); i++)
		{
			set_led_RGB(i, (LED_PATTERN_DPC_TOP_8x8[i]) * R, (LED_PATTERN_DPC_TOP_8x8[i]) * G, (LED_PATTERN_DPC_TOP_8x8[i]) * B);
		}
	}
	matrix->show(); //  Update strip to match
}

void LedController::set_bottom(u_int8_t NLed, u_int8_t R, u_int8_t G, u_int8_t B)
{
	if (NLed == NLED4x4)
	{
		for (int i = 0; i < (NLED4x4); i++)
		{
			set_led_RGB(i, (1 - LED_PATTERN_DPC_TOP_4x4[i]) * R, (1 - LED_PATTERN_DPC_TOP_4x4[i]) * G, (1 - LED_PATTERN_DPC_TOP_4x4[i]) * B);
		}
	}
	if (NLed == NLED8x8)
	{
		for (int i = 0; i < (NLED8x8); i++)
		{
			set_led_RGB(i, (1 - LED_PATTERN_DPC_TOP_8x8[i]) * R, (1 - LED_PATTERN_DPC_TOP_8x8[i]) * G, (1 - LED_PATTERN_DPC_TOP_8x8[i]) * B);
		}
	}
	matrix->show(); //  Update strip to match
}

void LedController::set_center(u_int8_t R, u_int8_t G, u_int8_t B)
{
	/*
	matrix.fillScreen(matrix.Color(0, 0, 0));
	matrix.drawPixel(4, 4, matrix.Color(R,   G,   B));
	matrix.show();
	*/
}
// LedController led;

/*

#include <Adafruit_IS31FL3741.h>

Adafruit_IS31FL3741_QT ledmatrix;
// If colors appear wrong on matrix, try invoking constructor like so:
// Adafruit_IS31FL3741_QT ledmatrix(IS3741_RBG);


// Create a new instance of the TwoWire class to represent the I2C bus
TwoWire customI2C = TwoWire(0);  // '0' is just an arbitrary number

int8_t I2C_SCL = GPIO_NUM_22;
int8_t I2C_SDA = GPIO_NUM_21;

void setup() {
  Serial.begin(115200);
  Serial.println("Adafruit QT RGB Matrix Simple RGB Swirl Test");

  // Initialize the custom I2C bus
  customI2C.begin(I2C_SDA, I2C_SCL);



   if (!ledmatrix.begin(IS3741_ADDR_DEFAULT, &customI2C)) {
	Serial.println("IS41 not found");
	while (1);
  }

  Serial.println("IS41 found!");

  // By default the LED controller communicates over I2C at 400 KHz.
  // Arduino Uno can usually do 800 KHz, and 32-bit microcontrollers 1 MHz.
  customI2C.setClock(800000);

  // Set brightness to max and bring controller out of shutdown state
  ledmatrix.setLEDscaling(0xFF);
  ledmatrix.setGlobalCurrent(0xFF);
  Serial.print("Global current set to: ");
  Serial.println(ledmatrix.getGlobalCurrent());
  ledmatrix.enable(true); // bring out of shutdown
}

uint16_t hue_offset = 0;

void drawRing(int centerX, int centerY, int radius, uint16_t color) {
  for (int y = 0; y < ledmatrix.height(); y++) {
	for (int x = 0; x < ledmatrix.width(); x++) {
	  int dx = x - centerX;
	  int dy = y - centerY;
	  // Check if the point is on the circumference of the circle
	  if (dx * dx + dy * dy >= (radius-1) * (radius-1) &&
		  dx * dx + dy * dy <= radius * radius) {
		ledmatrix.drawPixel(x, y, color);
	  }
	}
  }
}



  // Draw rings with different radii and centers
//  drawRing(int centerX, int centerY, int radius, uint16_t color)
  drawRing(5, 5, 4, ledmatrix.color565(255, 255, 255)); // Small red ring
*/