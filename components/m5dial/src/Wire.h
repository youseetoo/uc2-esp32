#pragma once
// T5.4: minimal Wire.h shim for the M5Dial vendored MFRC522 leaf (MFRC522.{h,cpp}
// #include <Wire.h>).
//
// The real `Wire` global is ALREADY declared by uc2_hal (uc2_wire.h:
// `extern uc2::Wire_ Wire;`). This shim must therefore NOT declare its own `Wire`
// — a second `inline TwoWire Wire;` produced "conflicting declaration 'TwoWire
// Wire'" (two differently-typed global `Wire` symbols). It only aliases the
// `TwoWire` type the MFRC522 leaf references to uc2's Wire, so the leaf's
// `TwoWire&` members / constructor defaults compile against the existing global.
// The NFC / RFID path is never used on the dial, so any Wire.* call it makes is a
// harmless no-op at runtime. See the sibling Arduino.h shim for why this lives in
// components/m5dial/src/.
#include <uc2_hal.h>
#include <stdint.h>

using TwoWire = uc2::Wire_;
