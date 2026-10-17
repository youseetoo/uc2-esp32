#pragma once
// T5.4: minimal Arduino.h shim for the M5Dial vendored MFRC522 (NFC/RFID) leaf,
// which unconditionally #includes <Arduino.h> (MFRC522.h:82). The M5Stack dial
// firmware never drives the RFID (only the PCNT encoder + M5GFX display are used),
// so this shim only satisfies the preprocessor + the few Arduino symbols MFRC522
// references: PROGMEM (the firmware reference arrays), F()/__FlashStringHelper
// (status / type-name return values), and the stdint types. The real runtime is
// uc2_hal.
//
// Lives in m5dial/src so the m5dial component and uc2_dial (DialController.cpp ->
// M5Dial.h -> MFRC522.h) resolve it. It is also reachable via the shared
// uc2_modinc include list, but that is harmless: the T5.4 change moves the M5
// includes out of DialController.h, so non-dial TUs never #include <Arduino.h> and
// this does not collide with onewire_ds18b20's own Arduino.h shim.
#include <uc2_hal.h>
#include <stdint.h>

// Stream / HardwareSerial — tmcstepper's own Arduino.h shim defines these too.
// This shim lives on the shared uc2_modinc include path, so a TMC translation unit
// (uc2_motor/TMCController.cpp -> TMCStepper.h -> #include <Arduino.h>) can resolve
// <Arduino.h> to THIS file rather than tmcstepper/include/Arduino.h. Mapping them to
// uc2_hal's UART facade keeps this a strict SUPERSET of the tmcstepper shim, so
// Stream always resolves no matter which Arduino.h the compiler picks.
typedef uc2::Serial_ Stream;
typedef uc2::Serial_ HardwareSerial;

#ifndef PROGMEM
#define PROGMEM
#endif
#ifndef PSTR
#define PSTR(x) (x)
#endif

// Arduino's flash-string marker type (the target of F()). MFRC522 only holds /
// returns F(...) pointers and the RFID path is never invoked, so an empty type
// suffices for the build.
struct __FlashStringHelper { };
#ifndef F
#define F(x) (reinterpret_cast<const __FlashStringHelper *>(PSTR(x)))
#endif
