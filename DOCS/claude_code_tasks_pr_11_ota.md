# CANopen OTA — review, diagnosis, and Claude Code work package

## Current state of the code

### What exists and works on the SLAVE side ✅

`CanOpenOTA.cpp` (255 lines) — **well-structured, no bugs found.** The slave:

1. Registers an `OD_extension_init` callback on `0x2F00` (OTA_FIRMWARE_DATA)
2. Registers a callback on `0x2F01` (OTA_FIRMWARE_SIZE) that triggers `esp_ota_begin()`
3. When SDO segmented/block writes arrive for `0x2F00`, each segment is streamed directly
   to `esp_ota_write()` — zero RAM buffering
4. On the final segment (`stream->dataOffset + count >= stream->dataLength`):
   - CRC32 is verified against `0x2F02`
   - `esp_ota_end()` commits the partition
   - `esp_ota_set_boot_partition()` sets boot target
   - 2-second delay, then reboot
5. Status is written to `0x2F03`, bytes received to `0x2F04`, error code to `0x2F05`
6. `registerOdExtensions()` is called at line 606 of `CANopenModule.cpp` during boot

**The OD entries are correctly defined in OD.c:**
- `0x2F00`: `dataOrig=NULL`, `dataLength=0`, `ODA_SDO_RW` — a proper DOMAIN entry
  with no RAM backing (the extension callback handles all writes)
- `0x2F01-0x2F05`: Normal VAR entries backed by `OD_RAM.x2F01_*` through `x2F05_*`

**Verdict: the slave-side OTA infrastructure is complete and should work.**

### What exists on the MASTER side (ESP32-as-master path) ⚠️

`CanOpenOTAStreaming.cpp` (122 lines) has:

- `flashSlave(nodeId, data, dataSize, crc32)` — **correctly implemented:**
  writes CRC32 → writes size → calls `writeSDODomain(0x2F00, data, dataSize)` → reads status.
  The underlying `_write_SDO` function uses `CO_SDOclientDownloadInitiate` +
  `CO_SDOclientDownloadBufWrite` + polling loop, which CANopenNode v4 handles as
  segmented SDO automatically when `dataSize > 4`.

- `actFromJson(doc)` — **BROKEN:** parses the JSON `{"task":"/ota_start","ota":{"nodeId":11,"size":N,"crc32":"..."}}`
  but then just logs `"OTA start acknowledged — awaiting firmware data"` and returns.
  **The serial binary receive mode is never implemented.** The TODO at line 116 says it all.

**The gap:** there is no code path that receives firmware bytes over serial on the master
and feeds them to `flashSlave()`. The old ISO-TP path had `BinaryOtaProtocol.cpp` for this
(385 lines of binary-mode serial protocol with sync bytes, chunking, ACK/NAK), but the
CANopen path doesn't reuse or replace it.

### What exists in Python ✅ (Waveshare direct path)

`uc2_ota_can.py` (153 lines) — **correctly implements the direct Waveshare path:**
1. Uses `python-canopen` library
2. Connects to CAN bus via Waveshare adapter
3. Does SDO downloads: CRC32 → size → firmware data (domain SDO)
4. `python-canopen` handles segmented transfer automatically
5. Reads back status

**This is the simpler and better path for production use.** Python → Waveshare → CAN bus → slave.
No ESP32 master needed. No serial binary mode needed. The python-canopen library handles all
the SDO segmentation.

### What exists in Python (serial → master path) ❌

`can_ota_streaming.py` (438 lines) — **old ISO-TP protocol, incompatible with CANopen.**
Uses a custom binary protocol (sync bytes `0xAA 0x55`, page-based chunking, ACK/NAK frames)
that talks to `BinaryOtaProtocol.cpp` / `CanOtaStreaming.cpp` on the master. This protocol
is completely different from CANopen SDO and cannot work with the new `CanOpenOTAStreaming.cpp`.

## The two paths forward

### Path A: Direct Waveshare → slave (recommended for production)

```
Laptop Python script
    │
    ▼ python-canopen SDO domain download
Waveshare USB-CAN-A adapter
    │
    ▼ CAN bus (500 kbit/s)
Slave ESP32-S3
    └── CanOpenOTA.cpp OD extension → esp_ota_write()
```

