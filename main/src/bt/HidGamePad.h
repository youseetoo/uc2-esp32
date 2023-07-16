#pragma once

typedef enum {up =0, up_right =1 , right = 2, right_down = 3, down = 4,down_left =5, left = 6, left_up = 7,none = 15} DpadDirection;

struct GamePadData
{
  uint8_t square;
  uint8_t cross;
  uint8_t circle;
  uint8_t triangle;
  uint8_t l1;
  uint8_t r1;
  uint8_t l2;
  uint8_t r2;
  uint16_t LeftX;
  uint16_t LeftY;
  uint16_t RightX;
  uint16_t RightY;
  DpadDirection dpaddirection;
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