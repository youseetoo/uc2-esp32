#include <cstring>
#include "freertos/FreeRTOS.h"
#include "SPIRenderer.h"
#include "driver/spi_master.h"
#include "driver/gpio.h"
#include "driver/timer.h"
#include "esp_err.h"
#include <esp32-hal-log.h>
#include "freertos/task.h"
#include "esp_log.h"
#include "soc/gpio_struct.h" // Gives you GPIO.out_w1ts, etc.
#include "hal/gpio_types.h"
#include "esp_rom_sys.h" // For esp_rom_delay_us()

static const char *TAG = "Renderer";

void SPIRenderer::set_gpio_pins(int pixelTrigVal, int lineTrigVal, int frameTrigVal)
{
  // Configure GPIO pins on first call
  static bool gpio_configured = false;
  if (!gpio_configured) {
    uint32_t gpio_mask = ((1ULL << _galvo_trig_pixel) |
                          (1ULL << _galvo_trig_line) |
                          (1ULL << _galvo_trig_frame));

    gpio_config_t io_conf;
    io_conf.intr_type = GPIO_INTR_DISABLE;
    io_conf.mode = GPIO_MODE_OUTPUT;
    io_conf.pin_bit_mask = gpio_mask;
    io_conf.pull_down_en = GPIO_PULLDOWN_DISABLE;
    io_conf.pull_up_en = GPIO_PULLUP_DISABLE;
    gpio_config(&io_conf);
    gpio_configured = true;
  }

  // Set GPIO states
#ifdef ESP32S3_MODEL_XIAO
  if (pixelTrigVal)
  {
    gpio_set_level(static_cast<gpio_num_t>(_galvo_trig_pixel), 1);
  }
  else
  {
    gpio_set_level(static_cast<gpio_num_t>(_galvo_trig_pixel), 0);
  }

  if (lineTrigVal)
  {
    gpio_set_level(static_cast<gpio_num_t>(_galvo_trig_line), 1);
  }
  else
  {
    gpio_set_level(static_cast<gpio_num_t>(_galvo_trig_line), 0);
  }

  if (frameTrigVal)
  {
    gpio_set_level(static_cast<gpio_num_t>(_galvo_trig_frame), 1);
  }
  else
  {
    gpio_set_level(static_cast<gpio_num_t>(_galvo_trig_frame), 0);
  }
#else
  if (pixelTrigVal)
    GPIO.out_w1ts = (1ULL << _galvo_trig_pixel);
  else
    GPIO.out_w1tc = (1ULL << _galvo_trig_pixel);

  if (lineTrigVal)
    GPIO.out_w1ts = (1ULL << _galvo_trig_line);
  else
    GPIO.out_w1tc = (1ULL << _galvo_trig_line);

  if (frameTrigVal)
    GPIO.out_w1ts = (1ULL << _galvo_trig_frame);
  else
    GPIO.out_w1tc = (1ULL << _galvo_trig_frame);
#endif
}

void SPIRenderer::set_gpio_pins_fast(int pixelTrigVal, int lineTrigVal, int frameTrigVal)
{
#ifdef ESP32S3_MODEL_XIAO
  // For ESP32-S3 XIAO, use gpio_set_level for compatibility
  set_gpio_pins(pixelTrigVal, lineTrigVal, frameTrigVal);
#else
  // Fast GPIO operations using direct register access
  if (pixelTrigVal)
    GPIO.out_w1ts = (1ULL << _galvo_trig_pixel);
  else
    GPIO.out_w1tc = (1ULL << _galvo_trig_pixel);

  if (lineTrigVal)
    GPIO.out_w1ts = (1ULL << _galvo_trig_line);
  else
    GPIO.out_w1tc = (1ULL << _galvo_trig_line);

  if (frameTrigVal)
    GPIO.out_w1ts = (1ULL << _galvo_trig_frame);
  else
    GPIO.out_w1tc = (1ULL << _galvo_trig_frame);
#endif
}

void SPIRenderer::setFastMode(bool enabled)
{
  fastMode = enabled;
  log_i("Fast mode %s", enabled ? "enabled" : "disabled");
}

