# Serial & CANopen interface review — 2026-10-08

Scope: everything a host or a CAN node sees of the interface. Code read, not hardware-tested.

| Repo | State reviewed |
|---|---|
| uc2-ESP (firmware) | `feature/strobed-sweep` @ `3c86acd` (8 commits ahead of `main`) |
| UC2-REST (`uc2rest`) | `mergemaster` @ `766dd4a` |
| UC2-REST-CANOPEN (`uc2canopen`) | `main` @ `41309e0` (0.2.3) |
| ImSwitch | `master` @ `42b8064e` (config defaults only) |

Paths: `SP` = `main/src/serial/SerialProcess.cpp`, `DR` = `main/src/canopen/DeviceRouter.cpp`, `CM` = `main/src/canopen/CANopenModule.cpp`, `RT` = `main/src/canopen/RoutingTable.cpp`.

---

## 1. Fix first (crash, wrong motion, silent failure)

| # | Where | Problem | Suggested fix |
|---|---|---|---|
| 1 | FW `TMCController.cpp:382-385`, `SP:213, 549-552` | `/tmc_get` on a board without local TMC (e.g. `UC2_canopen_master`) returns the request object → freed twice | return a new object or `nullptr` |
| 2 | FW `MotorJsonParser.cpp:355-356, 417-418` | `setpos` without `posval`, `setdir` without per-axis `setdir` → NULL dereference | check items before use |
| 3 | FW `CM:1434, 1585-1588` | NMT *Reset Node* (0x81): `CO_delete()` without restart; other tasks keep using `CO` (use-after-free, node silent until power cycle). `uc2canopen.State.reset_node()` broadcasts this by default | on `CO_RESET_APP` call `esp_restart()` |
| 4 | FW `DR:~489`, `FocusMotor.cpp:46`, `RT:159-170` | Standalone v4 (`MOTOR_AXIS_COUNT 4`): `/route_set {"type":"MOTOR","id":4,…}` then `/motor_act` writes `FocusMotor::getData()[4]` → out of bounds. "Axes 4+ via CAN" in `UC2_canopen_standalone_v4/PinConfig.h` is not implemented | **fixed (uncommitted):** `stepperid` bounds-checked in `/motor_act`, `/motor_get`, `/home_act`, `HomeMotor`; remote motors ≥ 4 via `/route_set` + multi-axis satellite |
| 5 | FW `OD.c:52-101`, `CM:2727-2732` | Every node enables RPDO1–4 on 0x18A–0x18D and TPDO1 (event 500 ms). Satellites echo node 10's data in their own TPDO1 → with an A motor (node 10) on the bus the master can attribute A's position/running bit to X/Y/Z, incl. false `isDone`. *Needs hardware confirmation* | enable the RPDOs only on the master; TPDO1 maps the node's own axis |
| 6 | FW `CM:2127-2136, 2436-2451` vs `DR:215-216, 1389` | Satellites read `0x2005` (enable) and `0x2020–0x2026` (TMC) only from sub 2; master writes sub stepperid+1 → `isen` and `/tmc_act` ignored on A/Y/Z satellites | remap like hard limits (`CM:2953-2962`) |
| 7 | FW `QidRegistry.cpp:149`, `.h:12` | qid timeout 30 s: long moves get `"state":"timeout"` while running; later `done` suppressed. Remote laser/LED report `done` even if the SDO failed (`DR:755-764, 1113-1120`); a remote zero-length move never completes | per-qid timeout from move estimate; report failure; complete immediately when no motion |
| 8 | uc2rest `home.py:70-71, 316-318` | `home_x/y/z/a()` with defaults send `"timeout": null` → firmware reads 0 → immediate timeout | default `endstoptimeout` |
| 9 | uc2rest `home.py:352-361` | `stop_home()` sends `{"stop":true,"home":…}`; router ignores `stop` and **starts** homing with speed 0 | send `/motor_act isStop` |
| 10 | uc2rest `mserial.py:51, 483-503, 624-653` | first reply must arrive within 1 s regardless of `timeout`; replies without `qid` are attached to the previous qid; one miss (or any line containing "error") makes the next call fail with "communication interrupted" | honour `timeout`; match qid-less replies by endpoint; don't treat log text as error |
| 11 | FW + all clients | Each node has one SDO server; master, PS4/PTZ/dial bridges and the Pi (`uc2canopen`) are all SDO clients with no arbitration. `uc2canopen` accepts any 0x580–0x5FF frame as its reply (`sdo.py:128-136`) | match node+index+sub in `uc2canopen`; listen-only/handover mode on the master; second SDO server channel for bridges |

