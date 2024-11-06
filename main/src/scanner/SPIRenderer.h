#pragma once

#include <esp_attr.h>

#include <vector>

typedef struct spi_device_t *spi_device_handle_t; ///< Handle for a device on a SPI bus

class SPIRenderer
{
private:
  spi_device_handle_t spi;
  void  draw();
  int nX;
  int nY;
  int tPixelDwelltime;
  int X_MIN = 0;
  int X_MAX = 2048;
  int Y_MIN = 0;
  int Y_MAX = 2048;
  int STEP = 64; // Adjust based on your desired resolution
  int nFrames = 1;
  

public:
  SPIRenderer(int xmin, int xmax, int ymin, int ymax, int step, int tPixelDwelltime, int nFramesI);
  void start();
  void setParameters(int xmin, int xmax, int ymin, int ymax, int step, int tPixelDwelltime, int nFramesI);

};