////////////////////////////////////////////////////////////////
// Optimized draw function for maximum speed
////////////////////////////////////////////////////////////////
void SPIRenderer::draw_fast()
{
  for (int iFrame = 0; iFrame < nFrames; iFrame++)
  {
    log_d("Drawing frame %d of %d (fast mode)", iFrame + 1, nFrames);
    
#ifdef ESP32S3_MODEL_XIAO
    // Use gpio_set_level for XIAO compatibility
    // Set all triggers high at frame start
    gpio_set_level(static_cast<gpio_num_t>(_galvo_trig_pixel), 1);
    gpio_set_level(static_cast<gpio_num_t>(_galvo_trig_line), 1);
    gpio_set_level(static_cast<gpio_num_t>(_galvo_trig_frame), 1);
    esp_rom_delay_us(1);
#else
    // Fast GPIO operations for frame start
    GPIO.out_w1ts = (1U << _galvo_trig_pixel) |
                    (1U << _galvo_trig_line) |
                    (1U << _galvo_trig_frame);
    esp_rom_delay_us(1);
#endif

    // Loop over X axis
    for (int dacX = X_MIN; dacX <= X_MAX; dacX += STEP)
    {
      // Loop over Y axis
      for (int dacY = Y_MIN; dacY <= Y_MAX; dacY += STEP)
      {
#ifdef ESP32S3_MODEL_XIAO
        // Clear triggers for XIAO
        gpio_set_level(static_cast<gpio_num_t>(_galvo_trig_pixel), 0);
        gpio_set_level(static_cast<gpio_num_t>(_galvo_trig_line), 0);
        gpio_set_level(static_cast<gpio_num_t>(_galvo_trig_frame), 0);
#else
        // Clear triggers with fast GPIO
        GPIO.out_w1tc = (1U << _galvo_trig_pixel) |
                        (1U << _galvo_trig_line) |
                        (1U << _galvo_trig_frame);
#endif
        esp_rom_delay_us(1);

        // Prepare SPI transactions for X and Y
        spi_transaction_t t1 = {};
        t1.length = 16;
        t1.flags = SPI_TRANS_USE_TXDATA;
        t1.tx_data[0] = (0b00110000 | ((dacX >> 8) & 0x0F)); // Channel A, Gain=1
        t1.tx_data[1] = (dacX & 0xFF);

        spi_transaction_t t2 = {};
        t2.length = 16;
        t2.flags = SPI_TRANS_USE_TXDATA;
        t2.tx_data[0] = (0b10110000 | ((dacY >> 8) & 0x0F)); // Channel B, Gain=1
        t2.tx_data[1] = (dacY & 0xFF);

        // Optimized SPI transmission with fewer LDAC toggles
#ifdef ESP32S3_MODEL_XIAO
        gpio_set_level(static_cast<gpio_num_t>(_galvo_ldac), 0); // Hold LDAC low
        spi_device_polling_transmit(spi, &t1); // Send X
        spi_device_polling_transmit(spi, &t2); // Send Y
        gpio_set_level(static_cast<gpio_num_t>(_galvo_ldac), 1); // Latch both channels
#else
        GPIO.out_w1tc = (1U << _galvo_ldac);  // Hold LDAC low
        spi_device_polling_transmit(spi, &t1); // Send X
        spi_device_polling_transmit(spi, &t2); // Send Y
        GPIO.out_w1ts = (1U << _galvo_ldac);  // Latch both channels
#endif

        // Set pixel trigger and dwell
#ifdef ESP32S3_MODEL_XIAO
        gpio_set_level(static_cast<gpio_num_t>(_galvo_trig_pixel), 1);
        esp_rom_delay_us(tPixelDwelltime);
        gpio_set_level(static_cast<gpio_num_t>(_galvo_trig_pixel), 0);
#else
        GPIO.out_w1ts = (1U << _galvo_trig_pixel);
        esp_rom_delay_us(tPixelDwelltime);
        GPIO.out_w1tc = (1U << _galvo_trig_pixel);
#endif
      }
      
      // Line end trigger
#ifdef ESP32S3_MODEL_XIAO
      gpio_set_level(static_cast<gpio_num_t>(_galvo_trig_line), 1);
      gpio_set_level(static_cast<gpio_num_t>(_galvo_trig_line), 0);
#else
      GPIO.out_w1ts = (1U << _galvo_trig_line);
      GPIO.out_w1tc = (1U << _galvo_trig_line);
#endif
    }
    
    // Frame end - clear all triggers
#ifdef ESP32S3_MODEL_XIAO
    gpio_set_level(static_cast<gpio_num_t>(_galvo_trig_pixel), 0);
    gpio_set_level(static_cast<gpio_num_t>(_galvo_trig_line), 0);
    gpio_set_level(static_cast<gpio_num_t>(_galvo_trig_frame), 0);
#else
    GPIO.out_w1tc = (1U << _galvo_trig_pixel) |
                    (1U << _galvo_trig_line) |
                    (1U << _galvo_trig_frame);
#endif
  }
}
////////////////////////////////////////////////////////////////
void SPIRenderer::trigger_camera(int tPixelDwelltime, int triggerPin)
{
#ifdef ESP32S3_MODEL_XIAO
  gpio_set_level((gpio_num_t)triggerPin, 1);
  esp_rom_delay_us(tPixelDwelltime);
  gpio_set_level((gpio_num_t)triggerPin, 0);
#else
  // On ESP32-S3, optionally do direct register toggling
  GPIO.out_w1ts.val = (1U << triggerPin); // set bit
  esp_rom_delay_us(tPixelDwelltime);
  GPIO.out_w1tc.val = (1U << triggerPin); // clear bit
#endif
}