## 2. Serial protocol (firmware)

**Framing**
- Every framed reply ends with `\0` after `--\n` (`SP:279, 604`: length includes the terminator).
- Not framed or not JSON: `/b` → `++{'b':N}--` (single quotes, one line, `SP:751`); boot `{'setup':'done'}`; CAN-OTA progress lines (`OtaBinaryReceive.cpp:52-55, 229`); LED overheat uses `+|` (`LedController.cpp:1195`).
- `HomeMotor.cpp:899-902`, `FocusScan.cpp:108-113`, `MessageController.cpp:203-208`, `LedController.cpp:1195-1213` print `++`/json/`--` with three `Serial.println` outside the output queue → other frames can interleave.
- A newline inside an object discards it → pretty-printed JSON always fails (now documented).
- Input queue 16, output queue 10, both drop silently (`SP:370-375, 154-159`). Stage scan emits a `steppers … qid:0` frame per point plus `cam` frames.

**Acknowledgements**
- Five success vocabularies: `success` int (`serialize(int)`), `success` bool (galvo), `return` (sometimes the qid: LED local, TMC), `ok`, `status`, `ota_status`.
- `serialize(int)` reports `"success":0` for a successful command sent without qid (`SP:555-573`).
- Unknown task and router `nullptr` both give `{"qid":-1,"success":-1}` → request qid lost (`SP:861-865`).
- No qid echo: `/laser_act`, `/ledarr_act`, most `*_get`, `/tmc_*`, `/state_act`, `/can_get`, `/can_act` (non-scan), `/route_*`, `/config_*`, `/modules_get`. This is the root cause of uc2rest #10.
- `/qid_state` sends two frames.

**Keys and types**
- `acceleration` (read) vs `accel` (`JsonKeys.h:141`, docs, comments, stage-scan fallback); `isaccel`, per-axis `isen`, `redu` ignored.
- `rms_current` in `/tmc_act` vs `rmscurr` in `/tmc_get`; TMC addressed by `axis`, everything else by `stepperid`; out-of-range `axis` silently → driver 0 (`TMCController.cpp:37-40`).
- `getJsonInt` ignores booleans (`cJsonTool.h:7-23`): `"hardhome":true` = 0.
- `/message_act`: request `value`, push `data`, qid read from inside `message`.
- `/route_set` takes uppercase, `/route_get` prints lowercase + `"disabled"` (now accepts both). ~~`subAxis` forced 0; RAM only~~ fixed: `subAxis` (default = id), `persist`, `reset`, input validation.
- `/digitalin_get`: nested object locally, flat remotely.
- `strobesweep` is an object in batches but `true` in the final message.
- Heat `Ki`/`Kd` parsed as int (`HeatController.cpp:128, 138`).

**Local vs remote gaps** (same JSON, different behaviour)
- Not implemented for remote devices: `/laser_get`, `/home_get`, `/ledarr_get`; `/tmc_get` never forwarded; `setpos` not forwarded (`DR:191-194`).
- LED: `brightness`, `patternId`, `patternSpeed`, `led_array` remote only; `status` and quadrants local only.
- Home: `endstoprelease` remote only; `endoffset`, `maxspeed` local only; omitted polarity sent remotely as 255 (`DR:841`).
- Galvo remote drops `trig_delay_us`, `trig_width_us`, `frame_count`, LUT, overscan, blanking; completion not tracked.
- Stage-scan LED commands carry no `task` → ignored by the local LED parser (`LedController.cpp:645-651`, `StageScan.cpp:117-133`).
- `/laser_act` requires both `LASERid` and `LASERval` (freq/res-only impossible), replies `return:1` even on local failure, no qid.
- `focusscan` floors speed/acceleration at 20000 / 1e6 (`MotorJsonParser.cpp:299-300`).
- `/b` reflects the laser busy flag only; stuck at 1 after `/laser_act` without `LASERval`.