**This path already works in theory.** The Python script (`uc2_ota_can.py`) and the slave-side
code (`CanOpenOTA.cpp`) are both correctly implemented. The only thing that might not work is
if the python-canopen library doesn't support SDO domain download to DOMAIN-type OD entries,
or if the Waveshare adapter's timing is too slow for the SDO state machine.

**The key risk is throughput.** SDO segmented transfer at 500 kbit/s with 7 data bytes per
segment frame = ~87.5 KB/s theoretical max. A 1 MB firmware = ~12 seconds minimum. But SDO
segmented requires ACK every 7 bytes (one request + one response frame per segment), so actual
throughput is more like 7 bytes per 2 CAN frames = ~44 KB/s = ~23 seconds for 1 MB.

SDO **block** transfer (if supported by both python-canopen and CANopenNode) does ACK every
N segments (configurable, typically 127), which gets closer to the wire-speed limit.

### Path B: Serial → ESP32 master → CAN → slave (needed for setups without Waveshare)

```
Laptop Python script
    │
    ▼ binary data over serial @ 921600 baud
ESP32 master (USB serial)
    │
    ▼ CanOpenOTAStreaming::flashSlave() → writeSDODomain()
CAN bus (500 kbit/s)
    │
    ▼
Slave ESP32-S3
    └── CanOpenOTA.cpp OD extension → esp_ota_write()
```

**This path requires implementing the serial binary receive on the master side.** The firmware
binary is too large for a JSON payload (~1 MB), so we need a binary transfer mode where:
1. Python sends a JSON preamble: `{"task":"/ota_start","ota":{"nodeId":11,"size":1048576,"crc32":"0xABCD1234"}}`
2. Master acknowledges and enters binary receive mode
3. Python sends raw firmware bytes (921600 baud serial)
4. Master buffers chunks and calls `CanOpenOTAStreaming::flashSlave()` when complete
   (or streams them through in smaller chunks)

The old `BinaryOtaProtocol.cpp` had this, but it's wired to the ISO-TP stack, not to CANopen.

## Claude Code instructions