// void IRAM_ATTR SPIRenderer::draw() {
void SPIRenderer::draw()
{
  // Set the initial position
  for (int iFrame = 0; iFrame <= nFrames; iFrame++)
  {
    
    // set all trigger high at the same time
    if (fastMode) {
      set_gpio_pins_fast(1, 1, 1);
    } else {
      set_gpio_pins(1, 1, 1);
    }
    
    // Loop over X
    for (int dacX = X_MIN; dacX <= X_MAX; dacX += STEP)
    {
      // Loop over Y
      for (int dacY = Y_MIN; dacY <= Y_MAX; dacY += STEP)
      {
        // Perform the scanning by setting x and y positions
        if (fastMode) {
          set_gpio_pins_fast(0, 0, 0);
        } else {
          set_gpio_pins(0, 0, 0);
        }
        printf("X: %d, Y: %d\n", dacX, dacY);


        // SPI transaction for channel A (X-axis)
        spi_transaction_t t1 = {};
        t1.length = 16; // 16 bits
        t1.flags = SPI_TRANS_USE_TXDATA;
        t1.tx_data[0] = (0b00110000 | ((dacX >> 8) & 0x0F)); // Bit 5 = 1 (Gain = 1)
        t1.tx_data[1] = (dacX & 0xFF);

        // SPI transaction for channel B (Y-axis)
        spi_transaction_t t2 = {};
        t2.length = 16;
        t2.flags = SPI_TRANS_USE_TXDATA;
        t2.tx_data[0] = (0b10110000 | ((dacY >> 8) & 0x0F)); // Bit 5 = 1 (Gain = 1)
        t2.tx_data[1] = (dacY & 0xFF);

        // Latch the DAC
        gpio_set_level((gpio_num_t)_galvo_ldac, 0); // Hold LDAC low
        spi_device_polling_transmit(spi, &t1);       // Send X value
        spi_device_polling_transmit(spi, &t2);       // Send Y value
        gpio_set_level((gpio_num_t)_galvo_ldac, 1); // Latch both channels
        // Latch the DAC // TODO: Necessary?
        gpio_set_level((gpio_num_t)_galvo_ldac, 0);
        gpio_set_level((gpio_num_t)_galvo_ldac, 1);

        if (fastMode) {
          set_gpio_pins_fast(1, 0, 0);
        } else {
          set_gpio_pins(1, 0, 0);
        }
      }
      // Possibly clear certain triggers
      if (fastMode) {
        set_gpio_pins_fast(1, 1, 0);
      } else {
        set_gpio_pins(1, 1, 0);
      }
    }
    // End of frame
    if (fastMode) {
      set_gpio_pins_fast(0, 0, 0);
    } else {
      set_gpio_pins(0, 0, 0);
    }
  }
}

