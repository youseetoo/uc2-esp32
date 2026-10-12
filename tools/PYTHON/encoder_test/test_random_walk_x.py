#!/usr/bin/env python3
"""
X-axis open-loop random walk test: random moves inside a safe window, then
back to start.

From the start position (where the axis is when the script starts, unless
--start is given) the axis makes --moves random moves. Every move has a
random direction and a random length between --min-move and --max-move
steps, and every target stays inside start +/- --range (the safe window),
so the effective longest move is 2 * --range. All moves are open loop unless
--mode is given (1 = monitor, 2 = correct, 3 = servo; needs a valid axis
calibration). In modes 2 and 3 the firmware re-zeroes the encoder after
every move, so compare "measured"/"commanded" there, not rawCounts.

The whole sequence is generated up front from --seed, so the same seed with
the same --start/--range/--moves/--min-move/--max-move/--dist gives exactly
the same moves again. Without --seed a seed is picked, printed and written
to the CSV header.

Backlash: the start position is approached from the same side (--approach)
and over the same distance (--preload) before the BEFORE photo and before
the AFTER photo, so backlash is the same in both photos.

Take a photo of the calibration slide before and after the run (the script
waits for Enter before the run) and compare them by cross correlation, as
part 6.5 of encoder_test.ipynb does for two photos, to get the accumulated
open-loop error (the xmas tree part of that notebook was taken out on
2026-10-08: it is part 7 of encoder_test.ipynb in commit be53de2).

Calibration: modes 1..3 only work with a valid encoder calibration in the
firmware (sign, counts per step, backlash), otherwise "measured"/"posErr"
are wrong and CORRECT/SERVO move the axis to the wrong place. So with
--mode 1..3 the script first runs the firmware's calibration
({"stepperid": 1, "calibrate": 1}: the axis sweeps +55000 steps and back,
the result is stored in the board), --no-calibrate skips it, --calibrate
also does it for an open-loop run. After that the first leg is always run
open loop as a scale check: if the encoder does not see about as many steps
as were commanded, a closed-loop run is aborted.

With --camera the photos are taken automatically with the Hikrobot camera
(needs the MVS SDK installed, and the MVS client must not have the camera
open) and saved as <csv name>_photos/0_before.bmp and 1_after.bmp; the
script then does not wait for Enter. Exposure, gain etc. are used as they
are set in the camera. --photo-test only takes one photo, to check the
camera and the picture, and does not move the axis.

Connect to the USB port of the node where the X axis + encoder are LOCAL.

Usage:
    uv run tools/PYTHON/encoder_test/test_random_walk_x.py --port COM7 --speed 100000 --accel 1000000 --moves 200 --seed 42
    uv run tools/PYTHON/encoder_test/test_random_walk_x.py --port COM7 --speed 100000 --accel 1000000 --moves 200 --seed 42 --camera
    uv run tools/PYTHON/encoder_test/test_random_walk_x.py --photo-test
    uv run tools/PYTHON/encoder_test/test_random_walk_x.py --moves 200 --seed 42 --dry-run

Output: the table is printed and also saved as CSV, one row per move end
(same columns as test_xmas_tree_x.py).
Lines starting with '#' are run metadata (pandas: read_csv(..., comment='#')).
"""
import argparse
import csv
import ctypes
import json
import math
import os
import random
import re
import sys
import time
from datetime import datetime

import serial

# results (CSV, photos) go to <repo>/encoder_test_data, which git ignores
DATA_DIR = os.path.normpath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "encoder_test_data"))

LOG = re.compile(r"axis 1: commanded=(-?\d+) measured=(-?\d+) posErr=(-?\d+) "
                 r"rawCounts=(-?\d+)")
# watchdog trip / latched fault / async event (DIVERGENCE only warns, the rest latch)
FAULT = re.compile(r'refused: fault \d+ latched|AxisWatchdog: \w+ trip|"axisEvent"')

# calibration result, as logged by AxisCalibration.cpp / AxisController.cpp
CAL_SCALE = re.compile(r"SCALE: countsPerStep=([\d.]+) \(q16=-?\d+\) sign=(-?\d+) "
                       r"R2=([\d.]+) scatter=(\d+) q=(\d+)")