**Dead or broken surface**
- Declared, never dispatched: `/motor_setcalibration`, `/config_act`, `/dac_get`, `/encoder_*`, `/linearencoder_*`, `/readanalogin_*`, `/bt_remove`, `/bt_paireddevices`, `/resetnv`.
- `SCANNER_CONTROLLER` and `I2C_MASTER` builds would not compile (undefined / duplicate endpoint strings).
- Dead code: `SerialProtocol.*` (MessagePack, not in CMakeLists), `processJsonDocument`, `MotorJsonParser::act`, `HomeMotor::act`, `s_defaultNodes`; legacy `#if CAN_BUS_ENABLED && !CAN_CONTROLLER_CANOPEN` blocks referencing deleted `src/can` (`main.cpp:95-97, 311-316, 508-511`, `State.cpp:234`).
- `UC2_canopen_master/PinConfig.h:140-143` sets `MOTOR_*_STEP = GPIO_NUM_0` so that bare `/motor_get` lists the remote axes. GPIO0 is a strapping pin; list routed axes explicitly instead.
- `copy_PinConfig.py` does not strip `_default` → `*_default` envs and `UC2_WEMOS` silently build with whatever `main/PinConfig.h` is present. 43 `sdkconfig.*` files and several `main/config/*` folders have no env.
- `/bt_*` send no reply. Memory leak per `/motor_act` in log builds (`MotorJsonParser.cpp:476`).

## 3. CANopen

**OD vs EDS vs registry**
- `openUC2_satellite.eds` (generated from the registry) ≠ compiled `lib/uc2_od/OD.c`: identity (EDS 0x55C2/0x00010001 vs device 0x1234/1), heartbeat (500 vs 1000 ms), all PDO comm/mapping defaults, 21 objects that do not exist (`0x2104/05, 0x2204/05, 0x2302, 0x2311, 0x2320, 0x2341/42, 0x2400–03, 0x2502, 0x2506, 0x2700–05`), `0x2609/0x260A` U32 vs 2 bytes (EDS-driven writes abort 0x06070012), access and PDO flags, `DefaultValue`s that are never applied (all RAM = 0). `regenerate_all.py:304` writes ms into the 100 µs inhibit field. `OD.h:1-3` and the registry header claim OD.c is generated — it is hand-written. `openUC2_satellite_new.eds` is stale.
- Bit semantics in registry/EDS differ from code: `0x2003` (per-slot stop bits 4–7, not one stop flag), `0x2004` (bit 1 = 1 Hz toggle, not homed), `0x2602` (4 stop / 5 e-stop), `0x2F03` (error = 0xFF, not 4), `0x2F05` codes, `0x2200` modes 6–8 absent, `0x2604` is a sample period in µs, `0x2609/0x260A` are sample counts, `0x2607/0x2608` reused for bidirectional/settle.
- Present but never populated/consumed: `0x2008–0x200A`, `0x2027`, `0x2101–0x2103`, `0x2106`, `0x2203`, `0x2340`, `0x2505`, `0x2601`; `0x2502` has RAM but no descriptor.
- Signed values in unsigned objects: `0x2002` speed (sign = direction), `0x2012` direction (U8 descriptor, int8 RAM, I8 in EDS).
- → Make one source (registry → OD.c + EDS + `od.py` + `uc2_indices.py`) and check it in CI.

