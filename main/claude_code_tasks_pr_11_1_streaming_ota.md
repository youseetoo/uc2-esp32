# PR-11.1 — Stream OTA chunks through SDO without buffering the full firmware

**Branch:** `feature/canopen-ota-streaming-fix`
**Depends on:** current `feature/runtime-config` (PR-11 partially done — OtaBinaryReceive exists)
**Estimated scope:** ~150 lines changed in `OtaBinaryReceive.cpp`, ~50 lines added in
`CANopenModule.cpp`, no Python changes

---

## What's actually broken — root cause

Your master is **ESP32 classic** (`board = esp32dev`), not ESP32-S3. The
`UC2_canopen_master` env at line 1037 of `platformio.ini` extends
`env:esp32dev` which sets `board = esp32dev`.

On ESP32 classic, PSRAM support depends entirely on **whether the physical
chip module has PSRAM hardware**:
- **WROOM-32** module → no PSRAM (most common, what `esp32dev` defaults to)
- **WROVER** module → 4 MB or 8 MB PSRAM
- The UC2 CAN HAT v2 schematic might use WROVER, but **the build target says
  WROOM**.

Even though the sdkconfig has `CONFIG_SPIRAM=y` and `CONFIG_BOARD_HAS_PSRAM=1`
is defined, **none of that creates physical PSRAM**. If the chip is a
WROOM-32 (no PSRAM):
- `heap_caps_malloc(size, MALLOC_CAP_SPIRAM)` returns NULL because no SPIRAM
  region was registered with the heap
- `ESP.getPsramSize()` returns 0
- The fallback `heap_caps_malloc(size, MALLOC_CAP_8BIT)` then tries to
  allocate the entire ~800 KB firmware in the ~200 KB of free internal
  DRAM → also returns NULL
- `out_of_memory` is the result you see

The **proper fix is not to enable PSRAM**. Even if PSRAM works, allocating
800 KB just to forward it to the slave wastes RAM and time. The right
architecture is to **stream chunks through the SDO client** without ever
holding the full firmware in memory.

## The correct architecture: chunked SDO download with partial buffer

CANopenNode v4's SDO client explicitly supports this pattern. From
`CO_SDOclient.h` documentation:

> If SDO Fifo buffer is too small, data can be refilled in the loop.

The API:

```c
// 1. Initiate transfer with total size known up-front
CO_SDOclientDownloadInitiate(SDO_C, index, sub, totalSize, timeoutMs, false);

// 2. In a loop, refill the buffer with the next chunk
while (more_data) {
    // Write a chunk into the SDO client's internal FIFO
    size_t nWritten = CO_SDOclientDownloadBufWrite(SDO_C, chunkData, chunkSize);

    // Advance the state machine with bufferPartial=true (more data coming)
    do {
        SDO_ret = CO_SDOclientDownload(SDO_C, dt_us,
                                        /*abort=*/false,
                                        /*bufferPartial=*/true,   // NOTE: true
                                        &abortCode, NULL, NULL);
        if (SDO_ret < 0) return abortCode;
        if (SDO_ret == CO_SDO_RT_blockDownldInProgress ||
            SDO_ret == CO_SDO_RT_uploadDataBufferFull) break;
        taskYIELD();
    } while (SDO_ret > 0);

    // Now we can refill with the next chunk
    chunkData = next_chunk();
}

// 3. Final call with bufferPartial=false to finalize
CO_SDOclientDownload(SDO_C, dt_us, false, /*bufferPartial=*/false, &abortCode, NULL, NULL);
```

**Memory cost: just the SDO FIFO** (a few KB) plus a small chunk buffer
(4-16 KB). Nothing close to 800 KB.

---

## Why this works on ESP32 classic without PSRAM

The serial RX path receives bytes into a small ring buffer (typically 1-4 KB)
in the UART driver. We **pull** from that buffer into our chunk buffer (we
control the size, say 4 KB). We feed the chunk to the SDO client. CAN
transmits it (one segment per CAN frame, 7 bytes each). After the slave
ACKs the last segment of this chunk, we can refill the FIFO with the next
chunk from serial.

The serial host (Python) needs to **respect flow control** — don't send
faster than the CAN bus can drain. At 500 kbit/s with segmented SDO:
- ~280 µs per segment frame + ~280 µs per ACK = ~560 µs per 7-byte payload
- ~12.5 KB/s wire-rate, ~12 KB/s effective payload rate
- 800 KB firmware → ~67 seconds