CAL_BACKLASH = re.compile(r"BACKLASH: (-?\d+) counts")
CAL_END = re.compile(r"calibrateAxis \d+ (OK|FAILED)")

MODES = {0: "OPEN_LOOP", 1: "MONITOR", 2: "CORRECT", 3: "SERVO"}
# CORRECT / SERVO adopt the encoder position as the new step counter, so the
# move ends near the target, not exactly on it
ARRIVE_TOL = 200
# the firmware's calibration sweeps preload 5000 + 10 x 5000 steps in + and back
CAL_SWEEP = 55000


def send(ser, stepper):
    ser.write((json.dumps({"task": "/motor_act",
                           "motor": {"steppers": [stepper]}}) + "\n").encode())


def read_position(ser, timeout=5.0):
    """Wait for the next feedback log line, return (commanded, measured, posErr, rawCounts)."""
    t_end = time.time() + timeout
    while time.time() < t_end:
        m = LOG.search(ser.readline().decode(errors="ignore"))
        if m:
            return tuple(int(v) for v in m.groups())
    raise TimeoutError("no 'axis 1: commanded=...' log line from the board")


def axis_status(ser, timeout=2.0, flush=True):
    """/motor_get of axis 1 as a dict: position, isRunning, measured, posErr,
    axismode, health, fault, calibrated, rawCounts (what the firmware sends)."""
    if flush:
        ser.reset_input_buffer()
    ser.write((json.dumps({"task": "/motor_get",
                           "motor": {"steppers": [{"stepperid": 1}]}}) + "\n").encode())
    text = ""
    t_end = time.time() + timeout
    while time.time() < t_end:
        text += ser.readline().decode(errors="ignore")
        if '"rawCounts"' in text:
            break
    return {k: int(float(v)) for k, v in re.findall(r'"(\w+)"\s*:\s*(-?\d+(?:\.\d+)?)', text)}


def calibrate(ser, timeout=90.0):
    """Run the firmware's encoder calibration of axis 1 (sign, counts per step,
    backlash). The axis sweeps +55000 steps from where it is and comes back.
    The result is taken from the firmware's log lines (its long JSON replies
    do not arrive complete on the USB serial port)."""
    ser.reset_input_buffer()
    send(ser, {"stepperid": 1, "calibrate": 1})
    cal = {}
    t_end = time.time() + timeout
    while time.time() < t_end:
        line = ser.readline().decode(errors="ignore")
        m = CAL_SCALE.search(line)
        if m:
            cps = float(m.group(1))
            cal.update(countsPerStep=cps, stepsPerCount=1 / cps, countSign=int(m.group(2)),
                       r2=float(m.group(3)), residualScatter=int(m.group(4)),
                       quality=int(m.group(5)))
        m = CAL_BACKLASH.search(line)
        if m and cal:
            cal.update(backlashCounts=int(m.group(1)),
                       backlashSteps=round(abs(int(m.group(1))) / cal["countsPerStep"]))
        m = CAL_END.search(line)
        if m:
            if m.group(1) != "OK" or "backlashSteps" not in cal:
                raise RuntimeError(f"axis calibration failed: {line.strip()}")
            return cal
    raise TimeoutError("no calibration result from the board")


def pos_err(ser):
    """Current posErr, from the log line or (if posErr does not change) /motor_get."""
    try:
        return read_position(ser, 2.0)[2]
    except TimeoutError:
        return axis_status(ser).get("posErr", 0)


