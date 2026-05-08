# PR-11 OTA via CANopen SDO — Audit / REVIEW

## Existing files reviewed

| File | Location | Role |
|------|----------|------|
| `CanOtaHandler.cpp/h` | `main/src/can/` | Slave-side chunk-based OTA receiver |
| `CanOtaStreaming.cpp/h` | `main/src/can/` | Slave-side streaming (4KB page) OTA |
| `CanOtaTypes.h` | `main/src/can/` | Shared struct & enum definitions |
| `can_ota_simple.py` | `tools/ota/` | Python master: serial binary protocol |
| `can_ota_streaming.py` | `tools/ota/` | Python master: serial streaming protocol |

## Audit questions & answers

### 1. Are SDO writes going through `CO_SDOclientDownload()` or bypassing CANopenNode?

**BAD — completely bypasses CANopenNode.**

- `CanOtaHandler.cpp` uses `can_controller::sendIsoTpData()` (old ISO-TP transport)
- `CanOtaStreaming.cpp` uses `can_controller::sendCanMessage()` (raw TWAI frames)
- Custom application-layer message types (`OTA_CAN_START = 0x62`, `STREAM_START = 0x70`)
- No SDO client or server involvement whatsoever

### 2. Magic numbers or `UC2_OD::` named constants?

**BAD — magic numbers everywhere.**

- Message types from `can_messagetype.h`: `0x62`–`0x69`, `0x70`–`0x76`
- `UC2_OD::OTA_FIRMWARE_DATA` (0x2F00) constants exist in `UC2_OD_Indices.h` but are
  **never referenced** by the OTA code
- Python tools use custom sync bytes `0xAA 0x55` and custom binary framing

### 3. Does 0x2F00 entry have an `OD_extension_init()` callback?

**NO — the OD entry does not even exist.**

- `OD_RAM_t` in `OD.h` has no `x2F0*` fields
- `OD.c` has no entries for 0x2F00–0x2F05 in `ODObjs` or `ODList`
- The `UC2_OD_Indices.h` defines the constants, but nothing in the OD implements them
- No streaming write callback — the old code buffers full pages (4KB) in static RAM

### 4. Dependencies on old `can_transport.cpp` symbols?

**YES — both files depend heavily on the old transport.**

- `#include "can_transport.h"` in both `.cpp` files
- `can_controller::sendIsoTpData()` in CanOtaHandler
- `can_controller::sendCanMessage()` in CanOtaStreaming
- `can_controller::device_can_id` for CAN ID
- `#include "can_messagetype.h"` for the custom enum values

## What works (would work if old CAN transport is active)

- ESP32 OTA API usage (`Update.begin/write/end`, `esp_ota_get_next_update_partition`)
- MD5 verification of complete firmware image
- CRC32 per-chunk verification
- Page-based double-buffered flash writes in streaming mode (clever FreeRTOS design)
- Progress reporting back to master
- Abort/retry flow

## What's wrong

1. **Wrong directory** — lives in `main/src/can/` instead of `main/src/canopen/`
2. **Completely bypasses CANopenNode** — custom protocol on raw CAN/ISO-TP
3. **No OD entries** — 0x2F00–0x2F05 missing from `OD.h`/`OD.c`
4. **Incompatible with CANopen builds** — excluded by `CMakeLists.txt` when
   `CAN_CONTROLLER_CANOPEN=1`
5. **Python tools use serial** — send binary frames to ESP master over UART, not
   directly via CAN SDO (python-canopen)
6. **Magic numbers** — no use of generated `UC2_OD::` constants

## Decision: **clean rewrite**

All four diagnostic answers are negative. The existing code cannot be progressively
fixed — it speaks a different protocol entirely. The ESP32 OTA API calls can be
reused conceptually, but the transport layer must be replaced with proper CANopen SDO
domain transfers using `OD_extension_init()` callbacks.

### New file layout

```
main/src/canopen/CanOpenOTA.cpp      — slave-side OD write extension (0x2F00)
main/src/canopen/CanOpenOTA.h        — header
main/src/canopen/CanOpenOTAStreaming.cpp — master-side SDO streaming to slave
main/src/canopen/CanOpenOTAStreaming.h   — header
tools/canopen/uc2_ota_can.py         — Python CLI (python-canopen direct flash)
```

### Key design points

- 0x2F00 = DOMAIN with `OD_extension_init()` write callback → `esp_ota_write()`
- 0x2F01 = firmware size (written before data transfer starts)
- 0x2F02 = expected CRC32 (written before data, verified after transfer)
- 0x2F03 = status (TPDO-mapped for progress reporting)
- 0x2F04 = bytes received (TPDO-mapped)
- 0x2F05 = error code
- Master uses `CANopenModule::writeSDODomain()` for the firmware data transfer
- CANopenNode handles segmented/block transfer automatically
- CRC32 verified BEFORE `esp_ota_set_boot_partition()`

The old files in `main/src/can/CanOta*` are NOT deleted (that's PR-12).
