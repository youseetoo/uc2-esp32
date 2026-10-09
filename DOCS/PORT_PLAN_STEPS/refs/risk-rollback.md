# Ref: Risks & rollback (short form)

Full risk register: `DOCS/PORT_PLAN_C_ESPIDF.md` §8. Index: `../PORT_PLAN_STEPS.md`.

| Risk | Where handled |
|---|---|
| IDF 4.4→6.x driver deltas | T1.1 explicit sdkconfig; T1.3 shim isolates deltas; laser app proves stack first |
| CANopenModule 138 KB big-bang | split in T4.4 (per-concern files); streaming-SDO OTA covered by suite mocks |
| BT RAM (master) | T3.1 baseline; T3.2 classic-only config; headroom logged in T3.7 |
| RMT channel/IRQ ownership (FAS vs WS2812) | Ws2812Rmt stays bare-metal; T2.2/T2.7/T3.4(UC2_4) explicit `[HW]` timing checks; no second RMT driver ever installed |
| Ws2812Rmt chip-specific structs | S3 proven in T2.1/T2.2, classic-ESP32 in T2.4/T3.4 — both chips covered before Phase 4 |
| FAS timing drift | D2 default = keep C++ leaf; T2.7 trace comparison before touching it |
| M5GFX boards | D4 gates T5.4; they stay on PIO meanwhile (no CI impact) |
| Dual-maintenance sprawl | ci-cutover.md §6 timebox + per-board §4 checklist; parked-board list |
| Stale-file confusion | T2.11 single revertable cleanup commit |
| Anything | every task = own commit/PR; PIO stays green until T6.1; worst case: revert to last green `port/idf-c` state, or back to `main`/`main_espidf` (untouched) |