def move_to(ser, target, speed, accel, mode=0, timeout=60.0):
    """Absolute move in the given axis mode, wait until arrived, return the last log line values."""
    ser.reset_input_buffer()
    stepper = {"stepperid": 1, "position": target, "speed": speed,
               "isabs": 1, "closedloop": mode}
    if accel is not None:
        # DeviceRouter builds read "acceleration", the legacy parser reads "accel"
        stepper["acceleration"] = accel
        stepper["accel"] = accel
    send(ser, stepper)
    tol = ARRIVE_TOL if mode >= 2 else 0
    settle = 2.0 if mode >= 2 else 1.0  # corrections / adoption follow the move
    arrived = None
    last = None
    t_end = time.time() + timeout
    t_line = time.time()
    while time.time() < t_end:
        line = ser.readline().decode(errors="ignore")
        if arrived is None and time.time() - t_line > 2.0:
            # the firmware only logs when posErr changes, so with a good
            # calibration there may be no log line at the target: ask instead
            st = axis_status(ser, 1.0, flush=False)
            t_line = time.time()
            if st.get("fault") in (1, 2):
                raise RuntimeError(f"axis fault {st['fault']} during move to {target}\n"
                                   'clear it with {"stepperid": 1, "axisreset": 1}')
            if st.get("isRunning") == 0 and abs(st.get("position", target + tol + 1) - target) <= tol:
                return (st["position"], st.get("measured", st["position"]),
                        st.get("posErr", 0), st.get("rawCounts", 0))
            continue
        if FAULT.search(line):
            if "DIVERGENCE" in line:
                print(f"    ! {line.strip()}")
            else:
                raise RuntimeError(f"axis fault during move to {target}: {line.strip()}\n"
                                   'clear it with {"stepperid": 1, "axisreset": 1}')
        m = LOG.search(line)
        if m:
            t_line = time.time()
            vals = tuple(int(v) for v in m.groups())
            if abs(vals[0] - target) <= tol:
                if arrived is None or (last and vals[0] != last[0]):
                    arrived = time.time()  # (re)start settling while it still moves
            else:
                arrived = None
            last = vals
        if arrived and time.time() - arrived > settle:  # let it settle
            return last
    raise TimeoutError(f"move to {target} did not finish")


def make_targets(rng, start, lo, hi, n, min_move, max_move, dist):
    """Random walk inside lo..hi, returns [(move, absolute target), ...]."""
    targets = []
    pos = start
    for _ in range(n):
        # directions that still have room for at least min_move
        options = []
        for direction in (-1, 1):
            room = pos - lo if direction < 0 else hi - pos
            longest = min(max_move, room)
            if longest >= min_move:
                options.append((direction, longest))
        direction, longest = rng.choice(options)
        if dist == "log":
            length = round(math.exp(rng.uniform(math.log(min_move), math.log(longest))))
            length = max(min_move, min(longest, length))
        else:
            length = rng.randint(min_move, longest)
        pos += direction * length
        targets.append((direction * length, pos))
    return targets


class Camera:
    """First Hikrobot camera found, through the MVS SDK's Python wrapper."""

    def __init__(self, gain=None):
        sdk = os.getenv("MVCAM_COMMON_RUNENV")
        if not sdk:
            raise RuntimeError("MVCAM_COMMON_RUNENV is not set - is the Hikrobot MVS SDK installed?")
        sys.path.append(os.path.join(sdk, "Samples", "Python", "MvImport"))
        import MvCameraControl_class as mv
        self.mv = mv
        self.cam = None
        mv.MvCamera.MV_CC_Initialize()
        devices = mv.MV_CC_DEVICE_INFO_LIST()
        self.check(mv.MvCamera.MV_CC_EnumDevices(mv.MV_GIGE_DEVICE | mv.MV_USB_DEVICE, devices),
                   "enum devices")
        if devices.nDeviceNum == 0:
            raise RuntimeError("no Hikrobot camera found")
        info = ctypes.cast(devices.pDeviceInfo[0], ctypes.POINTER(mv.MV_CC_DEVICE_INFO)).contents
        cam = mv.MvCamera()
        self.check(cam.MV_CC_CreateHandle(info), "create handle")
        self.cam = cam
        self.check(cam.MV_CC_OpenDevice(mv.MV_ACCESS_Exclusive, 0),
                   "open device (is it still open in the MVS client?)")
        # free running, otherwise no frame arrives without a trigger
        self.check(cam.MV_CC_SetEnumValue("TriggerMode", mv.MV_TRIGGER_MODE_OFF), "trigger mode off")
        if gain is not None:
            cam.MV_CC_SetEnumValueByString("GainAuto", "Off")  # not every model has it
            self.check(cam.MV_CC_SetFloatValue("Gain", gain), f"set gain {gain}")

    @staticmethod
    def check(ret, what):
        if ret != 0:
            raise RuntimeError(f"camera: {what} failed, ret = 0x{ret:x}")

    def photo(self, path, skip=2):
        """Save a frame taken from now on; the type follows the file extension."""
        mv, cam = self.mv, self.cam
        image_type = {".bmp": mv.MV_Image_Bmp, ".png": mv.MV_Image_Png,
                      ".jpg": mv.MV_Image_Jpeg, ".tif": mv.MV_Image_Tif}[os.path.splitext(path)[1]]
        self.check(cam.MV_CC_StartGrabbing(), "start grabbing")
        try:
            # the first frames after the start are skipped (auto exposure settles)
            for i in range(skip + 1):
                frame = mv.MV_FRAME_OUT()
                ctypes.memset(ctypes.byref(frame), 0, ctypes.sizeof(frame))
                self.check(cam.MV_CC_GetImageBuffer(frame, 5000), "get frame")
                try:
                    if i < skip:
                        continue
                    image = mv.MV_CC_IMAGE()
                    ctypes.memset(ctypes.byref(image), 0, ctypes.sizeof(image))
                    image.nWidth = frame.stFrameInfo.nExtendWidth
                    image.nHeight = frame.stFrameInfo.nExtendHeight
                    image.enPixelType = frame.stFrameInfo.enPixelType
                    image.pImageBuf = frame.pBufAddr
                    image.nImageBufSize = frame.stFrameInfo.nFrameLenEx
                    image.nImageLen = frame.stFrameInfo.nFrameLenEx
                    param = mv.MV_CC_SAVE_IMAGE_PARAM()
                    ctypes.memset(ctypes.byref(param), 0, ctypes.sizeof(param))
                    param.enImageType = image_type
                    param.iMethodValue = 1
                    param.nQuality = 99  # jpg only
                    self.check(cam.MV_CC_SaveImageToFileEx2(image, param, path), f"save {path}")
                finally:
                    cam.MV_CC_FreeImageBuffer(frame)
        finally:
            cam.MV_CC_StopGrabbing()
        print(f"photo saved to {path}")

    def close(self):
        if self.cam:
            self.cam.MV_CC_CloseDevice()
            self.cam.MV_CC_DestroyHandle()
        self.mv.MvCamera.MV_CC_Finalize()


