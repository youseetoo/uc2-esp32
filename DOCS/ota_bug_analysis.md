# UC2 CANopen OTA — root-cause analysis of the remaining bugs

I read the updated `OtaBinaryReceive.cpp`, `CANopenModule.cpp` (the streaming SDO
functions), `CanOpenOTA.cpp`, `SerialProcess.cpp`, `main.cpp`, and
`canopen_ota_serial.py`. There are **four distinct bugs**, and they explain every
symptom you described: works only in debug mode, works only with one binary,
master crashes on the second run, and the transfer is slow.

---

## Bug 1 — The master serial RX loop blocks itself during a chunk (debug-mode dependency)

This is the "only works in debug mode" bug.

### The call chain

`main.cpp:159` calls `SerialProcess::loop()` once per main-loop iteration. Inside
`loop()` at `SerialProcess.cpp:415`:

```cpp
if (OtaBinaryReceive::isActive()) {
    OtaBinaryReceive::processBytes();
    vTaskDelay(pdMS_TO_TICKS(1));
    return;
}
```

`processBytes()` reads whatever is in the UART FIFO into `s_chunk`, and **when the
chunk fills (4096 bytes) it calls `flushChunk()` synchronously**. `flushChunk()`
calls `sdoDownloadChunk()`, which **spins for the entire duration of the CAN
transfer of those 4 KB** — currently ~3.4 seconds per chunk (see Bug 4).

So the timeline of one chunk is:

```
processBytes()  reads ~4096 B from UART FIFO  (fast, ~microseconds)
  └─ flushChunk()
       └─ sdoDownloadChunk()  ←── BLOCKS HERE for ~3400 ms
                                  pushing segments over CAN
processBytes() returns
```

**During those 3.4 seconds, `processBytes()` is not reading the UART.** The ESP32's
UART hardware FIFO is only **128 bytes**, and the driver ring buffer is typically
256–1024 bytes. The host (Python) keeps sending — but only sends the *next* chunk
after it sees the `{"ota_rx":N}` ACK, so in theory it's ACK-paced…

…except for the **first chunk**. Look at the Python:

```python
ser.write(payload)      # sends 4096 bytes immediately
ser.flush()
# then waits for ack
```

The host dumps 4096 bytes into the OS serial buffer in one `write()`. The master
reads `avail` bytes — but `Serial.available()` only returns what's *currently* in
the FIFO/ring buffer. On the very first `processBytes()` call it might see only
64–200 bytes, read those, fill `s_chunkPos` partially, and **return without
flushing** (chunk not full yet). Next `loop()` iteration reads more. This works…

…**unless the main loop is too slow to drain the UART before the 256-byte ring
buffer overflows.** In debug builds the timing is different (slower CPU paths,
log calls yielding) so the UART gets drained in smaller bites and never
overflows. In a release build the main loop spins faster, but the first
`flushChunk()` blocks for 3.4 s — and any bytes the host sent for chunk #2
**while chunk #1 was being flushed** overflow the 256-byte ring buffer and are
**silently dropped**. The firmware image on the slave is then corrupt, but the
CRC only catches it at the very end.

Actually it's worse than that. The real reason debug-vs-release differs:

### The actual mechanism

In **debug builds**, your log shows `[ACK] chunk #1 rtt=3415 ms` then chunk #2,
#3 … all succeed. The host is strictly ACK-paced — it does NOT send chunk #2
until `{"ota_rx":4096}` arrives. So why does release fail with `size_write_failed`
or a crash?

Because in release the **first writeSDO to the slave fails** — `node 0x0B not
reachable` — and that path is timing-sensitive. `waitForNodeReachable()` polls
`isNodeReachable()`, which checks whether a heartbeat/TPDO arrived recently. In a
release build, between `begin()` returning `{"ota_status":"ready"}` and the host
sending the first 4 KB, less wall-time passes, so the master may not yet have
seen the slave's first heartbeat after *its own* boot — and the 10 s wait inside
`flushChunk` is only reached *after* the first full 4 KB chunk is buffered. If
the host sends fast, the chunk fills and `flushChunk` runs early; the slave
genuinely hasn't been heard from yet → `size_write_failed`.

