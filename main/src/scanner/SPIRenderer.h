#pragma once

#include <esp_attr.h>

#include <vector>

typedef struct spi_device_t *spi_device_handle_t; ///< Handle for a device on a SPI bus

class SPIRenderer
{
private:
  spi_device_handle_t spi;
  void  draw();
  void  draw_fast(); // Optimized drawing function for maximum speed
  int nX;
  int nY;
  int tPixelDwelltime;
  int X_MIN = 0;
  int X_MAX = 2048;
  int Y_MIN = 0;
  int Y_MAX = 2048;
  int STEP = 64; // Adjust based on your desired resolution
  int nFrames = 1;
  bool fastMode = true; // Use fast GPIO operations by default
  
  // pin assignment 
  uint8_t _galvo_sdi = 0;
  uint8_t _galvo_miso = 0;
  uint8_t _galvo_mosi = 0;
  uint8_t _galvo_sck = 0;
  uint8_t _galvo_cs = 0;
  uint8_t _galvo_trig_pixel = 0;
  uint8_t _galvo_trig_line = 0;
  uint8_t _galvo_trig_frame = 0;
  uint8_t _galvo_ldac = 0;


public:
  SPIRenderer(int xmin, int xmax, int ymin, int ymax, int step, int tPixelDwelltime, int nFramesI, 
    uint8_t galvo_sdi, uint8_t galvo_miso, uint8_t galvo_sck, uint8_t galvo_cs, 
    uint8_t galvo_ldac, uint8_t galvo_trig_pixel, uint8_t galvo_trig_line, uint8_t galvo_trig_frame);
  void start();
  void setParameters(int xmin, int xmax, int ymin, int ymax, int step, int tPixelDwelltime, int nFramesI);
  void setFastMode(bool enabled); // Enable/disable fast GPIO operations
  void trigger_camera(int tPixelDwelltime, int triggerPin);
  void set_gpio_pins(int pixel, int line, int frame);
  void set_gpio_pins_fast(int pixel, int line, int frame); // Fast GPIO version
};