Compared to the current (broken) "buffer everything in PSRAM" approach,
chunked streaming is:
- Constant memory (4 KB chunks vs 800 KB monolithic)
- Same throughput (limited by CAN, not by serial)
- Works on ESP32 classic without PSRAM
- Works on ESP32-S3 too
- No platformio.ini changes needed

## What changes

### 1. `CANopenModule.h` — add the streaming SDO API

```cpp
// Streaming SDO download — for OTA and other large transfers.
// Caller manages chunk buffering; CANopenModule holds the SDO session open.
static bool sdoDownloadBegin(uint8_t nodeId, uint16_t index, uint8_t sub,
                              uint32_t totalSize);
static bool sdoDownloadChunk(const uint8_t* data, size_t count);
static bool sdoDownloadEnd();
static void sdoDownloadAbort();
```

### 2. `CANopenModule.cpp` — implement the three primitives

Replace the single-shot `_write_SDO` blocking helper with primitives that
hold the SDO session across multiple calls.

```cpp
// Static state for the streaming session (one active session at a time)
static bool                s_sdoStreamActive = false;
static CO_SDOclient_t*     s_sdoStreamClient = nullptr;
static uint32_t            s_sdoStreamTotalSize = 0;
static uint32_t            s_sdoStreamBytesQueued = 0;

bool CANopenModule::sdoDownloadBegin(uint8_t nodeId, uint16_t index,
                                      uint8_t sub, uint32_t totalSize) {
    if (s_sdoStreamActive) return false;
    if (CO == nullptr || CO->SDOclient == nullptr) return false;
    if (!canBusReady()) return false;

    // Take the mutex for the ENTIRE streaming session
    if (s_sdoMutex && xSemaphoreTake(s_sdoMutex, pdMS_TO_TICKS(500)) != pdTRUE)
        return false;

    s_sdoStreamClient = CO->SDOclient;
    CO_SDO_return_t r = CO_SDOclient_setup(s_sdoStreamClient,
        CO_CAN_ID_SDO_CLI + nodeId,
        CO_CAN_ID_SDO_SRV + nodeId, nodeId);
    if (r != CO_SDO_RT_ok_communicationEnd) {
        xSemaphoreGive(s_sdoMutex);
        return false;
    }

    // Initiate with KNOWN total size — this triggers the slave to call
    // esp_ota_begin(size) before any data arrives.
    r = CO_SDOclientDownloadInitiate(s_sdoStreamClient, index, sub,
                                      totalSize, /*timeoutMs=*/5000, false);
    if (r != CO_SDO_RT_ok_communicationEnd) {
        xSemaphoreGive(s_sdoMutex);
        return false;
    }

    s_sdoStreamTotalSize = totalSize;
    s_sdoStreamBytesQueued = 0;
    s_sdoStreamActive = true;
    return true;
}

bool CANopenModule::sdoDownloadChunk(const uint8_t* data, size_t count) {
    if (!s_sdoStreamActive) return false;
    if (count == 0) return true;

    // Refill the SDO FIFO. If the FIFO is full, this returns less than count.
    // We loop: write what fits, run the state machine to drain the FIFO over
    // CAN, then refill with the rest.
    size_t offset = 0;
    uint64_t lastUs = esp_timer_get_time();

    while (offset < count) {
        size_t nWritten = CO_SDOclientDownloadBufWrite(s_sdoStreamClient,
                                                       data + offset,
                                                       count - offset);
        offset += nWritten;

        // Whether or not the FIFO is full, run the state machine to drain it.
        // Pass bufferPartial=true because more chunks may follow.
        bool isLastChunk = (s_sdoStreamBytesQueued + offset >= s_sdoStreamTotalSize);
        for (;;) {
            uint64_t now = esp_timer_get_time();
            uint32_t dtUs = (uint32_t)(now - lastUs);
            lastUs = now;

            CO_SDO_abortCode_t abortCode = CO_SDO_AB_NONE;
            CO_SDO_return_t r = CO_SDOclientDownload(s_sdoStreamClient,
                                                     dtUs,
                                                     /*abort=*/false,
                                                     /*bufferPartial=*/!isLastChunk,
                                                     &abortCode, NULL, NULL);
            if (r < 0) {
                ESP_LOGE("OTA_SDO", "Stream chunk failed: ret=%d abort=0x%08lX",
                         r, (unsigned long)abortCode);
                sdoDownloadAbort();
                return false;
            }
            // r > 0 with bufferPartial means "FIFO needs refill — give me more"
            // r == CO_SDO_RT_blockDownldInProgress or similar — still running
            // r == 0 = complete (only when bufferPartial=false on final chunk)
            if (r == 0) break;

            // If state machine is waiting for more data and we have more to
            // write, break out to refill.
            if (offset < count && nWritten == 0) {
                // FIFO has room now — try again
                break;
            }
            if (offset >= count && !isLastChunk) {
                // We've written everything we have for this chunk; the caller
                // will call sdoDownloadChunk again. Return success.
                s_sdoStreamBytesQueued += count;
                return true;
            }
            if (isLastChunk && r > 0) {
                // Final chunk, still in progress — keep spinning
                taskYIELD();
                continue;
            }
            taskYIELD();
        }
        if (offset >= count) break;
    }

    s_sdoStreamBytesQueued += count;
    return true;
}

bool CANopenModule::sdoDownloadEnd() {
    if (!s_sdoStreamActive) return false;

    // Final call with bufferPartial=false to finalize the transfer.
    uint64_t lastUs = esp_timer_get_time();
    for (;;) {
        uint64_t now = esp_timer_get_time();
        uint32_t dtUs = (uint32_t)(now - lastUs);
        lastUs = now;

        CO_SDO_abortCode_t abortCode = CO_SDO_AB_NONE;
        CO_SDO_return_t r = CO_SDOclientDownload(s_sdoStreamClient,
                                                  dtUs, false, false,
                                                  &abortCode, NULL, NULL);
        if (r < 0) {
            ESP_LOGE("OTA_SDO", "Stream end failed: 0x%08lX",
                     (unsigned long)abortCode);
            sdoDownloadAbort();
            return false;
        }
        if (r == 0) break;
        taskYIELD();
    }

    CO_SDOclientClose(s_sdoStreamClient);
    s_sdoStreamActive = false;
    s_sdoStreamClient = nullptr;
    if (s_sdoMutex) xSemaphoreGive(s_sdoMutex);
    return true;
}

void CANopenModule::sdoDownloadAbort() {
    if (!s_sdoStreamActive) return;

    // Send abort to slave
    CO_SDO_abortCode_t abortCode = CO_SDO_AB_GENERAL;
    CO_SDOclientDownload(s_sdoStreamClient, 0, /*abort=*/true, false,
                         &abortCode, NULL, NULL);
    CO_SDOclientClose(s_sdoStreamClient);
    s_sdoStreamActive = false;
    s_sdoStreamClient = nullptr;
    if (s_sdoMutex) xSemaphoreGive(s_sdoMutex);
}
```

