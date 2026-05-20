# UC2 CANopen OTA — diagnosis of the 1.1 KB/s throughput issue and concrete fix path

## What the logs actually tell us — converting confusion into facts

The slave log is the smoking gun. Compute the throughput from your numbers:

```
[ 7974] #1   written=0        ← first segment arrives
[ 8031] #2   written=28       Δ=57 ms for first 28 B
[ 8066] #3   written=56       Δ=35 ms
[ 8101] #4   written=84       Δ=35 ms
[ 9556] #64  written=1764     63 callbacks in 1582 ms → ~25 ms each, 28 B each
```

**Effective rate: 28 B / 25 ms = 1.1 KB/s.** A 798 KB firmware would take **12 minutes** at this rate. The master's 2 s/4 KB watchdog fires because at 1.1 KB/s, 4 KB takes 3.6 s.

So the question is not "why does it fail?" — it's **"why is each 28-byte burst taking 25 ms instead of ~2 ms?"**

## The physics: how long should one segment round-trip take?

A standard SDO segmented download cycle at 500 kbit/s CAN:

| Step | Time |
|------|------|
| Master sends segment frame (8 data bytes, 7 payload) | ~270 µs |
| Slave's TWAI ISR queues the frame | ~10 µs |
| Slave's `CO_interrupt_task` picks it up | up to 1 ms (vTaskDelay quantum) |
| Slave's `CO_process_SDOserver` processes (next `CO_process` tick) | up to 1 ms |
| Slave's SDO server queues the response frame | ~10 µs |
| Slave sends response frame | ~270 µs |
| Master's TWAI ISR + CO_process | up to 1 ms |
| **Total round-trip per segment** | **~2.5 ms theoretical** |

A 28-byte buffer flush = 4 segments × 2.5 ms ≈ **10 ms expected**.

You're seeing **25-35 ms per 28 B**. That's **3-5× slower than physics permits**. Something on the slave is *blocking the SDO state machine* for ~15-25 ms between bursts.

## Root cause: tiny SDO buffer × per-segment slave logging × competing FreeRTOS tasks

Three compounding effects, each measurable, all fixable:

### Cause A — the SDO server FIFO is only 32 bytes

CANopenNode's default is `CO_CONFIG_SDO_SRV_BUFFER_SIZE = 32`. The SDO server accumulates incoming segment payloads into this FIFO, then calls the OD write callback (`onOtaWriteChunk`) when it's full. With 32 B FIFO and 7 B payload per segment, the callback fires every **~28 bytes = every 4 segments**.

For a 798 KB firmware that's **~28,500 callbacks**. Each callback completes a full SDO round-trip at the CAN level (master sent 4 segments, slave responded 4 times), then control returns to the master to refill its client FIFO.

**This is the dominant bottleneck**. Bigger server FIFO = fewer callbacks = fewer round-trips = much higher throughput.

### Cause B — per-segment logging on the slave UART blocks the SDO task

Currently `onOtaWriteChunk` does:

```cpp
static uint32_t s_dbgCallCount = 0;
s_dbgCallCount++;
if (s_dbgCallCount <= 4 || (s_dbgCallCount & 0x3F) == 0) {
    log_i("onOtaWriteChunk #%lu: count=%u offset=%u ...");
}
```

That's ~80 chars at 115200 baud (the default for ESP_LOG output, NOT the 921600 of your USB-to-UART) = **~7 ms per line**. The log call is **synchronous** — it blocks the SDO server task on UART TX. While the task is blocked, no SDO segments are processed.

Look at your data: call #1→#2 is 57 ms (extra log writes for "OTA first segment" + the #1 callback log) vs steady-state 35 ms. That 22 ms gap **is the UART blocking time**.

### Cause C — two FreeRTOS tasks both tick the CANopen stack at 1 ms quanta

In `CANopenModule.cpp` you have two separate tasks both driving the CANopen stack:

