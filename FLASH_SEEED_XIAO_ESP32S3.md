# Local Build, Merge & Flash — `seeed_xiao_esp32s3_can_slave_motor`

This guide covers how to reproduce **locally** what the CI does for the
`seeed_xiao_esp32s3_can_slave_motor` target:

1. Build with PlatformIO
2. Merge all flash regions into a single `_merged.bin`
3. Erase the ESP32-S3 and flash the merged binary with esptool

---

## Prerequisites

```bash
pip install platformio esptool
```

Make sure your device is visible, e.g.:

```
/dev/cu.usbmodem101   # macOS
/dev/ttyACM0          # Linux
```

---

## 1 — Build with PlatformIO

From the repository root:

```bash
pio run --environment seeed_xiao_esp32s3_can_slave_motor
```

Artifacts land in `.pio/build/seeed_xiao_esp32s3_can_slave_motor/`:

| File | Description |
|---|---|
| `firmware.bin` | Application image only |
| `bootloader.bin` | Second-stage bootloader |
| `partitions.bin` | Partition table (from `custom_partition_esp32s3.csv`) |

---

## 2 — Locate `boot_app0.bin`

PlatformIO ships `boot_app0.bin` inside the framework package.
Find it once and remember the path:

```bash
find ~/.platformio -name "boot_app0.bin" | head -1
# typical result:
# ~/.platformio/packages/framework-arduinoespressif32/tools/partitions/boot_app0.bin
```

Set a variable for convenience:

```bash
BOOT_APP0="$(find ~/.platformio -name "boot_app0.bin" | head -1)"
```

---

## 3 — Merge into a single binary

The **ESP32-S3 flash map** used by this target
(see [`custom_partition_esp32s3.csv`](custom_partition_esp32s3.csv)):

| Region | Address | Source file |
|---|---|---|
| Bootloader | `0x0000` | `.pio/build/.../bootloader.bin` |
| Partition table | `0x8000` | `.pio/build/.../partitions.bin` |
| OTA data (`boot_app0`) | `0xe000` | `$BOOT_APP0` |
| Application | `0x10000` | `.pio/build/.../firmware.bin` |

> **Note:** The bootloader address for ESP32-S3 is `0x0000` (not `0x1000` as on classic ESP32).

Run the merge:

```bash
ENV="seeed_xiao_esp32s3_can_slave_motor"
BIN_DIR=".pio/build/$ENV"
BOOT_APP0="$(find ~/.platformio -name "boot_app0.bin" | head -1)"

/Users/bene/.platformio/penv/bin/python -m esptool \
  --chip esp32s3 \
  merge_bin \
  -o "esp32_${ENV}_merged.bin" \
  --flash_mode dio \
  --flash_freq 40m \
  --flash_size 4MB \
  0x0000  "$BIN_DIR/bootloader.bin" \
  0x8000  "$BIN_DIR/partitions.bin" \
  0xe000  "$BOOT_APP0" \
  0x10000 "$BIN_DIR/firmware.bin"
```

Output: `esp32_seeed_xiao_esp32s3_can_slave_motor_merged.bin`

---

## 4 — Erase the chip

Before flashing a merged binary it is recommended to erase the full flash so
no leftover OTA state or NVS data interferes:

```bash
/Users/bene/.platformio/penv/bin/python -m esptool \
  --chip esp32s3 \
  --port /dev/cu.usbmodem101 \
  --baud 460800 \
  erase_flash
```

> Adjust `--port` to match your system (`/dev/ttyACM0` on Linux,
> `COM3` on Windows).

---

## 5 — Flash the merged binary

```bash
/Users/bene/.platformio/penv/bin/python -m esptool \
  --chip esp32s3 \
  --port /dev/cu.usbmodem101 \
  --baud 460800 \
  write_flash \
  --flash_mode dio \
  --flash_freq 40m \
  --flash_size 4MB \
  0x0 "esp32_seeed_xiao_esp32s3_can_slave_motor_merged.bin"
```

The single file already contains everything at the right offsets, so
`--addr 0x0` is all that is needed.

---

## All-in-one script (copy & paste)

```bash
#!/usr/bin/env bash
set -euo pipefail

PORT="/dev/cu.usbmodem101"   # <-- adjust
BAUD=460800
ENV="seeed_xiao_esp32s3_can_slave_motor"
BIN_DIR=".pio/build/$ENV"
OUT="esp32_${ENV}_merged.bin"

# 1. Build
pio run --environment "$ENV"

# 2. Locate boot_app0.bin
BOOT_APP0="$(find ~/.platformio -name "boot_app0.bin" | head -1)"
echo "boot_app0.bin: $BOOT_APP0"

# 3. Merge
python -m esptool \
  --chip esp32s3 \
  merge_bin \
  -o "$OUT" \
  --flash_mode dio \
  --flash_freq 40m \
  --flash_size 4MB \
  0x0000  "$BIN_DIR/bootloader.bin" \
  0x8000  "$BIN_DIR/partitions.bin" \
  0xe000  "$BOOT_APP0" \
  0x10000 "$BIN_DIR/firmware.bin"

echo "Merged binary: $OUT ($(du -sh "$OUT" | cut -f1))"

# 4. Erase
python -m esptool --chip esp32s3 --port "$PORT" --baud "$BAUD" erase_flash

# 5. Flash
python -m esptool \
  --chip esp32s3 \
  --port "$PORT" \
  --baud "$BAUD" \
  write_flash \
  --flash_mode dio \
  --flash_freq 40m \
  --flash_size 4MB \
  0x0 "$OUT"

echo "Done. Reset the board."
```

---

## Troubleshooting

| Symptom | Likely cause | Fix |
|---|---|---|
| `invalid header: 0xefbdbfef` boot loop after flashing CI binary | CI chip detection returned uppercase `ESP32S3`; case-sensitive bash match fell through to `esp32` / `0x1000` bootloader offset instead of `esp32s3` / `0x0000` | Fixed in CI via `CHIP_LOWER="${CHIP,,}"` — always use the locally-merged binary as reference |
| `No serial data received` during erase/flash | Wrong port or board not in download mode | Hold **BOOT**, tap **RESET**, release **BOOT** |
| Guru Meditation / boot loop | Flash mode mismatch | Double-check `--flash_mode dio` matches the bootloader |
| App not starting after OTA | Stale `otadata` from old OTA run | Erase flash and reflash merged binary |
| `boot_app0.bin` not found | PlatformIO packages not yet downloaded | Run `pio run` once first so packages are fetched |
