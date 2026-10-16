#pragma once
//
//    FILE: TCA9555.h
//  AUTHOR: Rob Tillaart
// VERSION: 0.4.4
// PURPOSE: I2C TCA9555 16 channel port expander driver (vendored from
//          https://github.com/RobTillaart/TCA9555, Apache-2.0).
//     URL: https://github.com/RobTillaart/TCA9555
//
// T3.4 port to pure ESP-IDF 6.1: the only Arduino dependency of this lib is the
// TwoWire/Wire I2C API (beginTransmission/write/endTransmission/requestFrom/read)
// plus INPUT/OUTPUT/LOW/HIGH. Those are all provided by <uc2_hal.h> (uc2::Wire_
// over driver/i2c_master.h + the gpio constants in uc2_gpio.h). So this file
// swaps `Arduino.h`/`Wire.h` for `uc2_hal.h` and `TwoWire*` for `uc2::Wire_*`.
// The F() Arduino flash-string macro is a no-op here.

#include <uc2_hal.h>

#ifndef F
#define F(x) (x)
#endif

#define TCA9555_LIB_VERSION               (F("0.4.4"))

#define TCA9555_OK                        0x00
#define TCA9555_PIN_ERROR                 0x81
#define TCA9555_I2C_ERROR                 0x82
#define TCA9555_VALUE_ERROR               0x83
#define TCA9555_PORT_ERROR                0x84

#define TCA9555_INVALID_READ              -100


#if !defined(TCA9555_PIN_NAMES)
#define TCA9555_PIN_NAMES

  #define TCA_P00         0
  #define TCA_P01         1
  #define TCA_P02         2
  #define TCA_P03         3
  #define TCA_P04         4
  #define TCA_P05         5
  #define TCA_P06         6
  #define TCA_P07         7
  #define TCA_P10         8
  #define TCA_P11         9
  #define TCA_P12         10
  #define TCA_P13         11
  #define TCA_P14         12
  #define TCA_P15         13
  #define TCA_P16         14
  #define TCA_P17         15

#endif


class TCA9555
{
public:
  TCA9555(uint8_t address, uc2::Wire_ *wire = &Wire);

  //  mask has only meaning when mode == OUTPUT
  bool     begin(uint8_t = INPUT, uint16_t mask = 0x0000);
  bool     isConnected();
  uint8_t  getAddress();


  //  1 PIN INTERFACE
  //  pin    = 0..15
  //  mode  = INPUT, OUTPUT       (INPUT_PULLUP is not supported)
  //  value = LOW, HIGH
  bool     pinMode1(uint8_t pin, uint8_t mode);
  bool     write1(uint8_t pin, uint8_t value);
  uint8_t  read1(uint8_t pin);
  bool     setPolarity(uint8_t pin, uint8_t value);    //  input pins only.
  uint8_t  getPolarity(uint8_t pin);


  //  8 PIN INTERFACE
  //  port  = 0..1
  //  mask  = bit pattern
  bool     pinMode8(uint8_t port, uint8_t mask);
  bool     write8(uint8_t port, uint8_t mask);
  int      read8(uint8_t port);
  bool     setPolarity8(uint8_t port, uint8_t value);
  uint8_t  getPolarity8(uint8_t port);


  //  16 PIN INTERFACE
  //  wraps 2x 8 PIN call.
  //  opportunistic implementation of functions
  //  needs error checking in between calls
  //  mask  = bit pattern
  bool     pinMode16(uint16_t mask);
  bool     write16(uint16_t mask);
  uint16_t read16();
  bool     setPolarity16(uint16_t mask);
  uint8_t  getPolarity16();


  //  OTHER
  int      lastError();
  uint8_t  getType();


protected:
  bool     writeRegister(uint8_t reg, uint8_t value);
  uint8_t  readRegister(uint8_t reg);

  uint8_t  _address;
  uc2::Wire_ *_wire;
  uint8_t  _error;
  uint8_t  _type;
};


/////////////////////////////////////////////////////////////////////////
//
//  DERIVED CLASSES TCA9535 PCA9555 PCA9535 CAT9555
//
class TCA9535 : public TCA9555
{
public:
  TCA9535(uint8_t address, uc2::Wire_ *wire = &Wire);
};

class PCA9555 : public TCA9555
{
public:
  PCA9555(uint8_t address, uc2::Wire_ *wire = &Wire);
};

class PCA9535 : public TCA9555
{
public:
  PCA9535(uint8_t address, uc2::Wire_ *wire = &Wire);
};

class CAT9555 : public TCA9555
{
public:
  CAT9555(uint8_t address, uc2::Wire_ *wire = &Wire);
};



//  -- END OF FILE --