```cpp
// Task A: CO_tmr_task (line 227)
for (;;) {
    CO_process_SYNC(...);
    CO_process_RPDO(...);
    syncRpdoToModules();      ← motor command dispatch (heavy)
    syncModulesToTpdo();      ← motor state TPDO (heavy)
    CO_process_TPDO(...);
    vTaskDelay(pdMS_TO_TICKS(1));
}

// Task B: the main CANopen task body (line 916)
while (reset == CO_RESET_NOT) {
    reset = CO_process(CO, false, 1000, NULL);  ← this includes CO_process_SDOserver
    vTaskDelay(pdMS_TO_TICKS(1));
}
```

Both wait 1 ms between iterations. The SDO **server** is processed only in Task B. Task A's `syncRpdoToModules` / `syncModulesToTpdo` walk all 4 motor axes with mutex locks every tick, even though the slave is doing zero motor work during OTA.

When Task A is in the middle of motor housekeeping, Task B can't run, so the SDO server can't respond. Combined with Cause B's UART blocking, you get the 15-25 ms tail latency on top of physics.

## What to actually fix — three changes, in priority order

### Fix 1 (HIGHEST IMPACT): Bump the slave SDO server FIFO from 32 → 900 bytes

**Where:** `lib/ESP32_CanOpenNode/src/CO_driver_target.h` — add before any CANopenNode includes:

```c
#ifndef CO_CONFIG_SDO_SRV_BUFFER_SIZE
#define CO_CONFIG_SDO_SRV_BUFFER_SIZE 900
#endif
#ifndef CO_CONFIG_SDO_CLI_BUFFER_SIZE
#define CO_CONFIG_SDO_CLI_BUFFER_SIZE 900
#endif
```

This is a static buffer per SDO instance — one server, one client — so it costs ~1.8 KB BSS total. Trivial on any ESP32.

**Expected impact:** OD callbacks fire every ~127 segments × 7 B = 889 B instead of every 4 segments × 7 B = 28 B. That's **30× fewer callbacks**. Each callback no longer waits for a full client-side refill round-trip — the master can keep pumping segments into a 900-byte server buffer while the previous batch is being processed. Throughput should jump from ~1 KB/s to **~20-30 KB/s** (close to wire-rate for segmented SDO). 798 KB OTA drops from 12 minutes to ~30 seconds.

**Important caveat:** after editing this header you MUST do a full rebuild. Partial builds may not pick up the change because CANopenNode internal `.c` files cache the value:

```bash
pio run -e UC2_canopen_slave_motor -t clean
pio run -e UC2_canopen_master_debug -t clean
pio run -e UC2_canopen_slave_motor -t upload --upload-port /dev/cu.usbmodem101
pio run -e UC2_canopen_master_debug -t upload --upload-port /dev/cu.SLAB_USBtoUART
```

### Fix 2 (CRITICAL FOR CORRECTNESS): Delete all per-segment logging from `onOtaWriteChunk`

In `main/src/canopen/CanOpenOTA.cpp`, the function `onOtaWriteChunk` currently has:
- A `s_dbgCallCount` counter and conditional `log_i` (lines ~163-174)
- An "OTA first segment" `log_i` (lines ~178-184)
- The "OTA progress" `log_i` on `s_nextProgressMark`

**Keep only ONE** progress log per 64 KB:

```cpp
static uint32_t s_nextProgressMark = 0;
if (s_bytesWritten == 0) s_nextProgressMark = 65536;  // first log at 64KB
if (s_bytesWritten >= s_nextProgressMark) {
    log_i("OTA progress: %lu / %lu bytes",
          (unsigned long)s_bytesWritten,
          (unsigned long)OD_OTA_FIRMWARE_SIZE);
    s_nextProgressMark += 65536;
}
```

Drop everything else. Even at 921600 baud you cannot afford UART writes inside the SDO hot path — they steal milliseconds from the very task they're trying to debug.

After Fix 1, callbacks fire every ~900 bytes, so even "every 64th call" is "every 57 KB" — fine. But until Fix 1 lands AND you remove per-segment logs, the UART blocking IS what's keeping you at 1 KB/s.

