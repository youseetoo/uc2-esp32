#!/usr/bin/env python3
# /// script
# requires-python = ">=3.12"
# dependencies = ["pyserial>=3.5"]
# ///
"""
X-axis closed-loop test over the CAN bus: back and forth between the
calibration slide and the sample, a photo at each visit, random X moves in
between. All corrections are left to the firmware's axis mode (--mode):
1 = monitor (only reports), 2 = correct (default), 3 = servo. The script only
sends the moves and records what the axis reports.

First X goes to the sample (no photo), so the calibration slide is reached
from the sample side the first time too. Then every --interval seconds X
moves to the other place: Z retracts to --safe-z (if set), X moves, Y moves,
Z goes into focus, photo. Between two visits X makes --walk-min .. --walk-max
moves to random targets between the places (--walk-margin inside them), then
goes back to the place of the last photo, so the leg to a place is always the
whole travel. A random move that would not be over before the visit is due is
left out. --seed repeats the same moves, --walk-max 0 switches them off.

Default: a visit every 30 s for 10 min; --full: every 10 min for 24 h;
--interval / --hours override both. --no-camera: the same moves, no photos.
The firmware's encoder calibration runs first (--no-calibrate: only re-anchor
the encoder to the step counter).

A latched fault, a move that does not finish or an SDO failure ends the run
(the row is written, the axes stay, no fault is cleared). Health DEGRADED is
recorded and the run goes on.

The PC is on the USB port of the CAN master; X with its encoder (--node 11),
Y (--y-node 12) and Z (--z-node 13) are CAN slaves, Y and Z open loop.

Usage:
    uv run tools/PYTHON/encoder_test/encoder_closed_loop.py --port COM6 --no-camera
    uv run tools/PYTHON/encoder_test/encoder_closed_loop.py --port COM7 --status
    uv run tools/PYTHON/encoder_test/encoder_closed_loop.py --port COM7 --mode 1
    uv run tools/PYTHON/encoder_test/encoder_closed_loop.py --port COM7 --full

Output: <name>.csv, one row per visit, <name>_walk.csv, one row per random
move, photos in <name>_photos. '#' lines are run metadata
(pandas: read_csv(..., comment='#')).
"""
import argparse
import csv
import ctypes
import json
import math
import os
import random
import sys
import time
from datetime import datetime

import serial

# results (CSV, photos) go to encoder_test_data/ next to this script
DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "encoder_test_data")

# ---- the two places: absolute step positions (x, y, z) ----------------------
# Used when --slide-pos/-y/-z and --sample-pos/-y/-z are not given.
# X as the step counter reads on 2026-10-12 with the stage at the calibration
# slide; the sample is -296064 steps in X away from it.
SLIDE_XYZ = (-1177676, 328450, 41712)   # calibration slide
SAMPLE_XYZ = (-1473740, 327942, 43462)  # sample
# None = Z is not retracted while X and Y move. A number = Z goes there before
# X / Y move, and comes into focus afterwards.
SAFE_Z = None
# ------------------------------------------------------------------------------

MODES = {1: "MONITOR", 2: "CORRECT", 3: "SERVO"}
HEALTH = {0: "OK", 1: "DEGRADED", 2: "FAULT"}
FAULTS = {0: "NONE", 1: "STALL", 2: "LOST_STEPS", 3: "DIVERGENCE", 4: "TIMEOUT",
          5: "CAL_INVALID", 6: "CAL_FAILED", 7: "ENC_NOISE"}
# CORRECT / SERVO adopt the encoder position as the new step counter, so the
# move ends near the target, not exactly on it
ARRIVE_TOL = 200

# slave object dictionary (main/src/canopen/UC2_OD_Indices.h)
OD_POSITION, OD_STATUS = 0x2001, 0x2004
OD_MEASURED, OD_POS_ERR, OD_MODE, OD_HEALTH, OD_FAULT = 0x2040, 0x2041, 0x2042, 0x2043, 0x2044
OD_CALIBRATED, OD_CALIBRATE, OD_CPS_Q16, OD_BACKLASH, OD_RAW = 0x2046, 0x2048, 0x2049, 0x204A, 0x204B
OD_RESET = 0x2045  # 1 = trust encoder, 2 = trust steps, 3 = force rehome


