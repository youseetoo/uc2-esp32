#pragma once

struct Dpad
{
  enum Direction {up =0, up_right =1 , right = 2, right_down = 3, down = 4,down_left =5, left = 6, left_up = 7,none = 15};
};

struct GamePadData
{
  uint8_t square;
  uint8_t cross;
  uint8_t circle;
  uint8_t triangle;
  uint8_t share;
  uint8_t options;
  uint8_t l1;
  uint8_t r1;
  uint8_t l2;
  uint8_t r2;
  int16_t LeftX;
  int16_t LeftY;
  int16_t RightX;
  int16_t RightY;
  Dpad::Direction dpaddirection;
};


union PS4Buttons {
  struct {
    uint8_t dpad : 4;
    uint8_t square : 1;
    uint8_t cross : 1;
    uint8_t circle : 1;
    uint8_t triangle : 1;

    uint8_t l1 : 1;
    uint8_t r1 : 1;
    uint8_t l2 : 1;
    uint8_t r2 : 1;
    uint8_t share : 1;
    uint8_t options : 1;
    uint8_t l3 : 1;
    uint8_t r3 : 1;

    uint8_t ps : 1;
    uint8_t touchpad : 1;
    uint8_t reportCounter : 6;
  } __attribute__((packed));
  uint32_t val : 24;
} __attribute__((packed));

typedef struct {
  uint8_t LeftX;
  uint8_t LeftY;
  uint8_t RightX;
  uint8_t RightY;
  PS4Buttons Buttons;
  uint8_t LT;
  uint8_t RT;
} __attribute__((packed)) DS4Data;

// ---------------------------------------------------------------------------
// DS4 extended report (classic BT HID, report id 0x11, 77 bytes, little-endian)
//
// The DS4 only sends this report after the host has GET feature report
// 0x02 (37 bytes) - see PS4TrackpadParser.h for the full protocol notes.
typedef struct {
  uint8_t PacketCounter;      // per-packet counter (increments on touch changes)
  uint32_t Finger1Data;       // touch point 1 (see PS4TrackpadParser::decodeFingerWord)
  uint32_t Finger2Data;       // touch point 2 (see PS4TrackpadParser::decodeFingerWord)
} __attribute__((packed)) TrackpadPacket;

typedef struct {
  uint8_t  dummy0;            // always 0xC0
  uint8_t  headerId;          // always 0x00 (NOT the HID report id - that is stripped)
  uint8_t  LeftX;
  uint8_t  LeftY;
  uint8_t  RightX;
  uint8_t  RightY;
  PS4Buttons Buttons;         // 3 bytes, same layout as in the 9-byte report
  uint8_t  LT;
  uint8_t  RT;
  uint16_t Timestamp;
  uint8_t  Battery;           // low nibble = level (0..8), bit 4 = charging
  int16_t  AngularVelocityX;
  int16_t  AngularVelocityY;
  int16_t  AngularVelocityZ;
  int16_t  AccelerationX;
  int16_t  AccelerationY;
  int16_t  AccelerationZ;
  uint32_t dummy1;            // always 0x00
  uint8_t  dummy2;            // always 0x00
  uint8_t  peripheral;
  uint16_t dummy3;            // always 0x00
  uint8_t  TrackpadPacketCount;
  TrackpadPacket Packet[4];   // 4 x 9 = 36 bytes
  uint16_t dummy4;
  uint32_t crc32;             // ~CRC32({0xA1,0x11} + first 73 bytes), little-endian
} __attribute__((packed)) DS4DataExt;

// DS4 report ids / sizes used over classic BT HID
#define DS4_EXT_REPORT_SIZE 77
#define DS4_EXT_REPORT_ID 0x11
#define DS4_STANDARD_REPORT_ID 0x01
#define DS4_STANDARD_REPORT_SIZE 9
// GETting this feature report switches the controller into extended mode
#define DS4_FEATURE_ENABLE_REPORT_ID 0x02
#define DS4_FEATURE_ENABLE_REPORT_SIZE 37

// compile-time layout check (must match the wire format above)
typedef char DS4DataExtSizeCheck[(sizeof(DS4DataExt) == DS4_EXT_REPORT_SIZE) ? 1 : -1];

union HyperXClutchButtons
{
  struct {
		uint8_t A: 1;
		uint8_t B : 1;
    uint8_t unknown1 : 1;
		uint8_t X : 1;
		uint8_t Y : 1;
    uint8_t unknown2 : 1;
		
		uint8_t L1 : 1;
		uint8_t R1 : 1;
    uint8_t L2 : 1;
    uint8_t R2 : 1;

		uint8_t Select : 1;
		uint8_t Start : 1;
		uint8_t Home : 1;
		uint8_t Clear : 1;
		uint8_t Turbo : 1;
	} __attribute__((packed));
} __attribute__((packed));


typedef struct {
  HyperXClutchButtons Buttons;
  uint8_t Dpad;
  uint16_t LeftX;
  uint16_t LeftY;
  uint16_t RightX;
  uint16_t RightY;
} __attribute__((packed)) HyperXClutchData;

//hyperX clutch
//00 80 0f 00 80 00 80 00 80 00 80 00 00 00 00
// left d pad y  00 80 0f ed 71 39 ff 00 80 00 80 00 00 00 00 
//left d pad x   00 80 0f bc 03 49 61 00 80 00 80 00 00 00 00 
//right d pad x  00 80 0f 00 80 00 80 69 84 2c b7 00 00 00 00 
//right d pad y  00 80 0f 00 80 00 80 bf 9b 0a 03 00 00 00 00 

//analog down    00 80 04 00 80 00 80 00 80 00 80 00 00 00 00 
//analog up      00 80 0f 00 80 00 80 00 80 00 80 00 00 00 00 
//analog left    00 80 06 00 80 00 80 00 80 00 80 00 00 00 00 
//analog right   00 80 02 00 80 00 80 00 80 00 80 00 00 00 00 

//button A       01 80 0f 00 80 00 80 00 80 00 80 00 00 00 00 
//button X       08 80 0f 00 80 00 80 00 80 00 80 00 00 00 00 
//button y       10 80 0f 00 80 00 80 00 80 00 80 00 00 00 00 
//button b       02 80 0f 00 80 00 80 00 80 00 80 00 00 00 00 
//home           00 90 0f 00 80 00 80 00 80 00 80 00 00 00 00 
//L1             40 80 0f 00 80 00 80 00 80 00 80 00 00 00 00 
//L2             00 81 0f 00 80 00 80 00 80 00 80 00 00 00 00 
//R1             80 80 0f 00 80 00 80 00 80 00 80 00 00 00 00 
//R2             00 82 0f 00 80 00 80 00 80 00 80 00 00 00 00 
//select         00 84 0f 00 80 00 80 00 80 00 80 00 00 00 00 
//start          00 88 0f 00 80 00 80 00 80 00 80 00 00 00 00 
//turbo     ?    00 00 0f 00 80 00 80 00 80 00 80 00 00 00 00 
//clear     ?    00 00 0f 00 80 00 80 00 80 00 80 00 00 00 00 
