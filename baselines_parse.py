#!/usr/bin/env python3
"""T0.3: build DOCS/baselines.md table from PIO logs + firmware.elf.

Method (documented in DOCS/baselines.md):
  flash used  = size of .pio/build/<env>/firmware.bin (the flashed image)
  flash free  = app-partition size (custom_partition*.csv) - flash used
  IRAM used   = ELF ALLOC sections with VMA in SRAM-IRAM ranges (excludes
                flash-mapped iram0_2_seg / drom0_0_seg)
  RAM used    = ELF ALLOC sections with VMA in DRAM + RTC ranges
  RAM free    = linker-script region capacity (arduino-esp32 memory.ld) - RAM used
"""
import glob, os, re, subprocess, sys

REPO = r"M:\repos\uc2-esp32"
LOGDIR = os.path.join(os.environ["TEMP"], "uc2_baselines")
OBJDUMP = {
    "s3": r"C:\Users\troop\.platformio\packages\toolchain-xtensa-esp32s3\bin\xtensa-esp32s3-elf-objdump.exe",
    "classic": r"C:\Users\troop\.platformio\packages\toolchain-xtensa-esp32\bin\xtensa-esp32-elf-objdump.exe",
}
ENVS = [
    "UC2_2", "UC2_3", "UC2_4", "UC2_WEMOS",
    "seeed_xiao_esp32s3", "seeed_xiao_esp32s3_ledservo", "seeed_xiao_esp32s3_ledring",
    "UC2_canopen_slave_motor_release", "UC2_canopen_slave_accelmotor_release",
    "UC2_canopen_slave_galvo_release", "UC2_canopen_slave_laser_release",
    "UC2_canopen_slave_led_release", "UC2_canopen_slave_gpio_release",
    "UC2_canopen_bridge_ps4_usbhost_release", "UC2_canopen_master_release",
    "UC2_canopen_standalone_v4_release", "waveshare_esp32s3_ledarray",
]

# app partition size per env (from custom_partition*.csv at repo root)
APP_PART = {
    "classic": 0x200000,      # custom_partition.csv: factory app 2M
    "s3":      0x140000,      # custom_partition_esp32s3.csv: app0/app1 0x140000 (OTA)
    "s3w":     0x180000,      # custom_partition_esp32s3_waveshare.csv: 0x180000 (OTA)
}
WAVESHARE = {"waveshare_esp32s3_ledarray"}

# SRAM region ranges (VMA-based) and capacities (arduino-esp32 2.0.x memory.ld)
REGIONS = {
    "classic": {
        # iram0_0_seg + rtc_iram; flash-mapped iram0_2_seg starts at 0x400D0020
        "iram": ((0x40080000, 0x400A0000), (0x400C0000, 0x400D0020)),
        "iram_total": 0x20000 + 0x2000,                                   # 136 KB
        "dram": ((0x3FFBDB5C, 0x40000000), (0x3FF80000, 0x3FFA0000),     # dram0_0_seg + rtc_data
                 (0x50000000, 0x50002000)),                               # + rtc_slow
        "dram_total": 0x2C200 + 0x2000 + 0x2000,                          # 178.5 KB
    },
    "s3": {
        "iram": ((0x40374000, 0x403CC700), (0x600FE000, 0x60100000)),     # iram0_0_seg + rtc_iram
        "iram_total": 0x58700 + 0x2000,                                   # 370.5 KB
        "dram": ((0x3FC88000, 0x3FD00000), (0x50000000, 0x50002000)),     # dram0_0_seg + rtc_slow
        "dram_total": 0x54700 + 0x2000,                                   # 353.5 KB
    },
}

def sections(elf, chip):
    out = subprocess.check_output([OBJDUMP[chip], "-h", elf]).decode(errors="replace")
    lines = out.splitlines()
    res = []
    header = re.compile(r"^\s*(\d+)\s+(\S+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+([0-9a-fA-F]+)\s+\S+\s+\S+\s*$")
    for i, line in enumerate(lines):
        mm = header.match(line)
        if not mm:
            continue
        size, vma = int(mm.group(3), 16), int(mm.group(4), 16)
        # flag line follows the header line; "ALLOC" appears on the next 1-2 lines
        if size and "ALLOC" in "\n".join(lines[i + 1:i + 3]):
            res.append((vma, size, mm.group(2)))
    return res

def used_in(vmas_ranges, secs):
    total = 0
    for vma, size, _ in secs:
        for lo, hi in vmas_ranges:
            if lo <= vma < hi:
                total += size
                break
    return total

def main():
    rows = []
    for env in ENVS:
        log = os.path.join(LOGDIR, env + ".log")
        text = open(log, "r", errors="replace").read()
        hw = re.search(r"HARDWARE:\s+(\S+)\s+[\d.]+MHz", text)
        if not hw:
            print(f"!! no HARDWARE line for {env}", file=sys.stderr); continue
        chip = "s3" if "S3" in hw.group(1) else "classic"
        part = "s3w" if env in WAVESHARE else chip

        build = os.path.join(REPO, ".pio", "build", env)
        elf = os.path.join(build, "firmware.elf")
        binf = os.path.join(build, "firmware.bin")
        flash_used = os.path.getsize(binf)
        app = APP_PART[part]

        R = REGIONS[chip]
        secs = sections(elf, chip)
        iram_used = used_in(R["iram"], secs)
        dram_used = used_in(R["dram"], secs)

        rows.append((env, chip, flash_used, app - flash_used,
                     dram_used, R["dram_total"] - dram_used, iram_used, app))
        print(f"{env} ({chip}): flash {flash_used}/{app}, "
              f"ram {dram_used}/{R['dram_total']}, iram {iram_used}/{R['iram_total']}")

    print("\n| env | flash used | flash free | RAM used | RAM free | IRAM used |")
    print("|-----|-----------:|-----------:|---------:|---------:|----------:|")
    for env, chip, fu, ff, ru, rf, ir, app in rows:
        print(f"| {env} | {fu} ({fu/1024:.1f} KB) | {ff} ({ff/1024:.1f} KB) | "
              f"{ru} ({ru/1024:.1f} KB) | {rf} ({rf/1024:.1f} KB) | {ir} ({ir/1024:.1f} KB) |")

if __name__ == "__main__":
    main()