SPIRenderer::SPIRenderer(int xmin, int xmax, int ymin, int ymax, int step, int tPixelDwelltime, int nFramesI,
                         uint8_t galvo_sdi, uint8_t galvo_miso, uint8_t galvo_sck, uint8_t galvo_cs, uint8_t galvo_ldac, uint8_t galvo_trig_pixel, uint8_t galvo_trig_line, uint8_t galvo_trig_frame)
{

  // assigning pins
  _galvo_sdi = galvo_sdi;
  _galvo_miso = galvo_miso;
  _galvo_sck = galvo_sck;
  _galvo_cs = galvo_cs;
  _galvo_trig_pixel = galvo_trig_pixel;
  _galvo_trig_line = galvo_trig_line;
  _galvo_trig_frame = galvo_trig_frame;
  _galvo_ldac = galvo_ldac;
  // pins: 7 255 8 9 6 2 3 4
  /*
       uint8_t galvo_sck = GPIO_NUM_8;
     uint8_t galvo_sdi = GPIO_NUM_7;
     uint8_t galvo_cs = GPIO_NUM_9;
     uint8_t galvo_ldac = GPIO_NUM_6;
     uint8_t galvo_laser = GPIO_NUM_43;
     uint8_t galvo_trig_pixel = GPIO_NUM_2;
     uint8_t galvo_trig_line = GPIO_NUM_3;
     uint8_t galvo_trig_frame = GPIO_NUM_4;*/
  log_i("Setting up renderer with pins: %d %d %d %d %d %d %d %d\n", galvo_sdi, galvo_miso, galvo_sck, galvo_cs, galvo_ldac, galvo_trig_pixel, galvo_trig_line, galvo_trig_frame);

  nX = (xmax - xmin) / step;
  nY = (ymax - ymin) / step;
  this->tPixelDwelltime = tPixelDwelltime;
  X_MIN = xmin;
  X_MAX = xmax;
  Y_MIN = ymin;
  Y_MAX = ymax;
  STEP = step;
  nFrames = nFramesI;
  fastMode = true; // Default to fast mode for galvo scanning
  log_i("Setting up renderer with parameters: %d %d %d %d %d %d %d\n", xmin, xmax, ymin, ymax, step, tPixelDwelltime, nFrames);

  // setup the laser
  gpio_set_direction((gpio_num_t)_galvo_trig_frame, GPIO_MODE_OUTPUT);
  gpio_set_direction((gpio_num_t)_galvo_trig_line, GPIO_MODE_OUTPUT);
  gpio_set_direction((gpio_num_t)_galvo_trig_pixel, GPIO_MODE_OUTPUT);

  // setup the LDAC output
  gpio_set_direction((gpio_num_t)_galvo_ldac, GPIO_MODE_OUTPUT);

  // setup SPI output
  esp_err_t ret;
  spi_bus_config_t buscfg = {
      .mosi_io_num = _galvo_sdi,  // galvo_mosi,
      .miso_io_num = -1, // galvo_miso,
      .sclk_io_num = _galvo_sck,  // galvo_sclk,
      .quadwp_io_num = -1,
      .quadhd_io_num = -1,
      .max_transfer_sz = 4096};
  spi_device_interface_config_t devcfg = {
      .command_bits = 0,
      .address_bits = 0,
      .dummy_bits = 0,
      .mode = 0,
      .clock_speed_hz = 20 * 1000 * 1000, // 20 MHz
      .spics_io_num = _galvo_cs, // CS pin
      .flags = SPI_DEVICE_NO_DUMMY,
      .queue_size = 2,
  };
// Initialize the SPI bus
#ifdef ESP32S3_MODEL_XIAO
  ret = spi_bus_initialize(SPI3_HOST, &buscfg, SPI_DMA_CH_AUTO);
  ESP_ERROR_CHECK(ret);

  ret = spi_bus_add_device(SPI3_HOST, &devcfg, &spi);
  ESP_ERROR_CHECK(ret);
#else
  ret = spi_bus_initialize(HSPI_HOST, &buscfg, 1);
  ESP_ERROR_CHECK(ret);

  ret = spi_bus_add_device(HSPI_HOST, &devcfg, &spi);
  ESP_ERROR_CHECK(ret);
#endif

  // If you want chip-select forced low all the time (not typically recommended),
  // you can do:
  gpio_set_level((gpio_num_t)_galvo_cs, 0);

  printf("SPI Bus init ret = %s\n", esp_err_to_name(ret));
}

void SPIRenderer::setParameters(int xmin, int xmax, int ymin, int ymax, int step, int tPixelDwelltime, int nFramesI)
{
  nX = (xmax - xmin) / step;
  nY = (ymax - ymin) / step;
  this->tPixelDwelltime = tPixelDwelltime;
  X_MIN = xmin;
  X_MAX = xmax;
  Y_MIN = ymin;
  Y_MAX = ymax;
  STEP = step;
  nFrames = nFramesI;
}

void SPIRenderer::start()
{
  // start the SPI renderer
  log_d("Starting to draw %d frames in %s mode", nFrames, fastMode ? "fast" : "normal");
  
  if (fastMode) {
    draw_fast(); // Use optimized drawing function
  } else {
    draw(); // Use original drawing function
  }
  
  log_d("Done with drawing");
}