**Network behaviour**
- Master feedback: single-axis satellites still only via RPDO COB-IDs 0x18A–0x18D (nodes 10–13). **Fixed (uncommitted):** feedback is matched by (node, `subAxis`) instead of slot = `subAxis`; multi-axis satellites (standalone v4, `CAN_MULTI_AXIS_SLAVE`) report per axis on `0x480+node`, any node ID.
- Default routes cover motor ids 0–3 only; `CAN_ID_MOT_B..G` unused. More via `/route_set`.
- On a multi-axis satellite, RPDOs from nodes 10–13 overwrite its `0x2001/0x2004/0x2016` slots (same root cause as §1 #5); the master therefore skips the SDO position refresh for multi-axis nodes.
- No NMT master, all nodes self-start; no heartbeat consumer (`0x1016` = 0); liveness by frame sniffing (`CM:398-411`).
- Bitrate hard-coded 500 kbit/s; LSS bit-timing ignored; LSS slave only (comments about LSS conflict resolution are misleading).
- Node-ID schemes disagree: `PinConfigDefault.h` (used: A 10, laser 20, LED 30, galvo 40, GPIO 60) vs registry `UC2_NODE` (unused: A 14, LED 20, galvo 30) vs `platformio.ini` comments vs `uc2canopen.NODE` vs ImSwitch `UC2CANOpenManager` (A 14, LED 20, laser 21).
- Default motor image = node 11 for every board without the CI axis flag; Waveshare 7" screen also node 11. NVS `canRole`/`canNodeId` survive flashing a different role.
- `canMotorAxis` stored but unused (`CM:2855-2857`). `/can_get` `busOffCount` is the bus-error count (`CM:1620`).
- `CanOpenOTA::registerOdExtensions()` spawns a new watchdog task on every comm reset (`CanOpenOTA.cpp:430`, `CM:1552`).
- Non-standard choices to document or change: OTA in `0x2F00` (CiA 302 uses `0x1F50`), reboot via `0x2507`, node-ID via `0x250A`, SYNC as raw frames, PTZ events overload `0x2300`, TPDO4 base 0x580 (SDO response range), placeholder vendor ID 0x1234.
- No listen-only/hand-over mode on the master, although `uc2canopen` is meant to drive the same bus from the Pi.

## 4. uc2rest

- Default `baudrate` 115200; HAT+ master is 921600 → firmware check fails, auto-detect may attach to another board or the mock (`UC2Client.py:51`, `mserial.py:117-173`).
- `serialport=None` → client without `.serial`; non-existent port string → auto-detect; existing port that fails the check → auto-detect too.
- Return type is `list` / `int` / `str`. Methods that index the result as a dict always fail: `motor.get_motor(s)`, `isBusy`, `get_hard_limits`, `get_joystick_direction`, `get_speed_multiplier`, closed-loop helpers (`setAxisMode` … `calibrateAxis`), `state.isBusy/getHeap`, `temperature.get_temperature`, `modules.get_modules`.
- Calls to non-existent endpoints/keys: `set_motor*` (`/motor_set`), `set_motor_acceleration` (`accel` → triggers a 0-step move), `setTrigger` (`setTrig`), `*soft_limits*`, `startStageScanning` (legacy keys), `canota.start_ota_update` and `start_*_ota` (`/can_act ota` has no handler), `state.set_state`, `led.set_led`, `laser.set_laserpin`, `analog.*`, `objective.*` (except home), `gripper.*`, `rotator.*`, `lcd.*`, `camera.*`, `wifi.*`, `modules.set_modules`, `digitalout.setup_digitaloutpin/sendTrigger`.
- Crashes: `move_xyz()` default speed (ValueError), `stopStageScanning()` (TypeError), `home(axis="X")` (TypeError), `set_laser(auto_filterswitch=True)` (AttributeError), PyScript path (`SerialManagerWrapper` signature).
- Wrong payloads: `message.trigger_message` (not nested), `home` `endposrelease` sent as `endoffset` (ignored on CAN axes), `move_forever` always sends all 4 axes, `set_tmc_parameters` drops `stall_value`, `send_LEDMatrix_single/array` `LEDArrMode` table conflicts with firmware modes on remote LEDs, galvo `pause/resume_arbitrary_points` keys not parsed, 265 vs 256 points.
- `can.get_available_devices` waits for 2 replies to a 1-reply command; `can.scan(probe_range=True)` exceeds the 1 s cutoff.
- `get_position` picks step size by list index, returns zeros on timeout; `move_stepper` skips absolute moves when the (stale) Python cache says "already there".
- `use_qid_done=True` waits the full timeout for every endpoint that does not register a qid; user `qid` arguments are overwritten.
- Non-blocking calls sleep 100 ms (+20 ms per write) → ~8 commands/s. No auto-reconnect (dead code after `break`).
- Packaging/docs: pip name `UC2-REST` (README says `uc2rest`), four different licence statements and an empty `LICENSE`, `requests` unused, README lists WiFi, nonexistent methods and examples; `examples/` contains only CAN-OTA scripts, four of them for the removed `/can_ota`.

## 5. uc2canopen

- README/docstrings claim "same API shape as UC2-REST" — it is not (constructor, axis/node addressing, method names, 0- vs 1-based laser channels).
- `NODE` defaults ≠ firmware: `MOT_A` 14 (fw 10), `LED` 20 (fw 30, 20 is the laser board), `GALVO` 30 (fw 40, 30 is the LED board), DIO/analog/collision default node 11 (fw GPIO satellite 60). Every node has the full OD, so wrong-node writes succeed silently.
- `od.py` lists the 21 non-existent objects (8 public methods raise `SdoError 0x06020000`) and lacks `0x2040–0x204B`; claims to be auto-derived.
- `set_enabled`/`configure_tmc` with `axis=0` write sub 1, firmware reads sub 2.
- `start/stop/reset_node()` default to broadcast (see §1 #3).
- `wait_for_idle` can return before motion starts (cached TPDO1); `AsyncUC2Client.move_and_wait` keys by axis but all events carry axis 0 → 60 s wait for axis ≠ 0.
- `Led.set_pattern` writes mode 9 after the pattern id → firmware `setMode` cancels the pattern; modes 6–9 → fill.
- Waveshare framing splits on the first 0x55 byte instead of using DLC (`waveshare_bus.py:242-259`).
- STRING/DOMAIN reads return the 4-byte length as data (`sdo.py:266`); expedited only → no strings, LED pixels, galvo points, OTA.
- Callback exceptions swallowed; `__version__` 0.2.0 vs 0.2.3; CHANGELOG lists features the firmware does not have; `[canopen]` extra unused; `examples/can_sniffer.py` decodes a pre-CANopen protocol.

## 6. ImSwitch defaults

- `UC2CANOpenManager`: `nodeIdA` 14, `ledNodeId` 20, `laserNodeId` 21 → firmware 10, 30, 20.
- `ESP32Manager`: `baudrate` default 115200 → HAT+ configs must set 921600.

## 7. Hardware side findings (from the HAT+ v2 netlist in the docs iBOM)

- MCP2515 crystal Y901 is 12 MHz (LCSC C9002); the HAT+ v2 page said `oscillator=16000000`. Fixed in the docs.
- HAT EEPROM SCL is routed to Pi header pin 26 (GPIO7), not ID_SC (pin 28); likely why the HAT+ EEPROMs "do not work".
- ESP32 CAN pins are TX = GPIO17, RX = GPIO18; the HAT+ v2 and FRAME pages had them swapped. Fixed in the docs.

## 8. Stale documents in the code repos

- uc2-ESP: `DOCS/SerialAPI.md` (`accel`, `isaccel`, encoder keys), `DOCUMENTATION/ESP32_MESSAGEPACK_INTEGRATION.md` (not compiled), `DOCS/OTA_VIA_CAN_README.md` + `cheatsheet.txt` (`/can_ota`, `/can_ota_stream`), `main/json_api*.txt`, `main/docs.txt` (endpoints that no longer exist), `DOCS/CANopen_Network_Architecture.md` (1 Mbit/s, NMT master), `uc2_canopen_flow.html` (old node scheme, 115200), `platformio.ini` node-ID comments, `tools/ota/canopen_ota_serial.py` docstring.
- UC2-REST: `DOCUMENTATION/CAN_OTA_*.md`, README (see §4).
- UC2-REST-CANOPEN: README "no ESP32 master required" without coexistence rules, "TpdoListener background thread".

## Suggested order

1. §1 #1–#4 (memory safety) and #3 (NMT reset → reboot).
2. qid echo on every reply + one ack schema (`{"qid":q,"ok":bool,"error"?}`) → fixes most of uc2rest's matching problems.
3. One OD source of truth with generated EDS/`od.py`, CI diff check; fix registry bit semantics.
4. ~~Master feedback by node ID; routes for more than 4 motors~~ done for multi-axis satellites (uncommitted); single-axis satellites outside 10–13 still need §1 #5.
5. uc2rest: honour `timeout`, raise exceptions instead of returning strings, delete dead modules.
6. uc2canopen: SDO reply matching, unicast NMT, node defaults from the firmware table.

## 9. Release pipeline and web flasher (checked 2026-10-09)

Checked: `build-and-release.yaml` runs, the published `v2026.0.0-beta.4` images in `youseetoo/uc2-esp32-binaries`, the manifests, and the flasher (`youseetoo.github.io`) in headless Chrome. Not checked: flashing real hardware.

| # | Finding | Status |
|---|---|---|
| P1 | Every release since June 2026 is a pre-release. The old flasher's default "Latest Stable" picked `v2026.01.19-3` (pre-CANopen, other board IDs), so every CAN board showed "not available". | flasher fixed: defaults to the newest release that contains the board |
| P2 | ImSwitch-OS lookup: OS `v26.0.0` pins `firmware-image-server:sha-046d478`; the old code matched by date against stable releases only → January firmware. | flasher fixed: resolves the commit via the GitHub compare API → `v2026.0.0-beta.2` |
| P3 | `merge_bin` writes `boot_app0.bin` at a fixed 0xe000. Classic ESP32 tables (master, standalone, WEMOS) have no otadata: the 12-byte OTA-select record lands in the last NVS page (0xe000–0xf000) and the 0xFF padding into phy_init. On the XIAO tables otadata is at 0xd000 (still boots). Probably recovered by NVS (corrupt page erased) — not verified on hardware. | workflow fixed (uncommitted): offset read from the built `partitions.bin`, skipped when there is no otadata |
| P4 | `merge_bin --flash_mode dio --flash_freq 40m --flash_size 4MB` rewrites the bootloader header (compiled: DIO/80m/4MB) → bootloader 40m vs app 80m. Harmless today, but differs from `pio run -t upload` and from `build-frame.yaml`. | workflow fixed: `keep` |
| P5 | All `sdkconfig.*` say `FLASHSIZE_2MB`; partition tables need 4 MB. Works only because PlatformIO patches the header from `board_upload.flash_size`. | open |
| P6 | Web-flasher images are written from 0x0 → NVS (node ID, role, positions) is reset on every web flash. | documented |
| P7 | The public release has only merged images; CAN-OTA needs the app-only `firmware.bin` (only in the FRAME firmware server / local builds). | documented; add app-only assets to the release |
| P8 | Release matrix labels `seeed_xiao_esp32s3` "Xiao ESP32S3 ODMR" (it is the XIAO standalone, `pindef UC2_esp32s3_xiao`). | workflow fixed |
| P9 | Public motor image is node 11 only; per-axis images (`_motA/X/Y/Z`) exist only in the FRAME build. | flasher: Configure tab sets the node ID after flashing |
| P10 | Old flasher "Erase": `flashBegin()` with the stub loader running does not erase the region (3 s timeout, stub erases lazily); "NVS only" used a fixed 0x9000+0x5000 (misses one NVS page on ESP32, hits otadata on XIAO). | flasher fixed: ERASE_REGION in 256 KB chunks, NVS range read from the device's partition table |
| P11 | Old flasher Test/Configure commands: `accel`/`isaccel`, `stop:1` (not `isStop`), `{"task":"/state","get":true}`, `LEDArrMode`/`led_array` (no effect locally), laser id 5, CAN address default 256, CAN `address` key, slave node-ID defaults 10/25/30 (firmware 11/30/40). | flasher fixed |
| P12 | Logo and Arkitekt image URLs on the old pages return 404. | flasher fixed |

Verified OK: all 17 `beta.4` images carry the right `pindef` and the release version string; manifests are reachable with `Access-Control-Allow-Origin: *`; partition tables fit in 4 MB; bootloader offsets (0x1000 ESP32, 0x0 S3) and chip IDs are correct.