### 3. `OtaBinaryReceive.cpp` — rewrite to stream without buffering

Replace the current "allocate big buffer, fill, then flash" with
"allocate small chunk, fill, push to SDO, repeat":

```cpp
namespace {
// Small streaming buffer in INTERNAL DRAM — fits anywhere
constexpr size_t CHUNK_SIZE = 4096;
uint8_t   s_chunk[CHUNK_SIZE];
size_t    s_chunkPos    = 0;

bool      s_active           = false;
uint32_t  s_totalSize        = 0;
uint32_t  s_expectedCrc32    = 0;
uint32_t  s_runningCrc32     = 0;
uint8_t   s_nodeId           = 0;
uint32_t  s_bytesReceived    = 0;
uint32_t  s_lastAckBytes     = 0;
uint32_t  s_lastByteMillis   = 0;
bool      s_streamOpen       = false;

constexpr uint32_t ACK_INTERVAL_BYTES = 4096;
constexpr uint32_t RX_TIMEOUT_MS      = 30000;

void emitJson(const char* msg) { Serial.write(msg); Serial.write('\n'); }

void cleanup() {
    if (s_streamOpen) {
        CANopenModule::sdoDownloadAbort();
        s_streamOpen = false;
    }
    s_active = false;
    s_totalSize = 0;
    s_expectedCrc32 = 0;
    s_runningCrc32 = 0;
    s_nodeId = 0;
    s_bytesReceived = 0;
    s_lastAckBytes = 0;
    s_chunkPos = 0;
}

// Push the buffered chunk to the slave via streaming SDO.
// Returns false on error (and emits an error JSON).
bool flushChunk() {
    if (s_chunkPos == 0) return true;

    if (!s_streamOpen) {
        // Step 1: write CRC32 (so slave can verify) — separate one-shot SDO
        if (!CANopenModule::writeSDO_u32(s_nodeId, UC2_OD::OTA_FIRMWARE_CRC32,
                                          0, s_expectedCrc32)) {
            emitJson("{\"ota_status\":\"error\",\"error\":\"crc_write_failed\"}");
            return false;
        }
        // Step 2: write firmware size (triggers esp_ota_begin on slave)
        if (!CANopenModule::writeSDO_u32(s_nodeId, UC2_OD::OTA_FIRMWARE_SIZE,
                                          0, s_totalSize)) {
            emitJson("{\"ota_status\":\"error\",\"error\":\"size_write_failed\"}");
            return false;
        }
        vTaskDelay(pdMS_TO_TICKS(200));  // let slave call esp_ota_begin

        // Step 3: open the streaming SDO session for the firmware data
        if (!CANopenModule::sdoDownloadBegin(s_nodeId,
                UC2_OD::OTA_FIRMWARE_DATA, 0, s_totalSize)) {
            emitJson("{\"ota_status\":\"error\",\"error\":\"sdo_begin_failed\"}");
            return false;
        }
        s_streamOpen = true;
    }

    // Push this chunk to the slave
    if (!CANopenModule::sdoDownloadChunk(s_chunk, s_chunkPos)) {
        emitJson("{\"ota_status\":\"error\",\"error\":\"sdo_chunk_failed\"}");
        return false;
    }

    // Update running CRC32 for end-of-transfer verification
    s_runningCrc32 = crc32_le(s_runningCrc32, s_chunk, s_chunkPos);
    s_chunkPos = 0;
    return true;
}
}  // namespace

cJSON* OtaBinaryReceive::begin(uint8_t nodeId, uint32_t size, uint32_t crc32) {
    cJSON* resp = cJSON_CreateObject();
    if (!resp) return nullptr;

    if (s_active) {
        cJSON_AddStringToObject(resp, "ota_status", "error");
        cJSON_AddStringToObject(resp, "error", "ota_in_progress");
        return resp;
    }
    if (size == 0 || size > 8 * 1024 * 1024) {
        cJSON_AddStringToObject(resp, "ota_status", "error");
        cJSON_AddStringToObject(resp, "error", "invalid_size");
        return resp;
    }

    // NO MORE BIG ALLOCATION. We stream through s_chunk[4096].
    s_totalSize        = size;
    s_expectedCrc32    = crc32;
    s_runningCrc32     = 0;
    s_nodeId           = nodeId;
    s_bytesReceived    = 0;
    s_lastAckBytes     = 0;
    s_lastByteMillis   = millis();
    s_chunkPos         = 0;
    s_streamOpen       = false;
    s_active           = true;

    ESP_LOGI(TAG, "OTA session ready: node=%u size=%u crc32=0x%08lX (streaming, 4KB chunks)",
             nodeId, (unsigned)size, (unsigned long)crc32);

    cJSON_AddStringToObject(resp, "ota_status", "ready");
    cJSON_AddNumberToObject(resp, "size", (double)size);
    cJSON_AddNumberToObject(resp, "nodeId", nodeId);
    cJSON_AddNumberToObject(resp, "chunkSize", (double)CHUNK_SIZE);
    return resp;
}

void OtaBinaryReceive::processBytes() {
    if (!s_active) return;

    int avail = Serial.available();
    if (avail > 0) {
        uint32_t remaining = s_totalSize - s_bytesReceived;
        size_t toRead = (size_t)avail;
        if (toRead > remaining) toRead = remaining;
        // Don't overflow the chunk
        if (toRead > (CHUNK_SIZE - s_chunkPos))
            toRead = CHUNK_SIZE - s_chunkPos;

        size_t nRead = Serial.readBytes(s_chunk + s_chunkPos, toRead);
        s_chunkPos       += nRead;
        s_bytesReceived  += nRead;
        if (nRead > 0) s_lastByteMillis = millis();

        // If the chunk is full OR this is the last byte, flush to the slave
        bool isLast = (s_bytesReceived >= s_totalSize);
        if (s_chunkPos >= CHUNK_SIZE || isLast) {
            if (!flushChunk()) {
                cleanup();
                return;
            }
        }

        // Progress ACK every ACK_INTERVAL_BYTES
        if ((s_bytesReceived - s_lastAckBytes) >= ACK_INTERVAL_BYTES || isLast) {
            s_lastAckBytes = s_bytesReceived;
            char ack[48];
            snprintf(ack, sizeof(ack), "{\"ota_rx\":%lu}",
                     (unsigned long)s_bytesReceived);
            emitJson(ack);
        }
    } else {
        if ((millis() - s_lastByteMillis) > RX_TIMEOUT_MS) {
            ESP_LOGE(TAG, "OTA receive timeout after %u/%u bytes",
                     (unsigned)s_bytesReceived, (unsigned)s_totalSize);
            emitJson("{\"ota_status\":\"error\",\"error\":\"rx_timeout\"}");
            cleanup();
            return;
        }
    }

    if (s_bytesReceived < s_totalSize) return;

    // All bytes received and flushed. Verify the running CRC matches.
    if (s_expectedCrc32 != 0 && s_runningCrc32 != s_expectedCrc32) {
        char err[96];
        snprintf(err, sizeof(err),
                 "{\"ota_status\":\"error\",\"error\":\"crc_mismatch\","
                 "\"computed\":\"0x%08lX\",\"expected\":\"0x%08lX\"}",
                 (unsigned long)s_runningCrc32,
                 (unsigned long)s_expectedCrc32);
        emitJson(err);
        cleanup();
        return;
    }

    emitJson("{\"ota_status\":\"flashing\"}");

    // Close the SDO session — this is when the slave runs its final CRC check
    // and calls esp_ota_end + set_boot_partition + reboot.
    bool ok = CANopenModule::sdoDownloadEnd();
    s_streamOpen = false;

    if (ok) {
        emitJson("{\"ota_status\":\"success\"}");
    } else {
        emitJson("{\"ota_status\":\"error\",\"error\":\"sdo_end_failed\"}");
    }

    cleanup();
}
```

