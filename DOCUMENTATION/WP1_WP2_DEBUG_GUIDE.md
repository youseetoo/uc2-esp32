# UC2 Firmware v2 — WP1 & WP2 Debug Guide

This document explains how to test and debug the features introduced in
**Work Package 1** (Runtime Configuration via NVS) and **Work Package 2**
(COBS Serial Protocol + Two-Phase ACK/DONE).

---

## WP1: Runtime Configuration (NVS)

The firmware stores module-enable flags in NVS (Non-Volatile Storage) under the
namespace `uc2cfg`.  At boot, `NVSConfig::load()` populates the global
`runtimeConfig` struct.  Fields that have never been written default to the
compile-time `#ifdef` value.

### Endpoints

| Endpoint        | Direction | Purpose                                 |
| --------------- | --------- | --------------------------------------- |
| `/config_get`   | GET       | Read all current runtime-config fields  |
| `/config_set`   | SET       | Merge partial changes and persist       |
| `/config_reset` | —         | Erase NVS namespace and **reboot**      |

### Testing via Serial (legacy JSON)

Open a serial terminal (115 200 baud) and send:

```json
{"task": "/config_get"}
```

Expected response (fields vary by build):

```json
{
  "motor": true,
  "laser": false,
  "led": true,
  "wifi": true,
  "can": false,
  "can_node_id": 1,
  "can_bitrate": 500000,
  "serial_protocol": 0
  ...
}
```

To change a value (e.g. enable laser, disable motor):

```json
{"task": "/config_set", "config": {"laser": true, "motor": false}}
```

Response:

```json
{"config_saved": true}
```

The change takes effect immediately and survives reboot.

To factory-reset back to compile-time defaults:

```json
{"task": "/config_reset"}
```

The ESP will erase the `uc2cfg` NVS namespace and reboot.

### Testing via Python (UC2-REST)

```python
import serial, json, time

ser = serial.Serial("/dev/ttyUSB0", 115200, timeout=1)
time.sleep(2.5)

# Read config
ser.write(b'{"task": "/config_get"}\n')
time.sleep(0.5)
print(ser.read(ser.in_waiting).decode())

# Set config
cmd = {"task": "/config_set", "config": {"motor": False}}
ser.write((json.dumps(cmd) + "\n").encode())
time.sleep(0.5)
print(ser.read(ser.in_waiting).decode())
```

### Monitoring NVS via ESP-IDF Monitor

When watching the serial output at boot, look for:

```
I (xxx) NVSConfig: RuntimeConfig loaded from NVS
```

If the namespace does not exist yet (first boot):

```
W (xxx) NVSConfig: NVS namespace 'uc2cfg' not found, using compile-time defaults
```

### Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| `/config_get` returns `−1` | `NVSConfig::toJSON()` returned NULL | Check ESP heap (`heap_caps_get_free_size`) |
| `/config_set` returns `−1` | Missing `"config"` key in JSON | Wrap values in `{"config": {…}}` |
| Changes lost after reboot | `NVSConfig::save()` failed | Check serial log for `Failed to open NVS namespace` |
| `/config_reset` does nothing | Board crashed before erase | Run `esptool.py erase_flash` and reflash |

---

## WP2: COBS Serial Protocol + ACK/DONE

### Protocol Overview

The firmware now supports **two transport modes** on the same UART:

| Mode   | Delimiter      | Frame Format                         |
| ------ | -------------- | ------------------------------------ |
| Legacy | `++` / `--`    | `\n{"task":"…"}\n`                   |
| COBS   | `0x00`         | `0x00 | HDR | COBS(payload) | 0x00`  |

**Auto-detection**: the firmware inspects the first byte.  If it sees `0x00`, it
enters COBS mode for that frame; otherwise, it falls back to legacy
brace-counting.

Header bytes:
- `0x01` — JSON payload
- `0x02` — MsgPack payload (reserved, not yet implemented)

### Two-Phase Response Protocol

When COBS mode is active, long-running commands return **two** messages:

1. **ACK** — sent immediately after parsing:
   ```json
   {"qid": 42, "status": "ACK"}
   ```
2. **DONE** — sent when the action completes:
   ```json
   {"qid": 42, "status": "DONE", "motor": {…}}
   ```
3. **ERROR** (instead of DONE):
   ```json
   {"qid": 42, "status": "ERROR", "error": "description"}
   ```

Fire-and-forget commands (e.g. `/state_get`) return a single merged response
with `"status": "DONE"`.