def keep_awake(on):
    """Stop Windows from going to sleep on idle while the run is going."""
    if sys.platform == "win32":
        ES_CONTINUOUS, ES_SYSTEM_REQUIRED = 0x80000000, 0x00000001
        ctypes.windll.kernel32.SetThreadExecutionState(
            ES_CONTINUOUS | (ES_SYSTEM_REQUIRED if on else 0))


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument("--port", default=None)
    ap.add_argument("--start", type=int, default=None,
                    help="start/end position (default: where the axis is now)")
    ap.add_argument("--range", type=int, default=100000,
                    help="safe window: targets stay inside start +/- this")
    ap.add_argument("--moves", type=int, default=100, help="number of random moves")
    ap.add_argument("--min-move", type=int, default=10, help="shortest move")
    ap.add_argument("--max-move", type=int, default=500000,
                    help="longest move (also limited by the safe window)")
    ap.add_argument("--dist", choices=("uniform", "log"), default="uniform",
                    help="move length distribution: uniform (default, mostly "
                         "long moves) or log (as many short as long moves)")
    ap.add_argument("--seed", type=int, default=None,
                    help="random seed (default: picked and printed)")
    ap.add_argument("--mode", type=int, choices=sorted(MODES), default=0,
                    help="axis mode for every move: 0 = open loop (default), "
                         "1 = monitor (warn only), 2 = correct, 3 = servo")
    ap.add_argument("--calibrate", action=argparse.BooleanOptionalAction, default=None,
                    help="run the firmware's encoder calibration first (default: "
                         "yes if --mode is 1..3, --no-calibrate to skip)")
    ap.add_argument("--speed", type=int, default=10000)
    ap.add_argument("--accel", type=int, default=100000,
                    help="acceleration for all X moves")
    ap.add_argument("--approach", type=int, choices=(-1, 1), default=1,
                    help="side the start is approached from, before both "
                         "photos (1 = from start + preload)")
    ap.add_argument("--preload", type=int, default=2000,
                    help="approach the start from this far away")
    ap.add_argument("--camera", action="store_true",
                    help="take the BEFORE / AFTER photos with the Hikrobot "
                         "camera instead of waiting for Enter")
    ap.add_argument("--gain", type=float, default=None,
                    help="camera gain in dB (default: as set in the camera)")
    ap.add_argument("--photo-format", choices=("bmp", "png", "jpg", "tif"), default="bmp")
    ap.add_argument("--photo-delay", type=float, default=2.0,
                    help="seconds to wait after the move before a photo")
    ap.add_argument("--photo-test", action="store_true",
                    help="only take one photo and exit, the axis is not moved")
    ap.add_argument("--dry-run", action="store_true",
                    help="only print the moves, do not open the port "
                         "(start = 0 unless --start is given)")
    ap.add_argument("--out", default=None, help="CSV file (default: timestamped name in encoder_test_data/)")
    args = ap.parse_args()
    if args.moves < 1:
        ap.error("--moves must be >= 1")
    if not 1 <= args.min_move <= args.max_move:
        ap.error("need 1 <= --min-move <= --max-move")
    if args.min_move > args.range:
        ap.error("--min-move does not fit into the safe window")
    if not 0 < args.preload <= args.range:
        ap.error("--preload must be > 0 and inside the safe window")
    if args.photo_test:
        camera = Camera(args.gain)
        try:
            camera.photo(os.path.join(DATA_DIR, f"photo_test_{datetime.now():%Y%m%d_%H%M%S}.{args.photo_format}"))
        finally:
            camera.close()
        return
    if not args.dry_run and not args.port:
        ap.error("--port is required")
    if args.calibrate is None:
        args.calibrate = args.mode > 0
    if args.seed is None:
        args.seed = random.SystemRandom().randrange(2 ** 32)
    out = args.out or os.path.join(DATA_DIR, f"random_walk_x_{datetime.now():%Y%m%d_%H%M%S}.csv")

    ser = None
    camera = None
    current = 0
    if not args.dry_run:
        if args.camera:
            camera = Camera(args.gain)  # before anything moves, so a camera problem shows up early
            photos = os.path.splitext(out)[0] + "_photos"
            os.makedirs(photos, exist_ok=True)
        ser = serial.Serial(args.port, 115200, timeout=0.2)
        time.sleep(2.0)
        status = axis_status(ser)
        print(f"axis 1 status: {status}")
        try:
            current = read_position(ser, 3.0)[0]
            status.setdefault("calibrated", 1)  # only logged with a calibration
        except TimeoutError:  # no log line while posErr does not change
            if "position" not in status:
                raise
            current = status["position"]
    if args.start is None:
        args.start = current

    lo, hi = args.start - args.range, args.start + args.range
    targets = make_targets(random.Random(args.seed), args.start, lo, hi,
                           args.moves, args.min_move, args.max_move, args.dist)
    pre = args.start + args.approach * args.preload
    # nothing leaves the safe window, whatever the options were
    assert all(lo <= t <= hi for _, t in targets) and lo <= pre <= hi

    print(f"current = {current}, start = {args.start}, safe window {lo} .. {hi}, "
          f"{len(targets)} moves, seed = {args.seed}, dist = {args.dist}, "
          f"used range {min(t for _, t in targets)} .. {max(t for _, t in targets)}, "
          f"total travel {sum(abs(m) for m, _ in targets)}, speed = {args.speed}, "
          f"accel = {args.accel or 'default'}, mode = {args.mode} ({MODES[args.mode]})")

    if args.dry_run:
        print(f"\n{'leg':>7} {'move':>8} {'target':>10}")
        for i, (move, target) in enumerate(targets, 1):
            print(f"{i:>7} {move:>8} {target:>10}")
        return

    if not lo <= current <= hi:
        raise SystemExit(f"axis is at {current}, outside the safe window {lo} .. {hi}")

    def timeout_for(distance):
        return 30 + 3 * abs(distance) / args.speed

    cal = None
    if args.calibrate:
        if current + CAL_SWEEP > hi:
            raise SystemExit(f"the calibration sweep (+{CAL_SWEEP} steps from {current}) "
                             f"leaves the safe window {lo} .. {hi}")
        print("calibrating the encoder (the axis sweeps +55000 steps and back) ...")
        cal = calibrate(ser)
        print(f"calibration ok: countsPerStep = {cal['countsPerStep']:.5f} "
              f"(stepsPerCount = {cal['stepsPerCount']:.3f}), countSign = {cal['countSign']}, "
              f"backlash = {cal['backlashSteps']} steps, scatter = {cal['residualScatter']} counts, "
              f"quality = {cal['quality']}, R2 = {cal['r2']}")
        status = axis_status(ser)
        status.setdefault("calibrated", 1)
    if status.get("fault") == 5:
        print("WARNING: the firmware reports CAL_INVALID - its calibration was only "
              "rescaled after a microstep change, run with --calibrate")
    if args.mode and not status.get("calibrated"):
        raise SystemExit("axis 1 is not calibrated: the firmware would run every move "
                         "open loop. Run with --calibrate")

    if args.mode:
        # the firmware's verify/correct step follows the axis's configured mode,
        # not the per-move "closedloop" override, so set both
        send(ser, {"stepperid": 1, "axismode": args.mode})
        time.sleep(0.5)

    keep_awake(True)
    try:
        with open(out, "w", newline="") as f:
            f.write(f"# test=random_walk_x port={args.port} "
                    f"started={datetime.now().isoformat(timespec='seconds')}\n")
            f.write(f"# start={args.start} range={args.range} moves={args.moves} "
                    f"min_move={args.min_move} max_move={args.max_move} "
                    f"dist={args.dist} seed={args.seed} speed={args.speed} "
                    f"accel={args.accel} approach={args.approach} "
                    f"preload={args.preload} mode={args.mode} "
                    f"camera={int(args.camera)}\n")
            if cal:
                f.write("# calibration " + " ".join(
                    f"{k}={cal[k]}" for k in ("countsPerStep", "countSign", "backlashSteps",
                                              "residualScatter", "quality", "r2")) + "\n")
            w = csv.writer(f)
            w.writerow(["time_iso", "speed", "cycle", "leg", "move", "target",
                        "commanded", "rawCounts", "measured", "posErr"])

            def go(cycle, leg, target, mode=args.mode):
                move = target - prev[0]
                commanded, measured, pos_err, raw = move_to(
                    ser, target, args.speed, args.accel, mode,
                    timeout_for(move))
                prev[0] = target
                print(f"{cycle:>5} {leg:>7} {move:>8} {target:>10} {commanded:>10} "
                      f"{measured:>10} {raw:>10}")
                w.writerow([datetime.now().isoformat(timespec="milliseconds"),
                            args.speed, cycle, leg, move, target,
                            commanded, raw, measured, pos_err])
                f.flush()
                return pos_err

            print(f"\n{'cycle':>5} {'leg':>7} {'move':>8} {'target':>10} "
                  f"{'commanded':>10} {'measured':>10} {'rawCounts':>10}")
            prev = [current]
            # scale check: the first leg is always open loop, and the encoder
            # must see (about) as many steps as were commanded
            err0 = pos_err(ser)
            drift = go(0, "preload", pre, mode=0) - err0
            limit = 0.1 * abs(pre - current) + (cal["backlashSteps"] if cal else 0) + 50
            if status.get("calibrated") and abs(drift) > limit:
                msg = (f"encoder scale check failed: after {pre - current} open-loop steps "
                       f"posErr changed by {drift} (limit {limit:.0f}) - the firmware's "
                       f"calibration does not fit the encoder, run with --calibrate")
                if args.mode:
                    raise SystemExit(msg)
                print(f"WARNING: {msg}; 'measured' and 'posErr' are not valid in this run")
            go(0, "start", args.start)
            if camera:
                time.sleep(args.photo_delay)
                camera.photo(os.path.join(photos, f"0_before.{args.photo_format}"))
            else:
                input("\n>>> take the BEFORE photo, then press Enter to start the run ")

            for i, (_, target) in enumerate(targets, 1):
                go(1, i, target)
            # same last leg as before the BEFORE photo
            go(1, "preload", pre)
            go(1, "return", args.start)
            if camera:
                time.sleep(args.photo_delay)
                camera.photo(os.path.join(photos, f"1_after.{args.photo_format}"))
            else:
                print("\n>>> take the AFTER photo now")
    finally:
        keep_awake(False)
        ser.close()
        if camera:
            camera.close()

    print(f"raw data saved to {out}")
    if args.mode:
        print(f"note: axis 1 is left in axismode {args.mode} ({MODES[args.mode]})")


if __name__ == "__main__":
    main()