### 4. Remove the obsolete `flashSlave()` path from `CanOpenOTAStreaming.cpp`

`CanOpenOTAStreaming::flashSlave(nodeId, data, dataSize, crc32)` requires a
full-buffer pointer and is no longer used. Either delete it or mark it
deprecated with a comment pointing to `OtaBinaryReceive` and the new
streaming SDO primitives.

If you want to keep `flashSlave()` for testing (Waveshare direct-flash from
Python is unaffected — it uses python-canopen's own SDO segmentation),
leave it but add a note that it requires the full firmware in RAM.

### 5. Update the Python script

`tools/ota/canopen_ota_serial.py` does NOT need to change. The protocol is
the same from Python's perspective:
1. Send JSON preamble
2. Wait for `{"ota_status":"ready"}`
3. Stream raw bytes
4. Watch for `{"ota_rx":N}` progress messages
5. Wait for `{"ota_status":"success"}` or `error`

The master now flushes chunks to the slave as they arrive instead of
buffering. The Python script can keep sending at full serial speed — the
master will throttle naturally because `flushChunk` blocks until the SDO
transfer of that chunk completes.

**One subtle change:** the round-trip latency per chunk is now ~330 ms
(4096 bytes × 80 µs per CAN frame). If the host's serial buffer fills up
faster than the master can drain it, the host's `ser.write()` will block.
That's fine — flow control happens naturally via the OS serial buffer.

If you want explicit per-chunk ACKs, change the Python loop to wait for an
`ota_rx` ACK before sending the next chunk. The current code's
"opportunistic ACK reading" works but doesn't enforce flow control.

---

## Step-by-step Claude Code instructions

```
PR-11.1 — Streaming OTA without PSRAM

Implement chunked SDO download so that the ESP32 classic master can flash
slave firmware via serial → CAN without needing PSRAM or a large RAM buffer.

The current OtaBinaryReceive::begin() fails with "out_of_memory" because
it tries to allocate the entire firmware (~800 KB) in either PSRAM (which
the WROOM-32 chip doesn't have) or internal DRAM (which is only ~200 KB).

The fix is to stream chunks through the SDO client using CANopenNode's
bufferPartial mode. Memory cost drops from 800 KB to 4 KB.

## Step 1 — Add streaming SDO primitives to CANopenModule

File: main/src/canopen/CANopenModule.h

Add to the public class section:

    // Streaming SDO download — for large transfers that don't fit in RAM.
    // The caller manages chunk buffering; CANopenModule holds the SDO
    // session open across multiple chunks. Total size is known up front
    // and passed to Begin; each Chunk call appends data; End finalises.
    // Only one streaming session can be active at a time (per node).
    static bool sdoDownloadBegin(uint8_t nodeId, uint16_t index, uint8_t sub,
                                  uint32_t totalSize);
    static bool sdoDownloadChunk(const uint8_t* data, size_t count);
    static bool sdoDownloadEnd();
    static void sdoDownloadAbort();

File: main/src/canopen/CANopenModule.cpp

Add the four functions as static methods. The implementation pattern:

  - Begin: take s_sdoMutex, CO_SDOclient_setup, CO_SDOclientDownloadInitiate
    with the total size, mark s_sdoStreamActive=true
  - Chunk: call CO_SDOclientDownloadBufWrite, then loop CO_SDOclientDownload
    with bufferPartial=true until the FIFO is drained or the chunk is
    fully written
  - End: loop CO_SDOclientDownload with bufferPartial=false until the state
    machine returns 0 (complete), then CO_SDOclientClose and release mutex
  - Abort: send abort flag, close, release mutex

Reference: see /home/claude/archiv8/lib/ESP32_CanOpenNode/src/301/CO_SDOclient.h
for the API and the example block comment near CO_SDOclientDownloadInitiate.

Key details:
- Use esp_timer_get_time() to compute dt_us between CO_SDOclientDownload calls
- Use taskYIELD() instead of vTaskDelay(1) to avoid the 1 ms tick floor
- Acquire s_sdoMutex in Begin, release in End and Abort (not per-chunk)
- Skip isNodeReachable() check — during OTA, slave may miss TPDOs while
  writing flash, and we don't want to abort mid-transfer
- Increase the SDO timeout to 5000 ms (slave flash writes can be slow)

## Step 2 — Rewrite OtaBinaryReceive to use streaming

File: main/src/canopen/OtaBinaryReceive.cpp

Remove the large allocation:

  // DELETE THIS:
  // uint8_t* buf = (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_SPIRAM);
  // if (!buf) buf = (uint8_t*)heap_caps_malloc(size, MALLOC_CAP_8BIT);
  // if (!buf) { return error("out_of_memory"); }

Replace with a small static chunk buffer:

  static constexpr size_t CHUNK_SIZE = 4096;
  static uint8_t s_chunk[CHUNK_SIZE];

Update begin() to NOT allocate. Just initialise the streaming session state
(totalSize, expectedCrc32, nodeId, bytesReceived=0, etc.) and return ready.

Update processBytes() to:
  1. Read serial bytes into s_chunk at s_chunkPos
  2. When s_chunk is full OR last byte of firmware arrived:
     a. On FIRST flush: write CRC32 SDO, write size SDO (triggers slave
        esp_ota_begin), call sdoDownloadBegin with total firmware size
     b. Call sdoDownloadChunk with current chunk
     c. Update running CRC32
     d. Reset s_chunkPos to 0
  3. After last chunk: call sdoDownloadEnd to finalise, emit success
  4. On any error: call sdoDownloadAbort, emit error

The cleanup() function should call sdoDownloadAbort if the stream is open.

## Step 3 — Verify CO_SDOclient buffer size is reasonable

File: lib/ESP32_CanOpenNode/src/CO_driver_target.h (or similar)

Check if CO_CONFIG_SDO_CLI_BUFFER_SIZE or similar is defined. The default
buffer is typically 31 bytes for segmented mode, which is fine — we just
need to call sdoDownloadChunk with smaller refills if the FIFO is small.

Our 4 KB chunks will trickle into the SDO FIFO 31 bytes at a time, which
is exactly the segmented SDO frame payload (7 bytes per frame × 4 frames =
28 bytes typical). The state machine in sdoDownloadChunk handles this
naturally — CO_SDOclientDownloadBufWrite returns how many bytes it
accepted, and we loop until the chunk is fully accepted.

If performance is poor, increase the FIFO size by defining
CO_CONFIG_SDO_CLI_BUFFER_SIZE=4096 in build_flags. Default should be fine
for a working baseline.

## Step 4 — Remove or deprecate the now-broken full-buffer flash path

File: main/src/canopen/CanOpenOTAStreaming.cpp

The function CanOpenOTAStreaming::flashSlave(nodeId, data, dataSize, crc32)
requires the full firmware buffer in RAM. With the new streaming approach,
OtaBinaryReceive::processBytes() bypasses it entirely.

Two options:
  (a) Delete flashSlave() entirely. The only other caller is CanOpenOTA's
      JSON command handler which was a TODO stub anyway.
  (b) Keep flashSlave() but mark it deprecated — useful for Waveshare
      direct-flash tests where the host owns the full buffer.

Recommend (b) for now; delete it in a later cleanup PR.

## Step 5 — Verification

After implementing the above:

1. Build env:UC2_canopen_master (no PSRAM required)
2. Flash the master
3. Verify boot logs do NOT mention PSRAM (we no longer depend on it)
4. Run:
     python tools/ota/canopen_ota_serial.py \
       --port /dev/cu.usbmodem7 --baud 921600 \
       --node 11 --binary firmware.bin

Expected output sequence on the master serial:
  [info] OTA session ready: node=11 size=798432 (streaming, 4KB chunks)
  {"ota_status":"ready","size":798432,"nodeId":11,"chunkSize":4096}
  {"ota_rx":4096}
  {"ota_rx":8192}
  ... (every 4KB)
  {"ota_rx":798432}
  {"ota_status":"flashing"}
  {"ota_status":"success"}

Expected on the slave:
  OTA started: size=798432, partition=ota_1
  OTA progress: 65536 / 798432 bytes
  ... (every 64KB)
  OTA transfer complete: 798432 bytes
  OTA verified. CRC32=0x... Rebooting in 2s...

Expected free heap on the master during OTA:
  >100 KB free DRAM throughout (we only use 4 KB for the chunk buffer)

## Step 6 — Things NOT to do

- Do NOT add board_build settings for PSRAM. Streaming makes PSRAM
  unnecessary for OTA. If you want PSRAM for other reasons (large LED
  patterns, image buffers), that's a separate change.
- Do NOT increase the chunk size above 16 KB. CANopenNode's FIFO
  is small; large chunks just sit on the host side until the FIFO
  drains. 4 KB matches the flash page size and gives good throughput.
- Do NOT remove the CRC32 verification on the master side. The slave
  also verifies CRC32, but a master-side check catches host→master
  serial corruption before it wastes 60+ seconds of CAN transfer.
- Do NOT make sdoDownloadChunk re-entrant. Only one streaming session
  per master at a time.
```

---

## What you can do TODAY without firmware changes (alternative path)

If you don't want to wait for the firmware fix to land and ship, **use the
direct Waveshare path right now**:

```bash
python tools/canopen/uc2_ota_can.py \
    --node 11 \
    --binary firmware.bin \
    --waveshare
```

This bypasses the ESP32 master entirely. Python → Waveshare USB-CAN-A →
slave. The `python-canopen` library handles SDO segmentation in the host,
not on the master. No master code involved, no PSRAM needed, no buffer
needed on the master.

The only requirement is that the Waveshare adapter is on the same CAN bus
as the slave. If your master is plugged into the bus, you'll need to
either:
- (a) Disconnect the master temporarily (the bus must have at least one
  active node ACKing frames — your slave does that)
- (b) Leave the master powered but stop it from generating traffic (e.g.,
  send NMT stop to the master via Waveshare first)
- (c) Use a second CAN port on a different bus segment

For production setups where the master is always on, the streaming serial
path (this PR) is the right answer. For dev / one-off updates, Waveshare
direct works today.

---

## Summary

| Approach | Memory needed | Works on WROOM-32 | Status |
|----------|---------------|-------------------|--------|
| Buffer everything in PSRAM | ~800 KB PSRAM | NO (no PSRAM) | broken (current) |
| Buffer everything in DRAM | ~800 KB DRAM | NO (only 200 KB) | impossible |
| Stream 4 KB chunks through SDO | 4 KB DRAM | YES | the fix |
| Waveshare → slave directly | 0 on master | N/A | works today |

The root cause is treating SDO domain transfer as "one big call". CANopenNode
v4 was specifically designed to stream — that's what `bufferPartial=true`
exists for. Once the streaming primitives are in place, OTA works on any
ESP32 master, with or without PSRAM, and you can ship a unified firmware.