**The fix:** call `waitForNodeReachable()` in `begin()`, *before* sending
`{"ota_status":"ready"}` — not deferred until the first `flushChunk`. The host
should not even start streaming until the slave is confirmed reachable.

---

## Bug 2 — `processBytes()` only reads ONE FIFO-worth per main-loop iteration

Even ignoring Bug 1, `processBytes()` has a structural flaw:

```cpp
void processBytes() {
    if (!s_active) return;
    int avail = Serial.available();
    if (avail > 0) {
        // read up to min(avail, remaining, chunk-space) bytes  -- ONCE
        size_t nRead = Serial.readBytes(...);
        ...
        if (chunk full) flushChunk();
    }
    ...
}
```

It reads **once** per call, then returns. It's called once per `main.cpp` loop
iteration. Between calls, `main.cpp` runs `LinearEncoderController::loop()`,
`HomeMotor::loop()`, `TMCController::loop()`, … each with its own `vTaskDelay(1)`.

So the loop cadence on the master is maybe 10–30 ms per iteration. Each iteration
drains *at most* one FIFO (≤256 bytes typically). To fill a 4096-byte chunk you
need ~16 iterations = ~300–500 ms **just to assemble one chunk** before you even
start the CAN transfer. The host meanwhile has dumped all 4096 bytes and is
waiting — but the UART ring buffer is 256 bytes, so **bytes 256…4095 of that
chunk were dropped on the floor** unless the loop happened to drain fast enough.

The reason it *sometimes* works: the ESP32 Arduino UART driver ring buffer size
is set by `Serial.setRxBufferSize()`. If something in the debug build path
enlarged it, or the debug timing drains it fast enough, you get lucky.

**The fix:** in `processBytes()`, loop on `Serial.available()` and read
continuously until either the chunk is full or the FIFO is momentarily empty —
and crucially, **enlarge the UART RX buffer** at OTA start with
`Serial.setRxBufferSize(8192)` (must be called before `Serial.begin`, or re-`begin`
the port). Better yet, gate the host so it sends in ≤256-byte sub-chunks, or have
the host wait for a smaller ACK cadence.

---

## Bug 3 — Master crash on the 2nd run: SDO mutex never released + watchdog

This is the `abort() ... esp_task_wdt_add ... panic` you saw on run #2.

### What happens on run #1 (the failed/aborted run)

1. `begin()` sets `s_active = true`.
2. `flushChunk()` runs, calls `sdoDownloadBegin()` which **takes `s_sdoMutex`**
   and sets `s_sdoStreamActive = true`.
3. The transfer either completes, errors, or the host disconnects.
4. If the host gives up and the master's `RX_TIMEOUT_MS` (30 s) fires,
   `processBytes()` calls `cleanup()` → `cleanup()` calls
   `sdoDownloadAbort()` → that calls `CO_SDOclientClose`, `xSemaphoreGive(s_sdoMutex)`,
   `s_sdoStreamActive = false`. OK so far.

**But** look at the actual crash log — the panic is inside `esp_task_wdt_add`,
called from `esp_panic_handler_reconfigure_wdts`. That means a *different* panic
already happened and the panic handler itself faulted while reconfiguring
watchdogs. The original fault is masked.

### The real culprit: `sdoDownloadChunk` can run for up to 10 seconds holding the SDO mutex, while the Task WDT is 5 s

`SDO_STREAM_CHUNK_BUDGET_MS = 10000`. The fill/drain loops inside
`sdoDownloadChunk` do `vTaskDelay(1)` only **when `nWritten == 0`** (FIFO full) or
in the drain loop. But in the **fill loop**, if `CO_SDOclientDownloadBufWrite`
keeps accepting bytes (because the FIFO is large — you bumped it to 1024) and the
state machine keeps returning a non-negative non-zero code, the loop body runs:

```cpp
while (offset < count) {
    nWritten = CO_SDOclientDownloadBufWrite(...)  // accepts e.g. 200 B
    offset += nWritten;
    r = CO_SDOclientDownload(...)
    // budget check
    if (nWritten == 0) vTaskDelay(1);   // <-- ONLY yields if FIFO full
}
```