def open_master(port, baud):
    """Open the master's serial port without pulsing DTR / RTS: on the
    USB-UART bridge those lines reset the ESP32."""
    ser = serial.Serial()
    ser.port, ser.baudrate, ser.timeout = port, baud, 0.2
    ser.dtr = False
    ser.rts = False
    ser.open()
    return ser


class Axis:
    """One motor axis on a CAN slave, reached through the master's serial port."""

    def __init__(self, ser, node, sub, stepper):
        self.ser, self.node, self.sub, self.stepper = ser, node, sub, stepper

    def sdo(self, index, value=None, typ="i32", timeout=3.0, tries=3):
        """SDO read (value is None) or write through the master's SDO bridge."""
        req = {"node": self.node, "index": index, "sub": self.sub, "type": typ,
               "op": "r" if value is None else "w"}
        if value is not None:
            req["value"] = value
        for _ in range(tries):
            self.ser.reset_input_buffer()
            self.ser.write((json.dumps({"task": "/can_act", "sdo": req}) + "\n").encode())
            t_end = time.time() + timeout
            while time.time() < t_end:
                line = self.ser.readline().decode(errors="ignore")
                i = line.find("{")
                if i < 0:
                    continue
                try:
                    resp = json.loads(line[i:])
                except ValueError:
                    continue
                if not isinstance(resp, dict) or resp.get("index") != index or "status" not in resp:
                    continue
                if resp["status"] == "ok":
                    return resp.get("value")
                break  # SDO failed on the bus: try again
            time.sleep(0.2)
        raise RuntimeError(f"SDO {'read' if value is None else 'write'} 0x{index:04X} sub {self.sub} "
                           f"on node {self.node} failed - is this port the CAN master, "
                           f"and the slave on the bus?")

    def position(self):
        """(step position, is running)"""
        return self.sdo(OD_POSITION), bool(self.sdo(OD_STATUS, typ="u8") & 1)

    def feedback(self):
        """(commanded, measured, posErr, rawCounts), like the firmware's log line."""
        return (self.sdo(OD_POSITION), self.sdo(OD_MEASURED), self.sdo(OD_POS_ERR),
                self.sdo(OD_RAW))

    def status(self):
        pos, running = self.position()
        return {"position": pos, "isRunning": int(running),
                "measured": self.sdo(OD_MEASURED), "posErr": self.sdo(OD_POS_ERR),
                "axismode": self.sdo(OD_MODE, typ="u8"), "health": self.sdo(OD_HEALTH, typ="u8"),
                "fault": self.sdo(OD_FAULT, typ="u8"),
                "calibrated": self.sdo(OD_CALIBRATED, typ="u8"), "rawCounts": self.sdo(OD_RAW)}

    def check_fault(self, what):
        if self.sdo(OD_HEALTH, typ="u8") == 2:  # latched: motion is refused
            fault = self.sdo(OD_FAULT, typ="u8")
            raise RuntimeError(f"axis fault {fault} ({FAULTS.get(fault, '?')}) during {what}\n"
                               f'clear it with {{"stepperid": {self.stepper}, "axisreset": 1}}')

    def set_mode(self, mode):
        self.sdo(OD_MODE, mode, "u8")
        time.sleep(0.5)

    def calibrate(self, timeout=120.0):
        """Run the firmware's encoder calibration (sign, counts per step,
        backlash). The axis sweeps in + from where it is and comes back.
        Returns what can be read back over CAN."""
        start = self.position()[0]
        self.sdo(OD_CALIBRATE, 1, "u8")
        t0 = time.time()
        moved, idle = False, None
        while True:
            pos, running = self.position()
            moved = moved or pos != start or running
            if moved and not running and pos == start:
                idle = idle or time.time()
                if time.time() - idle > 2.0:  # the sweep pauses between its segments
                    break
            else:
                idle = None
            if not moved and time.time() - t0 > 15:
                raise RuntimeError("calibration did not start (the axis never moved)")
            if time.time() - t0 > timeout:
                raise TimeoutError("calibration did not finish")
            time.sleep(0.1)
        time.sleep(1.0)  # the slave publishes the result on its next loop
        fault = self.sdo(OD_FAULT, typ="u8")
        if fault == 6 or not self.sdo(OD_CALIBRATED, typ="u8"):
            raise RuntimeError(f"axis calibration failed (fault {fault} {FAULTS.get(fault, '?')})")
        q16 = self.sdo(OD_CPS_Q16)  # signed: the sign is the count direction
        return {"countsPerStep": abs(q16) / 65536, "countSign": 1 if q16 >= 0 else -1,
                "backlashSteps": abs(self.sdo(OD_BACKLASH))}

    def move_to(self, target, speed, accel, tol=0, settle=1.0, timeout=60.0, encoder=True):
        """Absolute move in the axis's current mode, wait until arrived,
        return the step position. encoder=False is for an axis without
        encoder (Y, focus): its health is not read."""
        stepper = {"stepperid": self.stepper, "position": target, "speed": speed, "isabs": 1}
        if accel is not None:
            stepper["acceleration"] = accel
        self.ser.write((json.dumps({"task": "/motor_act",
                                    "motor": {"steppers": [stepper]}}) + "\n").encode())
        arrived = None
        t_end = time.time() + timeout
        while time.time() < t_end:
            time.sleep(0.2)
            pos, running = self.position()
            if encoder:
                self.check_fault(f"move to {target}")
            if not running and abs(pos - target) <= tol:
                arrived = arrived or time.time()
                if time.time() - arrived > settle:  # the firmware's corrections follow the move
                    return pos
            else:
                arrived = None
        raise TimeoutError(f"move to {target} did not finish")