Legacy mode still returns the old `++{json}--` format (no ACK/DONE).

### Testing COBS with Python

Use the example script in `UC2-REST/examples/cobs_serial_example.py`:

```bash
cd UC2-REST
python examples/cobs_serial_example.py --port /dev/ttyUSB0
```

Or test manually:

```python
import serial, json, time
from uc2rest.transport import encode_cobs_frame, FrameAccumulator, SERIAL_HDR_JSON

ser = serial.Serial("/dev/ttyUSB0", 115200, timeout=0.1)
time.sleep(2.5)

# Send a COBS-framed command
cmd = json.dumps({"task": "/state_get"}).encode()
frame = encode_cobs_frame(cmd, header=SERIAL_HDR_JSON)
ser.write(frame)

# Read the response
acc = FrameAccumulator()
deadline = time.time() + 3.0
while time.time() < deadline:
    data = ser.read(ser.in_waiting or 1)
    if data:
        for mode, obj in acc.feed(data):
            print(f"Mode: {mode}")
            print(json.dumps(obj, indent=2))
```

### Debugging with a Hex Viewer

To inspect raw bytes on the wire, use `hexdump` or `xxd`:

```bash
# Capture 5 seconds of serial data
timeout 5 cat /dev/ttyUSB0 > /tmp/serial_dump.bin
xxd /tmp/serial_dump.bin | head -40
```

Look for:
- `00 01 …` — start of a COBS JSON frame
- `00` at the end — frame delimiter
- `2b 2b` (`++`) — legacy response start
- `2d 2d` (`--`) — legacy response end

### COBS Encode/Decode Verification

You can verify encoding/decoding without hardware:

```python
from uc2rest.transport import encode_cobs_frame, decode_cobs_frame

# Encode
payload = b'{"task":"/state_get"}'
frame = encode_cobs_frame(payload)
print("Encoded frame:", frame.hex())

# Decode (strip the leading and trailing 0x00 delimiters)
inner = frame[1:-1]
header, decoded = decode_cobs_frame(inner)
print(f"Header: 0x{header:02x}")
print(f"Decoded: {decoded.decode()}")
```

### Buffer Sizes

| Buffer | Size | Allocation |
|--------|------|------------|
| `legacyBuf` | 8 192 B | Static (BSS) |
| `cobsBuf` | 4 096 B | Heap (`malloc` in `setup()`) |
| `decodeBuf` | 4 096 B | Heap |
| `encodeBuf` | 4 096 B | Heap |

If you send a JSON command larger than 4 KB in COBS mode, the frame will be
silently dropped.  Increase `COBS_BUF_SIZE` in `SerialTransport.cpp` if needed
(watch DRAM usage).

### Troubleshooting

| Symptom | Likely Cause | Fix |
|---------|-------------|-----|
| No response in COBS mode | Frame delimiters wrong | Ensure frame starts AND ends with `0x00` |
| Garbled JSON responses | Mixing modes mid-session | Reset the board; send only one mode per session |
| `malloc` returns NULL at boot | Not enough heap | Reduce `COBS_BUF_SIZE` or disable unused modules via `/config_set` |
| Python `ImportError: cobs` | Package not installed | `pip install cobs` |
| ACK received but DONE never arrives | Command handler doesn't call `sendDONE()` | Check the specific module handler in firmware |

---

## Quick Reference — Useful Serial Commands

```json
{"task": "/state_get"}
{"task": "/config_get"}
{"task": "/config_set", "config": {"motor": true, "laser": false}}
{"task": "/config_reset"}
{"task": "/motor_act", "motor": {"steppers": [{"stepperid": 1, "position": 1000, "speed": 5000}]}, "qid": 1}
```

## File Map

| File | Purpose |
|------|---------|
| `main/src/config/RuntimeConfig.h` | Struct with all runtime flags |
| `main/src/config/NVSConfig.h/.cpp` | NVS load/save/reset/toJSON/fromJSON |
| `main/src/serial/SerialTransport.h/.cpp` | COBS + legacy frame detection & encoding |
| `main/src/serial/CommandTracker.h/.cpp` | ACK/DONE/ERROR response helpers |
| `main/src/serial/COBS.h/.cpp` | Pure COBS encoder/decoder |
| `main/src/serial/SerialProcess.cpp` | Main command dispatch loop |
| `uc2rest/transport.py` | Python-side COBS framing + FrameAccumulator |
| `uc2rest/mserial.py` | Python Serial class with COBS support |