If you want fine-grained OTA debugging, write to a static circular buffer and dump it via `/state_get` AFTER the transfer finishes.

### Fix 3 (DEFENSIVE): Increase the master's per-chunk budget

The master aborts after 2 seconds at `CANopenModule.cpp:637-643`. Even after Fix 1, occasional 30-90 ms flash erase pauses on the slave could push a chunk above 2 s under bad luck. Make it survive that:

```cpp
// Was: static constexpr uint32_t SDO_STREAM_CHUNK_BUDGET_MS = 2000;
static constexpr uint32_t SDO_STREAM_CHUNK_BUDGET_MS = 10000;
```

10 s per 4 KB chunk = lower bound of 0.4 KB/s before abort. With ~30 KB/s actual throughput after Fix 1, this is 60× headroom — a true safety net, not a regular limit.

Also bump `SDO_STREAM_TIMEOUT_MS` from 5000 to 15000 — the final `sdoDownloadEnd` waits for the slave to run CRC verify + `esp_ota_end` + `set_boot_partition`, which can take several seconds on partially-written flash.

## Why the "different error every run" pattern happens

Your three consecutive runs showed `sdo_chunk_failed`, then `size_write_failed`, then `crc_write_failed`. This is the slave's SDO server going into an **error-recovery state** that takes seconds to clear:

- Run 1: master writes CRC32 + SIZE OK, opens 0x2F00 stream, fills 256/4096 bytes, hits 2 s watchdog, master aborts → master sends `CO_SDO_AB_GENERAL` abort frame.
- Slave's SDO server receives the abort, but its internal state machine has already been advanced by 36+ segment frames worth of inbound data — server is now in `STATE_DOWNLOAD_SEGMENTED` waiting for more data. The abort frame moves it back to idle, but the slave's OTA module still has an `esp_ota_handle` open from `esp_ota_begin`.
- Run 2: master tries the SIZE write again. Slave's `onOtaSizeWrite` callback notices `s_otaStarted` is already true, calls `resetOtaState()` which calls `esp_ota_abort()`. **But this can take a moment**, during which the SDO server times out — hence `0x05040000` (SDO Protocol Timed Out).
- After a slave reset everything works again.

**Fix 1 should make this go away entirely**, because no chunk transfer will ever take long enough to trip the 10 s watchdog (Fix 3) — and without aborts, no leftover state between runs.

## How to verify each fix is actually working

### Check A: Confirm the buffer size change took effect

Add this temporary log at the top of `onOtaWriteChunk`:

```cpp
static bool s_loggedBufSize = false;
if (!s_loggedBufSize) {
    log_i("SDO server buffer compile-time = %d",
          (int)CO_CONFIG_SDO_SRV_BUFFER_SIZE);
    s_loggedBufSize = true;
}
```

After rebuild and OTA attempt, look for `SDO server buffer compile-time = 900`. If it still prints `32`, your `#define` isn't reaching the CANopenNode internals — check that the `#define` is in the right header and you did a clean build.

### Check B: Measure throughput with one log line per 64 KB

After Fixes 1+2, the slave log during a successful OTA should look like:

```
[T+0]     OTA started: size=798432, partition=app1
[T+2.1s]  OTA progress: 65536 / 798432 bytes
[T+4.3s]  OTA progress: 131072 / 798432 bytes
[T+6.5s]  OTA progress: 196608 / 798432 bytes
...
[T+27s]   OTA progress: 786432 / 798432 bytes
[T+27.5s] OTA transfer complete: 798432 bytes
[T+27.6s] OTA verified. CRC32=0x... Rebooting in 2s...
```

If you see this — done.

### Check C: If you still see <10 KB/s after Fixes 1-3

Three remaining suspects, in likelihood order:

**1. Task priority inversion.** The main `CO_process` task should be at least as high priority as `CO_tmr_task`. Check `xTaskCreatePinnedToCore` calls in `CANopenModule.cpp`. If `CO_tmr_task` has higher priority, the SDO server task gets starved. Fix:

```cpp
// During OTA, boost the CO_process task's priority
vTaskPrioritySet(s_coProcessTaskHandle, configMAX_PRIORITIES - 2);
// ...after OTA ends, restore:
vTaskPrioritySet(s_coProcessTaskHandle, original_priority);
```

**2. `syncRpdoToModules` / `syncModulesToTpdo` doing too much work during OTA.** Add an early return:

```cpp
void CANopenModule::syncRpdoToModules_slave() {
    if (OD_OTA_STATUS == CANOPEN_OTA_RECEIVING ||
        OD_OTA_STATUS == CANOPEN_OTA_VERIFYING) return;
    // ... existing body ...
}
```

Same guard at the top of `syncModulesToTpdo`. The slave doesn't need motor housekeeping during the 30 s OTA window.

**3. CAN bus errors causing retransmissions.** Your boot log shows `CAN bus errors: 1001+` before the master connects. Once both nodes are up these should stop. If they continue during OTA, you have a wiring / termination issue — missing 120 Ω terminator at one end of the bus, or both ends terminated, or stub too long.

## Test plan — what to actually run

### Test 1 (mandatory): tiny payload to confirm Fix 1

```bash
head -c 8192 /dev/urandom > /tmp/test8k.bin
python tools/ota/canopen_ota_serial.py \
    --port /dev/cu.SLAB_USBtoUART --baud 921600 \
    --node 11 --binary /tmp/test8k.bin
```

Expected: completes in <1 s after Fix 1. If it takes 20+ seconds, Fix 1 didn't take effect (rebuild from clean).

The slave will boot into the random 8 KB partition, fail to start, then fall back to the previous valid partition automatically (ESP-IDF rollback). No harm done — just a verification of throughput.

### Test 2: full firmware with progress monitoring

```bash
python tools/ota/canopen_ota_serial.py \
    --port /dev/cu.SLAB_USBtoUART --baud 921600 --node 11 \
    --binary /Users/bene/Dropbox/.../esp32_seeed_xiao_esp32s3_can_slave_motor.bin
```

Expected: completes in 30-60 seconds. Watch the Python output for `ota_rx` progress updates roughly every 4 KB on the master side, and the slave log for `OTA progress` every 64 KB.

### Test 3: repeated runs without slave restart

Run Test 2 three times in a row WITHOUT power-cycling the slave between runs. After the third run the slave should be running the new firmware. Before any of these fixes, your repeated runs gave inconsistent errors due to leftover SDO server state. After Fix 1+3 there should be no aborts and therefore no leftover state — each run cleanly opens, transfers, finalises, reboots.

## TL;DR

The slave logs prove the OTA *is* working — it's just running at 1.1 KB/s. The bottleneck is the SDO server's 32-byte default buffer combined with per-segment debug logging on the slave UART; together they make every 28-byte burst take 25 ms instead of the ~2 ms the CAN bus would actually allow.

**Fix 1 — set `CO_CONFIG_SDO_SRV_BUFFER_SIZE = 900` in `CO_driver_target.h` — is the single most important change**. It makes the OD callback fire every ~900 bytes instead of every 28, giving 30× fewer mutex/task wakeups.

**Fix 2 — delete all per-segment `log_i` from `onOtaWriteChunk`** — prevents UART blocking from stealing time inside the SDO hot path.

**Fix 3 — bump master's per-chunk budget to 10 s and end timeout to 15 s** — defensive headroom for occasional flash-erase pauses.

Together these take 798 KB OTA from ~12 minutes (which always trips the master watchdog and produces the inconsistent "different error each run" behavior) down to ~30 seconds. The intermittent errors go away because nothing aborts the SDO session anymore, so the slave's SDO server never gets stuck in mid-transfer state between runs.

After these changes, if you still see <10 KB/s, the remaining bottleneck is task priority inversion on the slave — boost the `CO_process` task's priority during OTA via `vTaskPrioritySet` and gate `syncRpdoToModules` / `syncModulesToTpdo` to no-op when `OD_OTA_STATUS == RECEIVING`.