class Camera:
    """First Hikrobot camera found, through the MVS SDK's Python wrapper."""

    def __init__(self, gain=None, exposure=None):
        sdk = os.getenv("MVCAM_COMMON_RUNENV")
        if not sdk:
            raise RuntimeError("MVCAM_COMMON_RUNENV is not set - is the Hikrobot MVS SDK "
                               "installed? (--no-camera runs without it)")
        sys.path.append(os.path.join(sdk, "Samples", "Python", "MvImport"))
        import MvCameraControl_class as mv
        self.mv = mv
        self.cam = None
        mv.MvCamera.MV_CC_Initialize()
        devices = mv.MV_CC_DEVICE_INFO_LIST()
        self.check(mv.MvCamera.MV_CC_EnumDevices(mv.MV_GIGE_DEVICE | mv.MV_USB_DEVICE, devices),
                   "enum devices")
        if devices.nDeviceNum == 0:
            raise RuntimeError("no Hikrobot camera found (--no-camera runs without it)")
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
        if exposure is not None:
            cam.MV_CC_SetEnumValueByString("ExposureAuto", "Off")
            self.check(cam.MV_CC_SetFloatValue("ExposureTime", exposure), f"set exposure {exposure}")
        value = mv.MVCC_FLOATVALUE()
        self.settings = {}
        for key in ("ExposureTime", "Gain"):
            if cam.MV_CC_GetFloatValue(key, value) == 0:
                self.settings[key] = round(value.fCurValue, 2)
        print(f"camera: {self.settings}")

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
    ap.add_argument("--port", default=None, help="serial port of the CAN master")
    ap.add_argument("--baud", type=int, default=921600,
                    help="baud rate of the master's serial port")
    ap.add_argument("--status", action="store_true",
                    help="only print the axis status read over CAN and exit")
    ap.add_argument("--mode", type=int, choices=sorted(MODES), default=2,
                    help="firmware axis mode for every X move: 1 = monitor, "
                         "2 = correct (default), 3 = servo")
    ap.add_argument("--calibrate", action=argparse.BooleanOptionalAction, default=True,
                    help="run the firmware's encoder calibration first "
                         "(--no-calibrate: only re-anchor the encoder to the step counter)")
    ap.add_argument("--camera", action=argparse.BooleanOptionalAction, default=True,
                    help="take a photo at every visit (--no-camera: the same moves, no photos)")
    ap.add_argument("--full", action="store_true",
                    help="the 24 h version: a visit every 10 min for 24 h "
                         "(default: quick test, every 30 s for 10 min)")
    ap.add_argument("--interval", type=float, default=None,
                    help="seconds between two visits (default: 30, with --full 600)")
    ap.add_argument("--hours", type=float, default=None,
                    help="length of the test (default: 10 min, with --full 24)")
    ap.add_argument("--return-to-slide", action=argparse.BooleanOptionalAction, default=True,
                    help="after the last visit go back to the calibration slide (x, y, z), "
                         "if the test did not end there anyway")
    ap.add_argument("--speed", type=int, default=100000)
    ap.add_argument("--accel", type=int, default=100000, help="acceleration for all X moves")
    ap.add_argument("--walk-min", type=int, default=2,
                    help="random X moves between two visits: at least this many")
    ap.add_argument("--walk-max", type=int, default=5,
                    help="random X moves between two visits: at most this many (0: none)")
    ap.add_argument("--walk-margin", type=int, default=2000,
                    help="the random targets stay this many steps inside the two places")
    ap.add_argument("--seed", type=int, default=42,
                    help="random seed of the random moves (same seed = same moves)")
    ap.add_argument("--node", type=int, default=11, help="CAN node id of the X slave")
    ap.add_argument("--sub", type=int, default=2,
                    help="SDO sub-index of the axis on the slave (its "
                         "REMOTE_MOTOR_AXIS_ID + 1, X = 2)")
    ap.add_argument("--stepper", type=int, default=1,
                    help="stepperid the master routes to that slave (X = 1)")
    ap.add_argument("--slide-pos", type=int, default=SLIDE_XYZ[0],
                    help="absolute X position of the calibration slide (steps)")
    ap.add_argument("--sample-pos", type=int, default=SAMPLE_XYZ[0],
                    help="absolute X position of the sample (steps)")
    ap.add_argument("--limit", type=int, default=400000,
                    help="refuse X / Y positions farther than this from where "
                         "the axis is now (guards against a typo)")
    ap.add_argument("--slide-y", type=int, default=SLIDE_XYZ[1],
                    help="Y position at the calibration slide")
    ap.add_argument("--sample-y", type=int, default=SAMPLE_XYZ[1],
                    help="Y position at the sample")
    ap.add_argument("--y-node", type=int, default=12, help="CAN node id of the Y slave")
    ap.add_argument("--y-sub", type=int, default=3,
                    help="SDO sub-index of the axis on the Y slave (axis id + 1, Y = 3)")
    ap.add_argument("--y-stepper", type=int, default=2,
                    help="stepperid the master routes to the Y slave (Y = 2)")
    ap.add_argument("--y-speed", type=int, default=5000)
    ap.add_argument("--y-accel", type=int, default=None,
                    help="Y acceleration (default: not sent, firmware default)")
    ap.add_argument("--slide-z", type=int, default=SLIDE_XYZ[2],
                    help="focus (Z) position at the calibration slide")
    ap.add_argument("--sample-z", type=int, default=SAMPLE_XYZ[2],
                    help="focus (Z) position at the sample")
    ap.add_argument("--safe-z", type=int, default=SAFE_Z,
                    help="retracted Z position: Z goes here before every X / Y "
                         "move (default: Z is not retracted)")
    ap.add_argument("--z-node", type=int, default=13, help="CAN node id of the Z slave")
    ap.add_argument("--z-sub", type=int, default=4,
                    help="SDO sub-index of the axis on the Z slave (axis id + 1, Z = 4)")
    ap.add_argument("--z-stepper", type=int, default=3,
                    help="stepperid the master routes to the Z slave (Z = 3)")
    ap.add_argument("--z-speed", type=int, default=5000)
    ap.add_argument("--z-accel", type=int, default=None,
                    help="Z acceleration (default: not sent, firmware default)")
    ap.add_argument("--z-limit", type=int, default=20000,
                    help="refuse Z positions farther than this from where Z is now")
    ap.add_argument("--gain", type=float, default=5.0, help="camera gain in dB")
    ap.add_argument("--exposure", type=float, default=None,
                    help="camera exposure time in microseconds (default: as set "
                         "in the camera)")
    ap.add_argument("--photo-format", choices=("bmp", "png", "jpg", "tif"), default="png")
    ap.add_argument("--photo-delay", type=float, default=2.0,
                    help="seconds to wait after the move before a photo")
    ap.add_argument("--out", default=None,
                    help="CSV file (default: timestamped name in encoder_test_data/)")
    args = ap.parse_args()
    if not args.port:
        ap.error("--port is required")
    if args.status:
        ser = open_master(args.port, args.baud)
        try:
            time.sleep(1.0)
            axis = Axis(ser, args.node, args.sub, args.stepper)
            print(f"X status (node {args.node}): {axis.status()}")
            print(f"countsPerStep = {axis.sdo(OD_CPS_Q16) / 65536:.5f}, "
                  f"backlash = {axis.sdo(OD_BACKLASH)} steps")
            for name, a in (("Y", Axis(ser, args.y_node, args.y_sub, args.y_stepper)),
                            ("Z", Axis(ser, args.z_node, args.z_sub, args.z_stepper))):
                print(f"{name} (node {a.node}): position, running = {a.position()}")
        finally:
            ser.close()
        return
    if args.slide_pos == args.sample_pos:
        ap.error("--slide-pos and --sample-pos are the same")
    interval = args.interval or (600.0 if args.full else 30.0)
    hours = args.hours or (24.0 if args.full else 10 / 60)
    if interval <= 0 or hours <= 0:
        ap.error("--interval and --hours must be > 0")
    visits = int(hours * 3600 / interval + 1e-6) + 1
    if not 0 <= args.walk_min <= args.walk_max:
        ap.error("need 0 <= --walk-min <= --walk-max")
    # the random moves stay between the two places: outside them X hits something
    lo, hi = sorted((args.slide_pos, args.sample_pos))
    walk_lo, walk_hi = lo + args.walk_margin, hi - args.walk_margin
    if args.walk_margin < 0 or walk_lo >= walk_hi:
        ap.error("--walk-margin does not leave a window between the two places")
    if args.safe_z is not None and min(args.slide_z, args.sample_z) < args.safe_z \
            < max(args.slide_z, args.sample_z):
        ap.error("--safe-z lies between the two focus positions, so it is closer "
                 "than the focus of one of the places")
    rng = random.Random(args.seed)

    def walk_plan():
        """The random targets of one interval: --walk-min .. --walk-max
        positions anywhere in the window between the two places."""
        return [rng.randint(walk_lo, walk_hi)
                for _ in range(rng.randint(args.walk_min, args.walk_max))]

    out = args.out or os.path.join(DATA_DIR, f"closed_loop_test_can_x_{datetime.now():%Y%m%d_%H%M%S}.csv")
    walk_path = os.path.splitext(out)[0] + "_walk.csv"
    photos = os.path.splitext(out)[0] + "_photos"
    places = {"calibration slide": args.slide_pos, "sample": args.sample_pos}
    ys = {"calibration slide": args.slide_y, "sample": args.sample_y}
    focus = {"calibration slide": args.slide_z, "sample": args.sample_z}
    os.makedirs(os.path.dirname(os.path.abspath(out)), exist_ok=True)
    # before anything moves, so a camera problem shows up early
    camera = Camera(args.gain, args.exposure) if args.camera else None
    if camera:
        for name in places:
            os.makedirs(os.path.join(photos, name.replace(" ", "_")), exist_ok=True)
    ser = open_master(args.port, args.baud)
    keep_awake(True)
    # CORRECT / SERVO end near the target and correct after the move
    tol, settle = (ARRIVE_TOL, 2.0) if args.mode >= 2 else (0, 1.0)

    try:
        time.sleep(2.0)
        axis = Axis(ser, args.node, args.sub, args.stepper)
        status = axis.status()
        print(f"axis status (node {args.node}): {status}")
        axis.check_fault("the start of the run")
        current = status["position"]
        print(f"current = {current}, calibration slide = {args.slide_pos}, sample = {args.sample_pos}, "
              f"{visits} visits every {interval:g} s, speed = {args.speed}, accel = {args.accel}, "
              f"mode = {args.mode} ({MODES[args.mode]}), "
              f"camera = {'on' if camera else 'off'}")
        print(f"random walk: {args.walk_min} .. {args.walk_max} moves between two visits, "
              f"targets inside {walk_lo} .. {walk_hi}, seed = {args.seed}")
        for name, pos in places.items():
            if abs(pos - current) > args.limit:
                raise SystemExit(f"{name} position {pos} is more than {args.limit} steps "
                                 f"away from the axis ({current}), see --limit")

        def y_to(target):
            """Open-loop Y move (no encoder), returns the Y step position."""
            return yaxis.move_to(target, args.y_speed, args.y_accel, 0, 0.5,
                                 30 + 3 * abs(target - ypos[0]) / args.y_speed, encoder=False)

        def z_to(target):
            """Open-loop focus move (no encoder), returns the Z step position."""
            return zaxis.move_to(target, args.z_speed, args.z_accel, 0, 0.5,
                                 30 + 3 * abs(target - zpos[0]) / args.z_speed, encoder=False)

        yaxis = Axis(ser, args.y_node, args.y_sub, args.y_stepper)
        ypos = [yaxis.position()[0]]
        print(f"Y (node {args.y_node}): current = {ypos[0]}, calibration slide = {args.slide_y}, "
              f"sample = {args.sample_y}, speed = {args.y_speed}")
        for name, y in ys.items():
            if abs(y - ypos[0]) > args.limit:
                raise SystemExit(f"{name} Y position {y} is more than {args.limit} steps "
                                 f"away from Y ({ypos[0]}), see --limit")

        zaxis = Axis(ser, args.z_node, args.z_sub, args.z_stepper)
        zpos = [zaxis.position()[0]]
        print(f"Z (node {args.z_node}): current = {zpos[0]}, calibration slide = {args.slide_z}, "
              f"sample = {args.sample_z}, safe = {args.safe_z}, speed = {args.z_speed}")
        z_all = {**focus, **({} if args.safe_z is None else {"safe": args.safe_z})}
        for name, z in z_all.items():
            if abs(z - zpos[0]) > args.z_limit:
                raise SystemExit(f"{name} Z position {z} is more than {args.z_limit} steps "
                                 f"away from Z ({zpos[0]}), see --z-limit")
        if args.safe_z is not None:
            # retract before anything moves in X (the calibration sweep too)
            zpos[0] = z_to(args.safe_z)

        cal = None
        if args.calibrate:
            print("calibrating the encoder (the axis sweeps in + and back) ...")
            cal = axis.calibrate()
            print(f"calibration ok: countsPerStep = {cal['countsPerStep']:.5f}, "
                  f"countSign = {cal['countSign']}, backlash = {cal['backlashSteps']} steps")
        else:
            # the two places were taught in as step positions: re-anchor the encoder
            # to the step counter (no motion), so an old posErr is not "corrected"
            print(f"posErr was {status['posErr']} steps: re-anchoring the encoder to the step counter")
            axis.sdo(OD_RESET, 2, "u8")
            time.sleep(1.0)
        status = axis.status()
        if status["fault"] == 5:
            print("WARNING: the firmware reports CAL_INVALID - its calibration was only "
                  "rescaled after a microstep change, run with --calibrate")
        if not status["calibrated"]:
            raise SystemExit("the axis is not calibrated: the firmware would run every move "
                             "open loop. Run with --calibrate")
        axis.set_mode(args.mode)  # over CAN a move follows the axis mode

        with open(out, "w", newline="") as f, open(walk_path, "w", newline="") as wf:
            f.write(f"# test=closed_loop_test_can_x port={args.port} node={args.node} "
                    f"started={datetime.now().isoformat(timespec='seconds')}\n")
            f.write(f"# slide_pos={args.slide_pos} sample_pos={args.sample_pos} "
                    f"interval={interval:g} visits={visits} speed={args.speed} "
                    f"accel={args.accel} mode={args.mode} "
                    f"camera={camera.settings if camera else 'off'}\n")
            f.write(f"# slide_y={args.slide_y} sample_y={args.sample_y} y_node={args.y_node} "
                    f"y_speed={args.y_speed} y_accel={args.y_accel}\n")
            f.write(f"# slide_z={args.slide_z} sample_z={args.sample_z} safe_z={args.safe_z} "
                    f"z_node={args.z_node} z_speed={args.z_speed} z_accel={args.z_accel}\n")
            f.write(f"# walk_min={args.walk_min} walk_max={args.walk_max} "
                    f"walk_window={walk_lo}..{walk_hi} seed={args.seed}\n")
            if cal:
                f.write("# calibration " + " ".join(f"{k}={v}" for k, v in cal.items()) + "\n")
            # what the axis reports after an X move, the same in both files
            axis_cols = ["commanded", "measured", "posErr", "rawCounts", "health", "fault",
                         "xMoveSec", "error"]
            w = csv.writer(f)
            w.writerow(["time_iso", "visit", "place", "move", "target"] + axis_cols
                       + ["y", "z", "photo"])
            ww = csv.writer(wf)
            ww.writerow(["time_iso", "visit", "place", "walk", "planned", "move", "target"]
                        + axis_cols)
            f.flush()
            wf.flush()
            prev = [current]  # the last X target

            def x_move(target):
                """One X move in the firmware's mode. Returns what the axis
                reports afterwards (axis_cols) and, if the move failed, why."""
                t_move = time.time()
                error = ""
                try:
                    axis.move_to(target, args.speed, args.accel, tol, settle,
                                 60 + 3 * abs(target - prev[0]) / args.speed)
                except (RuntimeError, TimeoutError) as e:
                    error = str(e)
                prev[0] = target
                try:
                    state = list(axis.feedback())
                    health, fault = axis.sdo(OD_HEALTH, typ="u8"), axis.sdo(OD_FAULT, typ="u8")
                    state += [HEALTH.get(health, health), FAULTS.get(fault, fault)]
                except RuntimeError as e:  # the slave no longer answers: the row is still written
                    state = [""] * 6
                    error = error or str(e)
                cols = state + [round(time.time() - t_move, 1),
                                error.splitlines()[0] if error else ""]
                return cols, error

            def show(cols):
                commanded, measured, pos_err, raw, health, fault = cols[:6]
                return (f"{commanded:>10} {measured:>10} {pos_err:>7} {raw:>10} "
                        f"{health:>8} {fault:>11}")

            def go(visit, place, photo=True, into_focus=False):
                target = places[place]
                move = target - prev[0]
                if args.safe_z is not None:  # retract, so nothing can touch while X / Y move
                    zpos[0] = z_to(args.safe_z)
                cols, error = x_move(target)
                if not error:
                    ypos[0] = y_to(ys[place])
                    if photo or into_focus or args.safe_z is None:  # into focus (from the safe Z, if any)
                        zpos[0] = z_to(focus[place])
                now = datetime.now()
                path = ""
                if camera and photo and not error:
                    time.sleep(args.photo_delay)
                    folder = place.replace(" ", "_")  # calibration slide -> calibration_slide
                    path = os.path.join(photos, folder,
                                        f"{visit:04d}_{folder}_{now:%Y%m%d_%H%M%S}.{args.photo_format}")
                    try:
                        camera.photo(path)
                    except RuntimeError as e:  # one missing photo should not end the run
                        print(f"    ! {e}")
                        path = f"FAILED: {e}"
                print(f"{now:%m-%d %H:%M:%S} {visit:>5} {place:>17} {move:>8} {target:>10} "
                      f"{show(cols)} {ypos[0]:>8} {zpos[0]:>8}")
                w.writerow([now.isoformat(timespec="milliseconds"), visit, place, move, target]
                           + cols + [ypos[0], zpos[0], path])
                f.flush()
                if error:
                    raise SystemExit(f"the move to the {place} failed, the run ends here: {error}")

            def walk_sec(d):
                """Longest a random move of d steps takes: the ramps of the move,
                and the settling and the polling of move_to."""
                v, a = args.speed, args.accel or args.speed
                return (d / v + v / a if d * a >= v * v else 2 * math.sqrt(d / a)) + settle + 2.0

            def walk(visit, place, due):
                """The random X moves before visit `visit` (to `place`, due at the
                time `due`): to the targets of walk_plan(), one after the other, as
                long as the next one and the way back are over before the visit is
                due, then back to the place of the last photo: X waits there, so
                the leg to `place` is the whole travel."""
                plan = walk_plan()
                last = "sample" if place == "calibration slide" else "calibration slide"
                home = places[last]
                if plan and args.safe_z is not None:  # retract, so nothing can touch while X moves
                    zpos[0] = z_to(args.safe_z)
                for i, t in enumerate(plan + [home], 1):
                    back = i > len(plan)  # the last move: back to the place of the last photo
                    name = "back" if back else f"{i}/{len(plan)}"
                    move = t - prev[0]
                    if back and not move:  # no random move was made
                        break
                    if not back and time.time() + walk_sec(abs(move)) + walk_sec(abs(home - t)) > due:
                        print(f"    walk {name} left out: visit {visit} is due")
                        continue
                    cols, error = x_move(t)
                    print(f"    walk {name:>4}: {move:>+8} -> {t:>10}  {show(cols)}")
                    ww.writerow([datetime.now().isoformat(timespec="milliseconds"), visit, place,
                                 name if back else i, len(plan), move, t] + cols)
                    wf.flush()
                    if error:
                        raise SystemExit(f"walk {name} before visit {visit} failed, "
                                         f"the run ends here: {error}")

            print(f"\n{'time':>14} {'visit':>5} {'place':>17} {'move':>8} {'target':>10} "
                  f"{'commanded':>10} {'measured':>10} {'posErr':>7} {'rawCounts':>10} "
                  f"{'health':>8} {'fault':>11} {'y':>8} {'z':>8}")
            # first to the sample, so the calibration slide is approached from the
            # sample the very first time too
            go(-1, "sample", photo=False)

            t0 = time.time()
            for visit in range(visits):
                place = "calibration slide" if visit % 2 == 0 else "sample"
                # fixed schedule from t0, so the move and photo times do not add up
                due = t0 + visit * interval
                # until then: the random moves, then X waits at the place of the last photo
                if visit:
                    walk(visit, place, due)
                while time.time() < due:
                    time.sleep(min(1.0, max(0.0, due - time.time())))
                go(visit, place)
            if args.return_to_slide and (visits - 1) % 2:
                # the last visit was the sample: end where the test started,
                # at the calibration slide (no photo, logged as one more row)
                go(visits, "calibration slide", photo=False, into_focus=True)
            print(f"\ndone, {visits} visits")
    except KeyboardInterrupt:
        print("\nstopped by Ctrl+C")
    finally:
        keep_awake(False)
        ser.close()
        if camera:
            camera.close()
        if os.path.exists(out):  # also after an error: the rows up to it are in the files
            print(f"raw data in {out}, the random moves in {walk_path}"
                  + (f", photos in {photos}" if camera else ""))
            print(f"note: the axis is left in axismode {args.mode} ({MODES[args.mode]})")


if __name__ == "__main__":
    main()