```
# PR-11: CANopen OTA — fix both paths

Branch: feature/canopen-ota
Depends on: current feature/runtime-config

## IMPORTANT CONTEXT

The slave-side OTA (CanOpenOTA.cpp) is correct and complete. Do NOT
modify it unless a bug is found during testing.

The Python direct-Waveshare path (uc2_ota_can.py) is correct and should
be tested first. Fix any issues there before touching the serial path.

## Step 1 — Test the Waveshare direct path (Python only, no firmware changes)

The Python script tools/canopen/uc2_ota_can.py uses python-canopen's SDO
domain download to write firmware directly to the slave. Test it first
because it has the fewest moving parts.

Update uc2_ota_can.py to support the Waveshare adapter properly:

    # The Waveshare USB-CAN-A uses a proprietary serial protocol, not slcan.
    # python-canopen needs a python-can bus backend. We already have
    # waveshare_bus.py as a python-can BusABC implementation.
    # Register it as a custom interface:

    import can
    from waveshare_bus import WaveshareBus

    # Create the python-can bus
    bus = WaveshareBus(channel=args.channel, bitrate=args.bitrate)

    # Create the canopen network using this bus
    network = canopen.Network()
    network.connect(bus=bus)

    # ... rest of the flash_node function stays the same ...

The key change: instead of `network.connect(bustype='slcan', ...)`, use
`network.connect(bus=bus)` with a pre-created WaveshareBus instance.

Also add a `--waveshare` flag that auto-detects the port:

    parser.add_argument("--waveshare", action="store_true",
                        help="Use Waveshare USB-CAN-A adapter (auto-detect port)")

Test with:
    python tools/canopen/uc2_ota_can.py --node 11 --binary firmware.bin --waveshare

Verify:
- The slave log shows "OTA started: size=NNNN, partition=ota_1"
- Progress logs appear every 64 KB
- "OTA verified. CRC32=0x... Rebooting in 2s..." appears
- The slave reboots with the new firmware

If this works, the direct path is done and can be used for production.

## Step 2 — Fix the serial → master → CAN path (firmware changes needed)

The serial binary receive needs to be implemented. The approach:

### 2a. JSON preamble starts the OTA session

In SerialProcess.cpp (or a new /ota_start endpoint in DeviceRouter),
handle the JSON command:

    {"task":"/ota_start","ota":{"nodeId":11,"size":1048576,"crc32":"0xABCD1234"}}

This should:
1. Parse nodeId, size, crc32 from the JSON
2. Allocate a buffer for the firmware (or prepare for streaming)
3. Switch the serial input to binary mode
4. Send an ACK response: {"ota_status":"ready","size":N}

### 2b. Binary receive mode

After the JSON preamble ACK, the master switches to binary mode:
- SerialProcess::loop() checks a flag `s_otaBinaryMode`
- If true, reads raw bytes into a buffer instead of parsing JSON
- Uses a simple protocol:
  - Python sends raw bytes continuously (no framing needed — size is known)
  - Master counts received bytes, sends progress ACKs every 4 KB
  - When all bytes received, verify CRC32 of the buffer

The simplest approach (and the one that works with the ESP32's available RAM):
**Stream through in chunks.** Don't buffer the entire 1 MB firmware in RAM.
Instead:

1. Master receives the JSON preamble with size and CRC32
2. Master sends CRC32 and size to the slave via SDO (triggers OTA begin on slave)
3. Master enters binary receive mode
4. For each 4096-byte chunk received over serial:
   a. Write it to the slave via `writeSDODomain(0x2F00, chunk, 4096)`
   b. Send a 4-byte ACK back over serial: bytes_received so far
5. After the last chunk, read OTA status from slave via SDO
6. Report success/failure over serial
7. Exit binary mode

Wait — this won't work directly because `writeSDODomain` does a single
SDO domain transfer for the ENTIRE blob. CANopenNode's SDO client expects
one `CO_SDOclientDownloadInitiate` call with the total size, then all the
segment data, then completion. You can't call `writeSDODomain` multiple
times for the same OD entry — each call is a new transfer.

**The correct approach is:**

1. Master receives ALL firmware bytes over serial first (into PSRAM or
   chunked flash buffer)
2. Then does ONE `writeSDODomain` call with the entire blob

The ESP32's PSRAM (2-8 MB on most boards) can hold 1-2 MB of firmware.
On ESP32 classic with only 512 KB of free DRAM (no PSRAM), this doesn't
work. Check the platform.

For ESP32 classic without PSRAM:
- Use a temporary SPI flash partition as a buffer
- OR use the slave's OTA partition as a double buffer (receive N KB,
  write to slave, receive next N KB, etc.)
- OR stream the SDO domain transfer in parallel with serial receive
  (complex but possible if we manage the SDO state machine manually)

For now, assume the master has PSRAM (the UC2 CAN HAT v2 uses ESP32
with PSRAM). If not, add a fallback.

### 2c. Implementation in CANopenModule/DeviceRouter

Add to DeviceRouter.cpp:

    cJSON* DeviceRouter::handleOtaStart(cJSON* doc) {
        cJSON* otaObj = cJSON_GetObjectItem(doc, "ota");
        if (!otaObj) return nullptr;

        cJSON* nodeIdItem = cJSON_GetObjectItem(otaObj, "nodeId");
        cJSON* sizeItem   = cJSON_GetObjectItem(otaObj, "size");
        cJSON* crc32Item  = cJSON_GetObjectItem(otaObj, "crc32");

        if (!nodeIdItem || !sizeItem) return nullptr;

        uint8_t  nodeId = (uint8_t)nodeIdItem->valueint;
        uint32_t fwSize = (uint32_t)sizeItem->valuedouble;
        uint32_t crc32  = 0;
        if (crc32Item) {
            if (cJSON_IsString(crc32Item))
                crc32 = (uint32_t)strtoul(crc32Item->valuestring, NULL, 16);
            else if (cJSON_IsNumber(crc32Item))
                crc32 = (uint32_t)crc32Item->valuedouble;
        }

        // Allocate buffer (PSRAM preferred)
        uint8_t* fwBuffer = (uint8_t*)heap_caps_malloc(fwSize, MALLOC_CAP_SPIRAM);
        if (!fwBuffer) {
            // Fallback to regular malloc
            fwBuffer = (uint8_t*)malloc(fwSize);
        }
        if (!fwBuffer) {
            cJSON* resp = cJSON_CreateObject();
            cJSON_AddStringToObject(resp, "ota_status", "error");
            cJSON_AddStringToObject(resp, "error", "out of memory");
            return resp;
        }

        // Store context for binary receive
        s_otaBuffer = fwBuffer;
        s_otaSize = fwSize;
        s_otaCrc32 = crc32;
        s_otaNodeId = nodeId;
        s_otaBytesReceived = 0;
        s_otaBinaryMode = true;  // SerialProcess::loop checks this

        cJSON* resp = cJSON_CreateObject();
        cJSON_AddStringToObject(resp, "ota_status", "ready");
        cJSON_AddNumberToObject(resp, "size", fwSize);
        return resp;
    }

In SerialProcess::loop(), at the top before JSON parsing:

    // OTA binary receive mode — read raw bytes instead of JSON
    if (s_otaBinaryMode) {
        int avail = Serial.available();
        if (avail > 0) {
            int toRead = min(avail, (int)(s_otaSize - s_otaBytesReceived));
            int nRead = Serial.readBytes(s_otaBuffer + s_otaBytesReceived, toRead);
            s_otaBytesReceived += nRead;

            // Progress ACK every 4 KB
            if ((s_otaBytesReceived % 4096) < nRead || s_otaBytesReceived >= s_otaSize) {
                char ack[32];
                snprintf(ack, sizeof(ack), "{\"ota_rx\":%lu}\n",
                         (unsigned long)s_otaBytesReceived);
                Serial.write(ack);
            }

            // All received?
            if (s_otaBytesReceived >= s_otaSize) {
                s_otaBinaryMode = false;

                // Verify CRC32
                uint32_t computedCrc = crc32_le(0, s_otaBuffer, s_otaSize);
                if (s_otaCrc32 != 0 && computedCrc != s_otaCrc32) {
                    Serial.printf("{\"ota_status\":\"error\",\"error\":\"crc_mismatch\"}\n");
                    free(s_otaBuffer);
                    s_otaBuffer = nullptr;
                    return;  // skip normal loop
                }

                // Flash the slave
                Serial.printf("{\"ota_status\":\"flashing\"}\n");
                bool ok = CanOpenOTAStreaming::flashSlave(
                    s_otaNodeId, s_otaBuffer, s_otaSize, s_otaCrc32);

                free(s_otaBuffer);
                s_otaBuffer = nullptr;

                if (ok) {
                    Serial.printf("{\"ota_status\":\"success\"}\n");
                } else {
                    Serial.printf("{\"ota_status\":\"error\",\"error\":\"flash_failed\"}\n");
                }
            }
        }
        vTaskDelay(pdMS_TO_TICKS(1));
        return;  // skip normal JSON parsing
    }

### 2d. Python script for serial → master path

Create tools/ota/canopen_ota_serial.py:

    #!/usr/bin/env python3
    """Flash slave firmware via serial → ESP32 master → CAN → slave."""

    import argparse
    import json
    import serial
    import struct
    import sys
    import time
    import zlib
    from pathlib import Path

    def flash_via_master(port: str, baud: int, node_id: int,
                         firmware: bytes) -> bool:
        crc32 = zlib.crc32(firmware) & 0xFFFFFFFF
        size = len(firmware)

        print(f"Target:    node {node_id}")
        print(f"Size:      {size:,} bytes")
        print(f"CRC32:     0x{crc32:08X}")

        ser = serial.Serial(port, baud, timeout=5)
        time.sleep(0.5)  # let ESP32 settle

        # Step 1: Send JSON preamble
        cmd = json.dumps({
            "task": "/ota_start",
            "ota": {
                "nodeId": node_id,
                "size": size,
                "crc32": f"0x{crc32:08X}",
            }
        })
        print(f"\n[1/3] Sending OTA start command...")
        ser.write((cmd + "\n").encode())
        ser.flush()

        # Wait for ready ACK
        deadline = time.time() + 10
        ready = False
        while time.time() < deadline:
            line = ser.readline().decode(errors='ignore').strip()
            if not line:
                continue
            if "ota_status" in line and "ready" in line:
                ready = True
                break
            print(f"  < {line}")

        if not ready:
            print("ERROR: Master did not acknowledge OTA start")
            ser.close()
            return False

        # Step 2: Send raw firmware bytes
        print(f"[2/3] Uploading firmware ({size:,} bytes)...")
        start = time.time()
        sent = 0
        chunk_size = 4096

        while sent < size:
            end = min(sent + chunk_size, size)
            n = ser.write(firmware[sent:end])
            sent += n

            # Read progress ACKs (non-blocking)
            while ser.in_waiting:
                ack = ser.readline().decode(errors='ignore').strip()
                if ack:
                    try:
                        d = json.loads(ack)
                        if "ota_rx" in d:
                            pct = d["ota_rx"] / size * 100
                            print(f"  Progress: {d['ota_rx']:,} / {size:,} ({pct:.0f}%)", end="\r")
                    except json.JSONDecodeError:
                        pass

            # Small delay to avoid overwhelming the serial buffer
            time.sleep(0.01)

        elapsed = time.time() - start
        print(f"\n  Transfer: {elapsed:.1f}s ({size/elapsed/1024:.0f} KB/s)")

        # Step 3: Wait for flash result
        print("[3/3] Waiting for flash result...")
        deadline = time.time() + 120  # 2 minutes max (SDO transfer is slow)
        while time.time() < deadline:
            line = ser.readline().decode(errors='ignore').strip()
            if not line:
                continue
            print(f"  < {line}")
            if "ota_status" in line:
                try:
                    d = json.loads(line)
                    if d.get("ota_status") == "success":
                        print("\n✓ OTA flash successful!")
                        ser.close()
                        return True
                    elif d.get("ota_status") == "error":
                        print(f"\n✗ OTA failed: {d.get('error','unknown')}")
                        ser.close()
                        return False
                except json.JSONDecodeError:
                    pass

        print("\n✗ Timeout waiting for flash result")
        ser.close()
        return False

    def main():
        parser = argparse.ArgumentParser(
            description="Flash UC2 slave firmware via serial → ESP32 master → CAN"
        )
        parser.add_argument("--port", default="/dev/cu.SLAB_USBtoUART",
                            help="Serial port of ESP32 master")
        parser.add_argument("--baud", type=int, default=921600)
        parser.add_argument("--node", type=int, required=True,
                            help="Target slave node ID")
        parser.add_argument("--binary", required=True,
                            help="Path to firmware .bin file")
        args = parser.parse_args()

        firmware = Path(args.binary).read_bytes()
        if not firmware:
            print("ERROR: Empty firmware file")
            sys.exit(1)

        success = flash_via_master(args.port, args.baud, args.node, firmware)
        sys.exit(0 if success else 1)

    if __name__ == "__main__":
        main()

## Step 3 — Wire the /ota_start endpoint in DeviceRouter

Add to DeviceRouter::routeCommand():

    if (strcmp(task, "/ota_start") == 0)
        return handleOtaStart(doc);

Add the endpoint constant to Endpoints.h:

    static const char* ota_start_endpoint = "/ota_start";

## Step 4 — Add the static state variables

In SerialProcess.cpp or a new OtaBinaryReceive.cpp, add:

    static bool     s_otaBinaryMode = false;
    static uint8_t* s_otaBuffer     = nullptr;
    static uint32_t s_otaSize       = 0;
    static uint32_t s_otaCrc32      = 0;
    static uint8_t  s_otaNodeId     = 0;
    static uint32_t s_otaBytesReceived = 0;

## Step 5 — writeSDODomain timeout for large transfers

The current `_write_SDO` has `timeoutMs=250` default, and `writeSDODomain`
passes `2000`. For a 1 MB firmware:
- 1 MB / 7 bytes per segment = 149,797 segments
- Each segment = 2 CAN frames (request + response) = ~1.1 ms at 500 kbit/s
- Total: ~165 seconds (2.75 minutes)

The `timeoutMs=2000` is the timeout per SDO transaction, not per segment.
CANopenNode's `CO_SDOclientDownload` advances the state machine per
segment, so the 2000 ms timeout applies between consecutive segment ACKs.
That should be fine — each ACK arrives in ~1 ms.

BUT: the `s_sdoMutex` is held for the ENTIRE domain transfer duration.
That's 2.75 minutes of blocking. Other SDO operations (motor commands,
state queries) will fail during OTA. This is acceptable for an OTA
operation but should be documented.

Also check: does `isNodeReachable()` interfere? During a 2.75 minute
transfer, the TPDO heartbeat from the slave will stop arriving because
the slave is busy doing esp_ota_write() in the SDO callback and may
miss its CO_tmr_task ticks. After 5 seconds without TPDO,
`isNodeReachable()` returns false and the SDO mutex acquisition fails.

**Fix:** bypass `isNodeReachable()` for OTA transfers. Either:
- Add a flag `s_otaInProgress` that makes `isNodeReachable` always return true
- OR increase the timeout from 5 s to 300 s during OTA
- OR skip the check in `writeSDODomain` entirely (it already checks
  `canBusReady()` which is sufficient)

In writeSDODomain, remove the isNodeReachable check:

    bool CANopenModule::writeSDODomain(...) {
        if (CO == NULL || CO->SDOclient == NULL) return false;
        // NOTE: no isNodeReachable check — domain transfers may take minutes
        // and the slave won't push TPDOs while busy writing to flash.
        if (s_sdoMutex && xSemaphoreTake(s_sdoMutex, pdMS_TO_TICKS(5000)) != pdTRUE) return false;
        ...
    }

## Step 6 — Verification

### Test A: Waveshare direct path
    python tools/canopen/uc2_ota_can.py --node 11 --binary firmware.bin --waveshare

Expected:
- "Writing CRC32..." → "Writing firmware size..." → "Uploading firmware..."
- Slave log: "OTA started", progress every 64 KB, "OTA verified", reboot
- Slave comes back with new firmware version

### Test B: Serial → master → CAN path
    python tools/ota/canopen_ota_serial.py --node 11 --binary firmware.bin --port /dev/cu.SLAB_USBtoUART

Expected:
- "Sending OTA start command..." → "ready" ACK
- "Uploading firmware..." → progress percentages
- "Waiting for flash result..." → "success"
- Slave reboots with new firmware

### Test C: OTA with motor still responding
- Start an OTA transfer
- Try sending /motor_act during transfer
- Motor command should fail gracefully (SDO mutex busy)
- OTA should complete normally
- After reboot, motor commands work again

## Do NOT

- Do NOT modify CanOpenOTA.cpp (slave side) unless a bug is found in testing
- Do NOT reuse BinaryOtaProtocol.cpp — it's ISO-TP-specific and will be
  deleted in PR-12
- Do NOT add a second SDO client — CANopenNode v4 supports one per node
- Do NOT change the OD entry layout for 0x2F00-0x2F05
- Do NOT add block transfer support in this PR — segmented SDO works and
  is simpler. Block transfer optimization is a future enhancement.
```

## Summary

There are two issues with the current OTA implementation:

**Issue 1 (the critical gap):** `CanOpenOTAStreaming::actFromJson()` on the master has a TODO
where the serial binary receive should be. It acknowledges the OTA command but never actually
receives any firmware bytes. The fix is ~60 lines: a binary-mode flag in `SerialProcess::loop()`
that reads raw bytes into a PSRAM buffer, then calls `flashSlave()` when all bytes arrive.

**Issue 2 (the Waveshare integration):** `uc2_ota_can.py` uses `network.connect(bustype='slcan')`
which won't work with the Waveshare adapter. Fix: pass a pre-created `WaveshareBus` instance
to `network.connect(bus=bus)`. Three lines of Python.

**Issue 3 (a latent bug):** `writeSDODomain` calls `isNodeReachable()` which will return false
after 5 seconds of OTA transfer because the slave stops pushing TPDOs while busy writing to
flash. Fix: skip `isNodeReachable` in `writeSDODomain`.

The slave-side code (`CanOpenOTA.cpp`) is well-structured and correct. The OD entries are
properly defined as DOMAIN type with OD extension callbacks. The flow (size → CRC → data stream
→ verify → commit → reboot) is the right CANopen pattern. No changes needed there.