If `nWritten` is consistently > 0 (FIFO has room because the block transfer is
draining it), this loop **never calls `vTaskDelay`** and **never yields**. It's a
tight CPU loop on whatever core/task `SerialProcess::loop()` runs on (the main
Arduino `loopTask`, priority 1). The IDLE task on that core is priority 0 and
**never gets to run** → the Task WDT (which watches IDLE) fires after 5 s → panic.

The `vTaskDelay(1)` you added only covers the `nWritten == 0` case. The case where
the block transfer is healthy and streaming fast is exactly the case that starves
IDLE.

**The fix:** call `vTaskDelay(1)` (or at least `taskYIELD()` plus an explicit
`esp_task_wdt_reset()`) on **every** iteration of both loops, not only when
`nWritten == 0`. The CAN bus is the bottleneck anyway — yielding 1 ms per
iteration costs nothing in throughput but keeps the system alive.

### Why run #1 leaves the system wedged for run #2

If run #1's `sdoDownloadChunk` is killed by the WDT panic mid-transfer, the
master reboots — but the **slave** still has `s_otaStarted = true` and an open
`esp_ota_handle`. On run #2, `begin()` on the master is fine, but the first
`writeSDO_u32(OTA_FIRMWARE_CRC32)` reaches a slave whose SDO server may still be
mid-block-transfer state → `abort=0x05040000` (SDO protocol timeout) →
`crc_write_failed` / `size_write_failed`. That's the "different error each run"
pattern. The slave only recovers after its own reboot.

---

## Bug 4 — Block transfer is enabled but throughput is still ~1.2 KB/s

Your debug-mode run shows `rtt = 3000–3600 ms` per 4096-byte chunk. That's
**~1.2 KB/s**, essentially unchanged from before block transfer was "enabled".
A 912 KB image would take **12+ minutes**.

`sdoDownloadBegin` does pass `true` as the last arg to
`CO_SDOclientDownloadInitiate` (block enable). But block transfer is a
**negotiated** feature — the **server** (slave) must also have
`CO_CONFIG_SDO_SRV_BLOCK` enabled, and the negotiation only succeeds if **both**
the client buffer and the server buffer are large enough. CANopenNode falls back
to **segmented** transfer silently if block negotiation fails.

3.4 s for 4096 bytes works out to:
- 4096 bytes / 7 bytes-per-segment = 585 segments
- 3400 ms / 585 = **~5.8 ms per segment**

That's the round-trip-per-segment signature of **segmented mode**, not block
mode. Block mode would ACK every 127 segments, giving ~50–100 ms per 4 KB chunk.

So block transfer **is not actually being negotiated**. Likely causes, in order:

1. **The slave's `CO_CONFIG_SDO_SRV` doesn't have `CO_CONFIG_SDO_SRV_BLOCK` set.**
   The conversation shows edits to `CO_driver_target.h` but the slave build may
   not have picked them up, or the flag is being masked by `CO_config.h`
   defaults. The `#define` must be visible to `CO_SDOserver.c` at compile time.
2. **`CO_CONFIG_SDO_SRV_BUFFER_SIZE` on the slave is < 889 bytes.** Block
   transfer needs to buffer a full block (127 × 7 = 889 bytes) before flushing.
   If the slave buffer is 1024 that's fine; if it's still 32 or 256, block mode
   silently won't negotiate.
3. **`CO_CONFIG_CRC16` not enabled** — block transfer uses CRC16; without it,
   `CO_CONFIG_SDO_SRV_BLOCK` doesn't compile in.
4. **The 0x2F00 OD entry is a DOMAIN with `dataLength = 0`.** Block transfer to a
   domain entry needs the server's OD-write path to accept `bufferPartial`.
   `onOtaWriteChunk` returns `ODR_PARTIAL` — good — but verify the server's block
   path actually calls the extension write. (It does in CANopenNode v4, but only
   if block was negotiated.)

**The fix:** verify on the **slave** that all of these compile-time flags are set
*and that the build actually recompiled CANopenNode*. Add a one-time boot log on
the slave printing the effective values:

```cpp
log_i("SDO SRV cfg: BLOCK=%d BUF=%d CRC16=%d",
      (CO_CONFIG_SDO_SRV & CO_CONFIG_SDO_SRV_BLOCK) ? 1 : 0,
      CO_CONFIG_SDO_SRV_BUFFER_SIZE,
      (CO_CONFIG_CRC16 & CO_CONFIG_CRC16_ENABLE) ? 1 : 0);
```

If `BLOCK=0` or `BUF<889`, that's your throughput bug. Also add the same on the
master for `CO_CONFIG_SDO_CLI`.

A clean rebuild is mandatory after touching `CO_driver_target.h` — CANopenNode's
`.c` files are in a library and PlatformIO's incremental build frequently does
**not** recompile them when only a header changes:

```bash
pio run -e UC2_canopen_slave_motor  -t clean
pio run -e UC2_canopen_master_debug -t clean
```

---

## Bug 5 (latent) — `_write_SDO` ignores the `blockEnable` everywhere else, and the regular `writeSDO_u32` path for CRC/size is fine, but there's an inconsistency

Line 337: the *non-streaming* `_write_SDO` helper calls
`CO_SDOclientDownloadInitiate(..., false)` — block disabled. That's correct for
4-byte expedited writes (CRC32, size). No bug there, just noting it so nobody
"fixes" it by mistake.

The inconsistency: `sdoDownloadBegin` uses `true`. Good. Just make sure nobody
later changes `_write_SDO` thinking it's the same path.

---

## Why it "only works with one specific binary"

It doesn't, really — it works with whichever binary you happened to flash *and
then immediately OTA'd in debug mode before the master's UART ring buffer
overflowed*. The `esp32_seeed_xiao_esp32s3_can_slave_motor.bin` from the
`UC2-REST/binaries/latest/` folder is 798,432 bytes; the one you build locally is
~908–912 KB. The larger one takes longer, giving more opportunity for:
- a UART ring-buffer overflow (Bug 2),
- the 10 s chunk budget to be approached,
- the IDLE WDT to fire (Bug 3).

The "specific binary" is a red herring — it's a **timing/size threshold**, not a
content dependency. A corrupted transfer that happens to still pass CRC is
astronomically unlikely, so what you actually get with the "wrong" binary is an
abort partway through, not a bad flash.

---

## The fix list, in priority order

### Fix A — Yield on every loop iteration in `sdoDownloadChunk` / `sdoDownloadEnd`
Both the fill loop and the drain loop must `vTaskDelay(1)` **unconditionally**
each iteration, not only when `nWritten == 0`. This stops the IDLE-WDT panic
(Bug 3). Throughput is unaffected — the CAN bus is the bottleneck.

### Fix B — Move `waitForNodeReachable` into `begin()`, before returning "ready"
Don't defer reachability to the first `flushChunk`. The master should confirm the
slave is alive *before* telling the host to start streaming (Bug 1).

```cpp
cJSON* begin(uint8_t nodeId, uint32_t size, uint32_t crc32) {
    ...
    if (!CANopenModule::waitForNodeReachable(nodeId, 10000)) {
        cJSON_AddStringToObject(resp, "ota_status", "error");
        cJSON_AddStringToObject(resp, "error", "slave_unreachable");
        return resp;
    }
    // only now set s_active = true and return "ready"
}
```

### Fix C — Drain the UART fully in `processBytes()` + enlarge the RX buffer
Loop reading until the chunk is full or the FIFO is momentarily empty. And at
OTA start, grow the UART RX buffer:

```cpp
// in begin(), before s_active = true:
Serial.setRxBufferSize(8192);   // must be effective before data streams in
```

`setRxBufferSize` only takes effect on the next `Serial.begin()`, so either:
- call it very early in firmware setup unconditionally (cheap, 8 KB BSS), or
- re-`begin()` the UART inside `begin()` (riskier — drops in-flight bytes).

Recommended: set an 8–16 KB RX buffer **unconditionally at boot** in the
master's serial setup. It costs a few KB of RAM and removes the whole class of
overflow bugs.

Also in `processBytes()`:

```cpp
void processBytes() {
    if (!s_active) return;
    // Drain as much as possible this call, not just one FIFO-worth.
    while (Serial.available() > 0 && s_bytesReceived < s_totalSize) {
        uint32_t remaining = s_totalSize - s_bytesReceived;
        size_t space = CHUNK_SIZE - s_chunkPos;
        size_t toRead = min((size_t)Serial.available(), min((size_t)remaining, space));
        size_t nRead = Serial.readBytes(s_chunk + s_chunkPos, toRead);
        s_chunkPos += nRead; s_bytesReceived += nRead;
        if (nRead) s_lastByteMillis = millis();
        if (s_chunkPos >= CHUNK_SIZE) {
            if (!flushChunk()) { cleanup(); return; }
        }
    }
    // ... last-chunk flush, ACK, timeout logic ...
}
```

### Fix D — Verify block transfer actually negotiates (the throughput fix)
On the **slave**, confirm at compile time and log at boot:
- `CO_CONFIG_SDO_SRV` includes `CO_CONFIG_SDO_SRV_BLOCK`
- `CO_CONFIG_SDO_SRV_BUFFER_SIZE >= 889` (use 1024)
- `CO_CONFIG_CRC16` includes `CO_CONFIG_CRC16_ENABLE`

On the **master**, same for `CO_CONFIG_SDO_CLI` / `CO_CONFIG_SDO_CLI_BLOCK` /
`CO_CONFIG_SDO_CLI_BUFFER_SIZE`.

Then **clean-rebuild both**. If after that the per-chunk RTT drops from ~3400 ms
to ~50–150 ms, block transfer is working. If it's still seconds, block
negotiation is still failing — capture a CAN sniff of the first 20 frames after
`sdoDownloadBegin` and check whether the slave responds with the block-init
response (`0xA4` server command specifier) or the segmented one (`0x60`).

### Fix E — Make abort robust so run #2 always works
On the **master** side, `cleanup()` already calls `sdoDownloadAbort()`. Good. But
also: if `begin()` is called while `s_sdoStreamActive` is still true (left over
from a wedged previous run), force-release it:

```cpp
cJSON* begin(...) {
    if (CANopenModule::sdoDownloadActive()) {
        log_w("OTA: forcing abort of stale SDO stream");
        CANopenModule::sdoDownloadAbort();
    }
    if (s_active) { s_active = false; /* reset stale state */ }
    ...
}
```

On the **slave** side, `onOtaSizeWrite` already aborts a previous OTA when
`s_otaStarted` — good. But also reset on size==0 and consider a watchdog: if
`s_otaStarted` and no segment arrived for N seconds, auto-abort so a wedged
slave self-heals without a power cycle.

### Fix F — Host: send in smaller sub-chunks, keep ACK pacing
The host currently `ser.write(4096)` in one shot. Even with a bigger master RX
buffer, it's safer to send in 512–1024 byte sub-writes with a tiny inter-write
`flush()`. Keep the per-4KB `{"ota_rx"}` ACK gate. This makes the host
self-throttling regardless of master-side buffer size.

---

## One-paragraph summary

The OTA logic is basically correct, but four things conspire against it: (1) the
master's `sdoDownloadChunk` busy-loops without yielding when the FIFO has room,
starving the IDLE task and tripping the Task WDT — that's the crash on run #2;
(2) `processBytes()` only drains one UART-FIFO-worth per main-loop iteration and
the UART RX ring buffer is too small, so for large/fast transfers bytes are
dropped — that's the "only one binary / only debug mode" flakiness; (3)
reachability is checked too late (in `flushChunk` instead of `begin`), so a
release-timing race makes the first CRC/size write hit a not-yet-heard-from
slave; and (4) block transfer is passed `true` on the client but isn't actually
negotiating — almost certainly because the slave's `CO_CONFIG_SDO_SRV_BLOCK` /
buffer-size / CRC16 flags didn't make it into a clean rebuild — so you're still
in segmented mode at ~1.2 KB/s. Fix the unconditional yield first (stops the
crash), then move `waitForNodeReachable` into `begin`, enlarge + fully drain the
UART, and verify the block-transfer compile flags with a boot-time log and a
clean rebuild.
