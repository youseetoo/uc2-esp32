#!/usr/bin/env python3
"""
X-axis 24 hour test: go back and forth between two positions and take a
photo at each visit.

Two absolute positions are given: --slide-pos (the calibration slide) and
--sample-pos (the sample). Every --interval seconds
(default 10 min) the axis moves to the other position and a photo is taken
there, for --hours hours (default 24): calibration slide, sample, calibration slide, sample, ...
With the defaults that is 145 photos, 73 of the calibration slide and 72 of the sample.

All moves are done in --mode 2 (CORRECT) by default (0 = open loop,
1 = monitor, 3 = servo). In modes 2 and 3 the firmware adopts the encoder
position as the new step counter after every move, so compare
"measured"/"commanded" there, not rawCounts.

Calibration: modes 1..3 only work with a valid encoder calibration in the
firmware (sign, counts per step, backlash), otherwise "measured"/"posErr"
are wrong and CORRECT/SERVO move the axis to the wrong place. So with
--mode 1..3 the script first runs the firmware's calibration
({"stepperid": 1, "calibrate": 1}: the axis sweeps +55000 steps from where it
is and back, the result is stored in the board), --no-calibrate skips it.
After that the first leg (to the sample position) is run open loop as a
scale check: if the encoder does not see about as many steps as were
commanded, a closed-loop run is aborted.

Backlash: the calibration slide position is always approached from the sample position
and the other way round, also the very first time, so every photo of one
position is taken after the same leg.

The photos are taken with the Hikrobot camera (needs the MVS SDK installed,
and the MVS client must not have the camera open) and saved as
<csv name>_photos/calibration_slide/0000_calibration_slide_<time>.png and .../sample/0001_sample_<time>.png,
the first photo of a place is the reference for the later ones (how two
photos are compared: part 6 of encoder_test.ipynb). Exposure, gain etc. are used as they are set in the
camera unless --gain is given. --photo-test only takes one photo, to check
the camera and the picture, and does not move the axis.
A full-size png is about 30 MB, so 24 h need about 5 GB of disk space.

The PC is kept awake while the script runs. Stop early with Ctrl+C.

Connect to the USB port of the node where the X axis + encoder are LOCAL.

Usage:
    uv run tools/PYTHON/encoder_test/24hrstest.py --photo-test --gain 23
    uv run tools/PYTHON/encoder_test/24hrstest.py --port COM7 --slide-pos 51793 --sample-pos 91793 --speed 100000 --accel 1000000 --gain 23
    uv run tools/PYTHON/encoder_test/24hrstest.py --port COM7 --slide-pos 51793 --sample-pos 91793 --interval 60 --hours 0.1

Output: the table is printed and also saved as CSV, one row per visit.
Lines starting with '#' are run metadata (pandas: read_csv(..., comment='#')).
"""
import argparse
import csv
import ctypes
import json
import os
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
    ap.add_argument("--slide-pos", type=int, default=None,
                    help="absolute position of the calibration slide (steps)")
    ap.add_argument("--sample-pos", type=int, default=None,
                    help="absolute position of the sample (steps)")
    ap.add_argument("--interval", type=float, default=600,
                    help="seconds between two visits (default 600 = 10 min)")
    ap.add_argument("--hours", type=float, default=24, help="length of the test")
    ap.add_argument("--limit", type=int, default=200000,
                    help="refuse positions farther than this from where the "
                         "axis is now (guards against a typo)")
    ap.add_argument("--mode", type=int, choices=sorted(MODES), default=2,
                    help="axis mode for every move: 0 = open loop, 1 = monitor "
                         "(warn only), 2 = correct (default), 3 = servo")
    ap.add_argument("--calibrate", action=argparse.BooleanOptionalAction, default=None,
                    help="run the firmware's encoder calibration first (default: "
                         "yes if --mode is 1..3, --no-calibrate to skip)")
    ap.add_argument("--speed", type=int, default=10000)
    ap.add_argument("--accel", type=int, default=100000,
                    help="acceleration for all X moves")
    ap.add_argument("--gain", type=float, default=None,
                    help="camera gain in dB (default: as set in the camera)")
    ap.add_argument("--photo-format", choices=("bmp", "png", "jpg", "tif"), default="png")
    ap.add_argument("--photo-delay", type=float, default=2.0,
                    help="seconds to wait after the move before a photo")
    ap.add_argument("--photo-test", action="store_true",
                    help="only take one photo and exit, the axis is not moved")
    ap.add_argument("--out", default=None, help="CSV file (default: timestamped name in encoder_test_data/)")
    args = ap.parse_args()
    if args.photo_test:
        camera = Camera(args.gain)
        try:
            path = os.path.join(DATA_DIR, f"photo_test_{datetime.now():%Y%m%d_%H%M%S}.{args.photo_format}")
            camera.photo(path)
            print(f"photo saved to {path}")
        finally:
            camera.close()
        return
    if not args.port or args.slide_pos is None or args.sample_pos is None:
        ap.error("--port, --slide-pos and --sample-pos are required")
    if args.slide_pos == args.sample_pos:
        ap.error("--slide-pos and --sample-pos are the same")
    if args.interval <= 0 or args.hours <= 0:
        ap.error("--interval and --hours must be > 0")
    if args.calibrate is None:
        args.calibrate = args.mode > 0
    out = args.out or os.path.join(DATA_DIR, f"24hrstest_x_{datetime.now():%Y%m%d_%H%M%S}.csv")
    places = {"calibration slide": args.slide_pos, "sample": args.sample_pos}
    visits = int(args.hours * 3600 / args.interval) + 1

    camera = Camera(args.gain)  # before anything moves, so a camera problem shows up early
    photos = os.path.splitext(out)[0] + "_photos"
    for name in places:
        os.makedirs(os.path.join(photos, name.replace(" ", "_")), exist_ok=True)
    ser = serial.Serial(args.port, 115200, timeout=0.2)
    keep_awake(True)
    try:
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

        print(f"current = {current}, calibration slide = {args.slide_pos}, sample = {args.sample_pos}, "
              f"{visits} visits every {args.interval:g} s ({args.hours:g} h), "
              f"speed = {args.speed}, accel = {args.accel or 'default'}, "
              f"mode = {args.mode} ({MODES[args.mode]})")
        for name, pos in places.items():
            if abs(pos - current) > args.limit:
                raise SystemExit(f"{name} position {pos} is more than {args.limit} steps "
                                 f"away from the axis ({current}), see --limit")

        def timeout_for(distance):
            return 30 + 3 * abs(distance) / args.speed

        cal = None
        if args.calibrate:
            print(f"calibrating the encoder (the axis sweeps +{CAL_SWEEP} steps and back) ...")
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

        with open(out, "w", newline="") as f:
            f.write(f"# test=24hrstest_x port={args.port} "
                    f"started={datetime.now().isoformat(timespec='seconds')}\n")
            f.write(f"# slide_pos={args.slide_pos} sample_pos={args.sample_pos} "
                    f"interval={args.interval:g} hours={args.hours:g} speed={args.speed} "
                    f"accel={args.accel} mode={args.mode} gain={args.gain}\n")
            if cal:
                f.write("# calibration " + " ".join(
                    f"{k}={cal[k]}" for k in ("countsPerStep", "countSign", "backlashSteps",
                                              "residualScatter", "quality", "r2")) + "\n")
            w = csv.writer(f)
            w.writerow(["time_iso", "speed", "visit", "place", "move", "target",
                        "commanded", "rawCounts", "measured", "posErr", "photo"])
            f.flush()

            def go(visit, place, mode=args.mode, photo=True):
                target = places[place]
                move = target - prev[0]
                commanded, measured, pos_err, raw = move_to(
                    ser, target, args.speed, args.accel, mode, timeout_for(move))
                prev[0] = target
                now = datetime.now()
                path = ""
                if photo:
                    time.sleep(args.photo_delay)
                    folder = place.replace(" ", "_")  # calibration slide -> calibration_slide
                    path = os.path.join(photos, folder,
                                        f"{visit:04d}_{folder}_{now:%Y%m%d_%H%M%S}.{args.photo_format}")
                    try:
                        camera.photo(path)
                    except RuntimeError as e:  # one missing photo should not end 24 h
                        print(f"    ! {e}")
                        path = f"FAILED: {e}"
                print(f"{now:%m-%d %H:%M:%S} {visit:>5} {place:>17} {move:>8} {target:>10} "
                      f"{commanded:>10} {measured:>10} {pos_err:>7} {raw:>10}")
                w.writerow([now.isoformat(timespec="milliseconds"), args.speed, visit, place,
                            move, target, commanded, raw, measured, pos_err, path])
                f.flush()
                return pos_err

            print(f"\n{'time':>14} {'visit':>5} {'place':>17} {'move':>8} {'target':>10} "
                  f"{'commanded':>10} {'measured':>10} {'posErr':>7} {'rawCounts':>10}")
            prev = [current]
            # first to the sample, so the calibration slide is approached from the sample the
            # very first time too. This leg is open loop and is the scale check:
            # the encoder must see (about) as many steps as were commanded
            first = args.sample_pos - current
            err0 = pos_err(ser)
            drift = go(-1, "sample", mode=0, photo=False) - err0
            limit = 0.1 * abs(first) + (cal["backlashSteps"] if cal else 0) + 50
            if status.get("calibrated") and abs(drift) > limit:
                msg = (f"encoder scale check failed: after {first} open-loop steps posErr "
                       f"changed by {drift} (limit {limit:.0f}) - the firmware's calibration "
                       f"does not fit the encoder, run with --calibrate")
                if args.mode:
                    raise SystemExit(msg)
                print(f"WARNING: {msg}; 'measured' and 'posErr' are not valid in this run")

            t0 = time.time()
            for visit in range(visits):
                # fixed schedule from t0, so the move and photo times do not add up
                while time.time() < t0 + visit * args.interval:
                    time.sleep(min(1.0, max(0.0, t0 + visit * args.interval - time.time())))
                go(visit, "calibration slide" if visit % 2 == 0 else "sample")
            print(f"\ndone, {visits} visits")
    except KeyboardInterrupt:
        print("\nstopped by Ctrl+C")
    finally:
        keep_awake(False)
        ser.close()
        camera.close()

    print(f"raw data saved to {out}, photos in {photos}")
    if args.mode:
        print(f"note: axis 1 is left in axismode {args.mode} ({MODES[args.mode]})")


if __name__ == "__main__":
    main()
