#pragma once
// T4.5: C-ified HID gamepad data structures. The C++ nested enum `Dpad::Direction`
// is replaced by a C enum `DpadDirection` matching the same values.

#include <stdint.h>

// Former C++ `struct Dpad { enum Direction { ... } };`
typedef enum {
    DIRECTION_UP = 0,
    DIRECTION_UP_RIGHT = 1,
    DIRECTION_RIGHT = 2,
    DIRECTION_RIGHT_DOWN = 3,
    DIRECTION_DOWN = 4,
    DIRECTION_DOWN_LEFT = 5,
    DIRECTION_LEFT = 6,
    DIRECTION_LEFT_UP = 7,
    DIRECTION_NONE = 15
} DpadDirection;

typedef struct {
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
    DpadDirection dpaddirection;
} GamePadData;

// Portable packed-struct marker: GCC/Clang (ESP-IDF) accept
// __attribute__((packed)); MSVC needs #pragma pack. Only the wire-layout
// report structs below (PS4Buttons → HyperXClutchData) are packed.
#ifdef _MSC_VER
#define UC2_PACKED
#pragma pack(push, 1)
#else
#define UC2_PACKED __attribute__((packed))
#endif

// T4.5: unions must be typedef'd for C compatibility.
typedef union {
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
  } UC2_PACKED;
  uint32_t val : 24;
} UC2_PACKED PS4Buttons;

typedef struct {
  uint8_t LeftX;
  uint8_t LeftY;
  uint8_t RightX;
  uint8_t RightY;
  PS4Buttons Buttons;
  uint8_t LT;
  uint8_t RT;
} UC2_PACKED DS4Data;

typedef union {
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
	} UC2_PACKED;
} UC2_PACKED HyperXClutchButtons;

typedef struct {
  HyperXClutchButtons Buttons;
  uint8_t Dpad;
  uint16_t LeftX;
  uint16_t LeftY;
  uint16_t RightX;
  uint16_t RightY;
} UC2_PACKED HyperXClutchData;

#ifdef _MSC_VER
#pragma pack(pop)
#endif
