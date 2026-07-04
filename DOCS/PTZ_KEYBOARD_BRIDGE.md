# PTZ Keyboard → CANopen Bridge (`UC2_canopen_bridge_ptz`)

Use a low-cost CCTV PTZ joystick keyboard (TPS3103C family, "Keyboard
Controller Ver 3.057") as a hardware control panel for the microscope: the
3-axis joystick drives X/Y (proportional speed) and zoom, the lens keys give
extra channels, and every discrete key (presets, AUX, iris) is forwarded to
the host so ImSwitch can map it to snap-image / illumination / whatever.

```
PTZ keyboard ──RS-485 (Pelco-D, 9600)──► RS485-TTL adapter ──UART1──►
XIAO ESP32-S3 bridge (node 61) ──CANopen SDO──► motor slaves (X/Y/Z/A)
                              └─TPDO2 events──► master ──serial JSON──► ImSwitch
```

The bridge reuses the PS4-USB-bridge pattern: `NODE_ROLE=2`, no local
actuators, all routes REMOTE, expedited SDO writes against the standard
motor-slave node-ids (X=0x0B, Y=0x0C, Z=0x0D, A=0x0A).

## Hardware

XIAO ESP32-S3 on the can-gpio-esp carrier + any RS485↔TTL transceiver
(MAX3485/MAX13487 style; automatic direction control preferred — we only
listen, so even the DE/RE pin can simply stay tied to receive).

| XIAO pin | GPIO   | connects to                        |
|----------|--------|------------------------------------|
| D7       | GPIO44 | adapter **RO** (TTL RX)            |
| D6       | GPIO43 | adapter **DI** (TTL TX, unused)    |
| 3V3      |        | adapter VCC                        |
| GND      |        | adapter GND (+ keyboard GND, RJ45 pin 5/6, if you can) |
| D1/D2    | GPIO2/3| CAN RX / TX (unchanged)            |

Keyboard side (RJ45 or screw terminals, see manual p.5): pin 2 = RS485B,
pin 4 = RS485B(A)… in practice: connect keyboard **A+ → adapter A**,
**B- → adapter B**. The keyboard needs its own 9–12 V supply.

## Keyboard configuration (once)

1. `995` + `AUX` → menu. Set **BAUDRATE = 9600** and **PROTOCOL → PTZ →
   PELCO_D**. Exit and save. (Shortcuts: `96`+`ACK` = 9600, `44`+`ACK` = Pelco-D.)
2. The keyboard is a bus master by itself ("host mode", manual ch. 4) — no
   matrix needed. Select the target camera address with `1` + `CAM`.
3. Re-power the keyboard after changing settings.

## Bring-up / debugging (do this first!)

Flash `UC2_canopen_bridge_ptz` and open the USB serial monitor (115200).
Debug level 1 (decoded frames) is the compile-time default
(`PTZ_DEBUG_DEFAULT`), so wiggling the joystick should immediately print:

```
{"ptz_frame":{"proto":"D","addr":1,"raw":"FF 01 00 04 20 00 25","pan":-32,"tilt":0,"zoom":0,...}}
```

Nothing showing? Escalate to raw byte dump:

```json
{"task":"/ptz_act", "debug":2}
```

* garbage bytes / `resyncs` climbing → wrong baud rate or A/B swapped
* nothing at all → wiring, adapter power, or keyboard not in PTZ mode
* frames with the wrong `addr` → press `1`+`CAM` on the keyboard, or set
  `{"task":"/ptz_act","addr":0}` to accept any address

Statistics and the last decoded frame at any time:

```json
{"task":"/ptz_get"}
```

CAN traffic is *not* required for any of this — the parser and debug output
run standalone, so you can bring the wiring up on the bench first.

## Runtime mapping

| Keyboard input           | Action                                        |
|--------------------------|-----------------------------------------------|
| joystick left/right      | X axis, speed ∝ deflection (0–63 → ±2000·mult) |
| joystick up/down         | Y axis, speed ∝ deflection                    |
| joystick twist / TELE-WIDE | Z axis, fixed speed (`PTZ_SPEED_MULTIPLIER_Z`) |
| NEAR / FAR               | A axis, fixed speed (`PTZ_SPEED_MULTIPLIER_A`) |
| joystick release         | stop frame → all axes stop                    |
| `N` + `PRESET`           | event `preset_set`, arg N → host              |
| `N` + `CALL`             | event `preset_call`, arg N → host             |
| `N` + `AUX` + `ON`/`OFF` | event `aux_on`/`aux_off`, arg N → host        |
| `OPEN` / `CLOSE` (iris)  | event `iris_open`/`iris_close` → host         |

A watchdog (`PTZ_MOTION_TIMEOUT_MS`, default 2000 ms, `/ptz_act
{"timeout":N}`, 0 = off) stops all axes if the keyboard goes silent while an
axis is moving, so a lost stop frame can't crash the stage.

Discrete keys are **not** interpreted on the bridge. They travel as a TPDO2
event frame (COB-ID `0x280 + 61`, payload marker `0x80`, sequence-deduped) to
the master, which prints:

```json
{"ptz":{"event":1,"node":61,"type":1,"name":"preset_call","arg":1,"seq":7}}
```

UC2-REST / ImSwitch subscribes to `"ptz"` like it does to `"gpio"` collision
events and decides what preset 1 means (e.g. **snap image**). That gives you
255 presets × 3 verbs + 6 AUX toggles + iris keys ≈ far more assignable
functions than the CAN payload ever needs.

The master must know the bridge's node-id: `MASTER_PTZ_NODE_ID = 61`
(set in `UC2_canopen_master/PinConfig.h`; `disabled` elsewhere).

## Build / flash

```bash
pio run -e UC2_canopen_bridge_ptz -t upload     # the bridge (XIAO S3)
pio run -e UC2_canopen_master -t upload         # master (new ptz sniffer)
```

## Files

* `main/src/ptz/PelcoParser.{h,cpp}` — Pelco-D/P frame validation + decode
* `main/src/ptz/PtzKeyboard.{h,cpp}` — UART1 RX task, `/ptz_act`, `/ptz_get`
* `main/src/ptz/PtzRouter.{h,cpp}` — 50 Hz motion → SDO, events → TPDO2
* `main/config/UC2_canopen_bridge_ptz/PinConfig.h` — pins, speeds, node-id
* `CANopenModule.cpp: forwardPtzSlaveTpdo` — master-side JSON forwarder

Protocol reference: the keyboard manual ("Keyboard Controller Ver 3.057")
and levkovigor/PTZProtocolHandler (MIT) which documents the same keyboard
family; the parser here is self-contained ESP-IDF code.
