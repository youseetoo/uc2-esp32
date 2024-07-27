#include <cstring>
#include "freertos/FreeRTOS.h"
#include "SPIRenderer.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/timer.h"
#include "esp_err.h"
#include "esp_log.h"{"task":"/state_get", "heap":1}
#include <esp32-hal-log.h>


static const char *TAG = "Renderer";
#define PIN_NUM_MISO -1
#define PIN_NUM_MOSI  GPIO_NUM_14
#define PIN_NUM_CLK GPIO_NUM_12 // 26
#define PIN_NUM_CS GPIO_NUM_13 //27
#define PIN_NUM_LDAC GPIO_NUM_27 //GPIO_NUM_33
#define PIN_NUM_LASER GPIO_NUM_26 // 
#define PIN_NUM_TRIG_PIXEL GPIO_NUM_23
#define PIN_NUM_TRIG_LINE GPIO_NUM_22
#define PIN_NUM_TRIG_FRAME GPIO_NUM_21


void set_gpio_pins(int pixelTrigVal, int lineTrigVal, int frameTrigVal) {
    uint32_t gpio_mask = (
        (1ULL << PIN_NUM_TRIG_PIXEL) |
        (1ULL << PIN_NUM_TRIG_LINE) |
        (1ULL << PIN_NUM_TRIG_FRAME)
    );

    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = gpio_mask;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);

    if (pixelTrigVal) GPIO.out_w1ts = (1ULL << PIN_NUM_TRIG_PIXEL);
    else GPIO.out_w1tc = (1ULL << PIN_NUM_TRIG_PIXEL);

    if (lineTrigVal) GPIO.out_w1ts = (1ULL << PIN_NUM_TRIG_LINE);
    else GPIO.out_w1tc = (1ULL << PIN_NUM_TRIG_LINE);

    if (frameTrigVal) GPIO.out_w1ts = (1ULL << PIN_NUM_TRIG_FRAME);
    else GPIO.out_w1tc = (1ULL << PIN_NUM_TRIG_FRAME);
}

// Function to trigger the camera
void trigger_camera(int tPixelDwelltime, int triggerPin = PIN_NUM_TRIG_PIXEL)
{
  gpio_set_level((gpio_num_t)triggerPin, 1); // Set high
  ets_delay_us(tPixelDwelltime);             // Delay for 10us (adjust based on your camera's requirements)
  gpio_set_level((gpio_num_t)triggerPin, 0); // Set low
}

// void IRAM_ATTR SPIRenderer::draw() {
void SPIRenderer::draw()
{
  // Set the initial position
  for (int iFrame = 0; iFrame <= nFrames; iFrame++)
  {
    log_d("Drawing %d\n out of %d", iFrame, nFrames);
    log_d("X_MIN %d\n, X_MAX %d\n, Y_MIN %d\n, Y_MAX %d\n, STEP %d\n", X_MIN, X_MAX, Y_MIN, Y_MAX, STEP);

    // set all trigger high at the same time
    set_gpio_pins(1,1,1); 
    for (int x = X_MIN; x <= X_MAX; x += STEP)
    {
      set_gpio_pins(1,1,1); 
      for (int y = Y_MIN; y <= Y_MAX; y += STEP)
      {
        // Perform the scanning by setting x and y positions
        set_gpio_pins(1,0,1); 
        // Convert x, y to DAC values as needed
        int dacX = 2048 + (x * 1024) / 32768;
        int dacY = 2048 + (y * 1024) / 32768;

        // SPI transaction for channel A (X-axis)
        spi_transaction_t t1 = {};
        t1.length = 16;
        t1.flags = SPI_TRANS_USE_TXDATA;
        t1.tx_data[0] = 0b11010000 | ((dacX >> 8) & 0xF);
        t1.tx_data[1] = dacX & 255;

        spi_device_polling_transmit(spi, &t1);

        // SPI transaction for channel B (Y-axis)
        spi_transaction_t t2 = {};
        t2.length = 16;
        t2.flags = SPI_TRANS_USE_TXDATA;
        t2.tx_data[0] = 0b01010000 | ((dacY >> 8) & 0xF);
        t2.tx_data[1] = dacY & 255;
        log_d("x/y %d %d and X/Y to draw %d %d\n", x, y, dacX, dacY);
        spi_device_polling_transmit(spi, &t2);

        // Load the DAC
        gpio_set_level(PIN_NUM_LDAC, 0);
        gpio_set_level(PIN_NUM_LDAC, 1);

        // Trigger the camera for each position
        int tTriggerTime = 1; // 10us trigger time
        trigger_camera(tTriggerTime,PIN_NUM_TRIG_PIXEL);
        set_gpio_pins(1,0,0); 
      }
    }
    set_gpio_pins(0,0,0); 
  }
}

SPIRenderer::SPIRenderer(int xmin, int xmax, int ymin, int ymax, int step, int tPixelDwelltime, int nFramesI)
{
  nX = (xmax - xmin) / step;
  nY = (ymax - ymin) / step;
  tPixelDwelltime = tPixelDwelltime;
  X_MIN = xmin;
  X_MAX = xmax;
  Y_MIN = ymin;
  Y_MAX = ymax;
  STEP = step;
  nFrames = nFramesI;
  printf("Setting up renderer with parameters: %d %d %d %d %d %d %d\n", xmin, xmax, ymin, ymax, step, tPixelDwelltime, nFrames);

  // setup the laser
  gpio_set_direction(PIN_NUM_LASER, GPIO_MODE_OUTPUT);
  gpio_set_direction(PIN_NUM_TRIG_PIXEL, GPIO_MODE_OUTPUT);

  // setup the LDAC output
  gpio_set_direction(PIN_NUM_LDAC, GPIO_MODE_OUTPUT);

  // setup SPI output
  esp_err_t ret;
  spi_bus_config_t buscfg = {
      .mosi_io_num = PIN_NUM_MOSI,
      .miso_io_num = PIN_NUM_MISO,
      .sclk_io_num = PIN_NUM_CLK,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 0};
  spi_device_interface_config_t devcfg = {
      .command_bits = 0,
      .address_bits = 0,
      .dummy_bits = 0,
      .mode = 0,
      .clock_speed_hz = 80000000,
      .spics_io_num = PIN_NUM_CS, // CS pin
      .flags = SPI_DEVICE_NO_DUMMY,
      .queue_size = 2,
  };
  // Initialize the SPI bus
  ret = spi_bus_initialize(HSPI_HOST, &buscfg, 1);
  assert(ret == ESP_OK);
  // Attach the SPI device
  ret = spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
  printf("Error message %s\n", esp_err_to_name(ret));
  // printf("Ret code is %d\n", ret);
  // assert(ret == ESP_OK);
}

void SPIRenderer::setParameters(int xmin, int xmax, int ymin, int ymax, int step, int tPixelDwelltime, int nFramesI)
{
  nX = (xmax - xmin) / step;
  nY = (ymax - ymin) / step;
  tPixelDwelltime = tPixelDwelltime;
  X_MIN = xmin;
  X_MAX = xmax;
  Y_MIN = ymin;
  Y_MAX = ymax;
  STEP = step;
  nFrames = nFramesI;
  printf("Setting up renderer with parameters: %d %d %d %d %d %d %d\n", xmin, xmax, ymin, ymax, step, tPixelDwelltime, nFrames);
}

void SPIRenderer::start()
{
  // start the SPI renderer
  log_d("Starting to draw %d\n", 1);
  draw();
  log_d("Done with drawing %d", 1);
}
