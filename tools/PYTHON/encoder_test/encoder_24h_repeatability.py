#!/usr/bin/env python3
"""
X-axis 24 hour repeatability test over the CAN bus, open loop: go back and
forth between the calibration slide and the sample and take a photo at each
visit. Every --interval seconds (default 10 min) for --hours (default 24),
145 photos with the defaults.

The firmware runs X open loop (axismode 0); this script reads the linear
encoder and compensates by itself. The raw count at the first arrival at a
place is its reference. At every later arrival the count is compared with
it; if it is more than --comp-counts off, all X targets are shifted by the
missing steps and the place is approached again from the same side (--preload
back-off, so the backlash stays the same), up to --comp-tries times. The
steps per count come from the two references (travel / counts between the
places). --no-compensate only records.

Lost steps: a correction above --lost-steps means the motor stalled. It is
recovered at --recover-speed in chunks of --recover-chunk steps; after every
chunk the encoder must have followed, otherwise the run ends. The first leg
to each place (its reference) is driven at --recover-speed too. The samples
of a lost move go to <csv name>_lost_steps.csv. After lost steps the step
counter is reset from the encoder at the end of the run (back at the
calibration slide); if the run ended elsewhere, --resync brings X back to the
calibration slide by the encoder first.

Safety: nothing moves before the camera and all three axes have answered and
all targets passed --limit / --z-limit. X, then Y, then Z into focus (with a
safe Z, Z retracts first). Any SDO failure, move timeout or latched fault
ends the run and leaves the axes where they are; only a failed photo is
skipped.

The places are (x, y, z) step positions, SLIDE_XYZ / SAMPLE_XYZ below (the
command line overrides them). The PC is on the USB port of the CAN master;
X (--node 11), Y (--y-node 12) and Z (--z-node 13) are CAN slaves. Photos
(Hikrobot camera, MVS SDK, client closed) go to <csv name>_photos/<place>/,
about 30 MB each.

Usage:
    uv run tools/PYTHON/encoder_test/encoder_24h_repeatability.py --photo-test --gain 23
    uv run tools/PYTHON/encoder_test/encoder_24h_repeatability.py --port COM7 --status
    uv run tools/PYTHON/encoder_test/encoder_24h_repeatability.py --port COM7 --speed 100000 --gain 23 --quick
    uv run tools/PYTHON/encoder_test/encoder_24h_repeatability.py --port COM7 --speed 100000 --gain 23

Output: one CSV row per visit, '#' lines are run metadata
(pandas: read_csv(..., comment='#')).
"""
import argparse
import csv
import ctypes
import json
import os
import sys
import time
from datetime import datetime

import serial

# results (CSV, photos) go to encoder_test_data/ next to this script
DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "encoder_test_data")

# ---- the two places: absolute step positions (x, y, z) ----------------------
# Used when --slide-pos/-y/-z and --sample-pos/-y/-z are not given.
SLIDE_XYZ = (147808, 328052, 41438)    # calibration slide
SAMPLE_XYZ = (-148202, 328022, 43168)  # sample
# None = Z stays in focus while X and Y move (X clears both focus heights).
# A number = Z goes there before X / Y move, and comes into focus afterwards.
SAFE_Z = None
# ------------------------------------------------------------------------------

FAULTS = {0: "NONE", 1: "STALL", 2: "LOST_STEPS", 3: "DIVERGENCE", 4: "TIMEOUT",
          5: "CAL_INVALID", 6: "CAL_FAILED", 7: "ENC_NOISE"}

# slave object dictionary (main/src/canopen/UC2_OD_Indices.h)
OD_POSITION, OD_STATUS = 0x2001, 0x2004
OD_MEASURED, OD_POS_ERR, OD_MODE, OD_HEALTH, OD_FAULT = 0x2040, 0x2041, 0x2042, 0x2043, 0x2044
OD_CALIBRATED, OD_CPS_Q16, OD_BACKLASH, OD_RAW = 0x2046, 0x2049, 0x204A, 0x204B
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

    def move_to(self, target, speed, accel, tol=0, settle=1.0, timeout=60.0, encoder=True,
                trace=None):
        """Absolute move, wait until arrived, return (commanded, measured,
        posErr, rawCounts). encoder=False: an axis without encoder (Y, Z).
        trace: a list, gets (seconds, step position, raw count) per poll."""
        stepper = {"stepperid": self.stepper, "position": target, "speed": speed, "isabs": 1}
        if accel is not None:
            stepper["acceleration"] = accel
        self.ser.write((json.dumps({"task": "/motor_act",
                                    "motor": {"steppers": [stepper]}}) + "\n").encode())
        arrived = None
        t0 = time.time()
        t_end = t0 + timeout
        while time.time() < t_end:
            time.sleep(0.2)
            if trace is not None:  # the two reads right after each other
                trace.append((round(time.time() - t0, 2), self.sdo(OD_POSITION), self.sdo(OD_RAW)))
            pos, running = self.position()
            if encoder:
                self.check_fault(f"move to {target}")
            if not running and abs(pos - target) <= tol:
                arrived = arrived or time.time()
                if time.time() - arrived > settle:
                    return self.feedback() if encoder else (pos, pos, 0, 0)
            else:
                arrived = None
        raise TimeoutError(f"move to {target} did not finish")


def chunked_move(axis, goal, speed, accel, chunk, k, what, samples):
    """To the step position `goal` in chunks of `chunk` steps. After every
    chunk the encoder must have seen the steps (k = steps per count; 5 % and
    200 steps of slack for the backlash), otherwise nothing more is sent.
    samples gets (seconds, step position, raw count) after every chunk.
    Returns what is wrong, or None."""
    t0 = time.time()
    pos, raw = axis.sdo(OD_POSITION), axis.sdo(OD_RAW)
    while pos != goal:
        d = max(-chunk, min(chunk, goal - pos))
        new_pos, _, _, new_raw = axis.move_to(pos + d, speed, accel, 0, 0.3,
                                              30 + 3 * abs(d) / speed)
        seen = round((new_raw - raw) * k)
        ok = abs(seen - d) <= 0.05 * abs(d) + 200
        samples.append((round(time.time() - t0, 2), new_pos, new_raw))
        print(f"    !     {d:+6d} steps: counter {new_pos:>8}  encoder {new_raw:>7}  "
              f"({seen:+d} steps seen{'' if ok else ' - NOT FOLLOWING'})")
        if not ok:
            return (f"{what}: {d:+d} steps were sent from counter {pos}, but the encoder saw "
                    f"{new_raw - raw:+d} counts = {seen:+d} steps. Nothing more is sent")
        pos, raw = new_pos, new_raw
    return None


def to_raw(axis, goal_raw, k, tol, speed, accel, chunk, what, samples, past=0):
    """To the raw encoder count `goal_raw` in checked chunks, until fewer than
    `tol` steps are missing (k may be a few % off, so this iterates).
    past = +1 / -1: also done once the axis is past the goal in that step
    direction. Returns what is wrong, or None."""
    for _ in range(6):
        pos, raw = axis.sdo(OD_POSITION), axis.sdo(OD_RAW)
        need = round((goal_raw - raw) * k)
        if abs(need) <= tol or need * past < 0:
            break
        problem = chunked_move(axis, pos + need, speed, accel, chunk, k, what, samples)
        if problem:
            return problem
    return None


class Camera:
    """First Hikrobot camera found, through the MVS SDK's Python wrapper."""

    def __init__(self, gain=None, exposure=None):
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
    ap.add_argument("--node", type=int, default=11, help="CAN node id of the X slave")
    ap.add_argument("--sub", type=int, default=2,
                    help="SDO sub-index of the axis on the slave (its "
                         "REMOTE_MOTOR_AXIS_ID + 1, X = 2)")
    ap.add_argument("--stepper", type=int, default=1,
                    help="stepperid the master routes to that slave (X = 1)")
    ap.add_argument("--status", action="store_true",
                    help="only print the axis status read over CAN and exit")
    ap.add_argument("--slide-pos", type=int, default=SLIDE_XYZ[0],
                    help="absolute X position of the calibration slide (steps)")
    ap.add_argument("--sample-pos", type=int, default=SAMPLE_XYZ[0],
                    help="absolute X position of the sample (steps)")
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
    ap.add_argument("--compensate", action=argparse.BooleanOptionalAction, default=True,
                    help="correct X ourselves when the encoder at a place is "
                         "off its first arrival there (--no-compensate: only record)")
    ap.add_argument("--comp-counts", type=int, default=5,
                    help="compensate when the encoder is more than this many counts off")
    ap.add_argument("--preload", type=int, default=2000,
                    help="back-off distance for the re-approach after a correction")
    ap.add_argument("--comp-tries", type=int, default=3,
                    help="corrections per visit before giving up")
    ap.add_argument("--lost-steps", type=int, default=2000,
                    help="a correction above this many steps is recorded as a "
                         "lost-step event (the motor stalled during the move)")
    ap.add_argument("--recover-speed", type=int, default=10000,
                    help="speed for the recovery after a lost-step event")
    ap.add_argument("--recover-chunk", type=int, default=5000,
                    help="the recovery is driven in chunks of this many steps; the "
                         "run ends at the first chunk the encoder does not follow")
    ap.add_argument("--resync", action="store_true",
                    help="after lost steps: bring X back to the calibration slide by the "
                         "encoder (to the raw count --resync-raw, at --recover-speed in "
                         "checked chunks), set the step counter from the encoder there "
                         "and exit. Y and Z are not moved")
    ap.add_argument("--resync-raw", type=int, default=0,
                    help="raw encoder count of the calibration slide for --resync (0 if the "
                         "encoder was last zeroed there, as --resync itself does)")
    ap.add_argument("--max-comp", type=int, default=None,
                    help="largest single correction in steps; beyond it the run ends "
                         "(default: 1.1 x the travel between the two places)")
    ap.add_argument("--max-offset", type=int, default=3000000,
                    help="largest total correction in steps; beyond it the run ends")
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
    ap.add_argument("--interval", type=float, default=600,
                    help="seconds between two visits (default 600 = 10 min)")
    ap.add_argument("--hours", type=float, default=24, help="length of the test")
    ap.add_argument("--return-to-slide", action=argparse.BooleanOptionalAction, default=True,
                    help="after the last visit go back to the calibration slide (x, y, z), "
                         "if the test did not end there anyway")
    ap.add_argument("--quick", action="store_true",
                    help="rehearsal: the same sequence, but only 4 visits "
                         "(calibration slide, sample, calibration slide, sample) 20 s apart")
    ap.add_argument("--limit", type=int, default=400000,
                    help="refuse X / Y positions farther than this from where "
                         "the axis is now (guards against a typo)")
    ap.add_argument("--speed", type=int, default=10000)
    ap.add_argument("--accel", type=int, default=100000,
                    help="acceleration for all X moves")
    ap.add_argument("--gain", type=float, default=None,
                    help="camera gain in dB (default: as set in the camera)")
    ap.add_argument("--exposure", type=float, default=None,
                    help="camera exposure time in microseconds (default: as set "
                         "in the camera)")
    ap.add_argument("--photo-format", choices=("bmp", "png", "jpg", "tif"), default="png")
    ap.add_argument("--photo-delay", type=float, default=2.0,
                    help="seconds to wait after the move before a photo")
    ap.add_argument("--photo-test", action="store_true",
                    help="only take one photo and exit, the axis is not moved")
    ap.add_argument("--out", default=None, help="CSV file (default: timestamped name in encoder_test_data/)")
    args = ap.parse_args()
    os.makedirs(DATA_DIR, exist_ok=True)
    if args.photo_test:
        camera = Camera(args.gain, args.exposure)
        try:
            path = os.path.join(DATA_DIR, f"photo_test_{datetime.now():%Y%m%d_%H%M%S}.{args.photo_format}")
            camera.photo(path)
            print(f"photo saved to {path}")
        finally:
            camera.close()
        return
    if args.status:
        if not args.port:
            ap.error("--port is required")
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
    if args.resync:
        if not args.port:
            ap.error("--port is required")
        ser = open_master(args.port, args.baud)
        try:
            time.sleep(1.0)
            axis = Axis(ser, args.node, args.sub, args.stepper)
            st = axis.status()
            print(f"X status (node {args.node}): {st}")
            if st["isRunning"] or st["health"] == 2 or st["axismode"] != 0 or not st["calibrated"]:
                raise SystemExit("--resync needs an idle, calibrated X axis in open loop "
                                 "(axismode 0) without a latched fault")
            k = 65536 / axis.sdo(OD_CPS_Q16)  # the firmware's steps per count, signed
            speed = min(args.speed, args.recover_speed)
            # first to the approach point on the side the calibration slide is always
            # reached from, then onto the count; k is about 1.5 % off, so iterate
            side = 1 if args.sample_pos > args.slide_pos else -1
            for goal_raw, tol, past in ((args.resync_raw + side * args.preload / k, 200, 0),
                                        (args.resync_raw, abs(k), -side)):
                problem = to_raw(axis, goal_raw, k, tol, speed, args.accel,
                                 args.recover_chunk, "resync", [], past)
                if problem:
                    raise SystemExit(problem + ", X stays where it is")
            raw = axis.sdo(OD_RAW)
            if abs(raw - args.resync_raw) > 3:
                raise SystemExit(f"encoder at {raw}, not at {args.resync_raw}: not re-synced")
            axis.sdo(OD_RESET, 1, "u8")  # step counter := encoder position, encoder zeroed
            time.sleep(1.0)
            st = axis.status()
            print(f"re-synced: {st}")
            if abs(st["position"] - args.slide_pos) > 50:
                print(f"WARNING: the step counter is {st['position']} here, the calibration slide "
                      f"is expected at {args.slide_pos} (--slide-pos)")
        finally:
            ser.close()
        return
    if not args.port:
        ap.error("--port is required")
    if args.slide_pos == args.sample_pos:
        ap.error("--slide-pos and --sample-pos are the same")
    if args.interval <= 0 or args.hours <= 0:
        ap.error("--interval and --hours must be > 0")
    if args.safe_z is not None and min(args.slide_z, args.sample_z) < args.safe_z \
            < max(args.slide_z, args.sample_z):
        ap.error("--safe-z lies between the two focus positions, so it is closer "
                 "than the focus of one of the places")
    max_comp = args.max_comp or round(1.1 * abs(args.slide_pos - args.sample_pos))
    out = args.out or os.path.join(DATA_DIR, f"24hrstest_can_linear_x_{datetime.now():%Y%m%d_%H%M%S}.csv")
    trace_path = os.path.splitext(out)[0] + "_lost_steps.csv"
    places = {"calibration slide": args.slide_pos, "sample": args.sample_pos}
    ys = {"calibration slide": args.slide_y, "sample": args.sample_y}
    focus = {"calibration slide": args.slide_z, "sample": args.sample_z}
    if args.quick:
        args.interval, args.hours = 20.0, 3 * 20 / 3600
    visits = int(args.hours * 3600 / args.interval + 1e-6) + 1

    camera = Camera(args.gain, args.exposure)  # before anything moves, so a camera problem shows up early
    photos = os.path.splitext(out)[0] + "_photos"
    for name in places:
        os.makedirs(os.path.join(photos, name.replace(" ", "_")), exist_ok=True)
    ser = open_master(args.port, args.baud)
    keep_awake(True)
    try:
        time.sleep(2.0)
        axis = Axis(ser, args.node, args.sub, args.stepper)
        status = axis.status()
        print(f"axis status (node {args.node}): {status}")
        current = status["position"]

        print(f"current = {current}, calibration slide = {args.slide_pos}, sample = {args.sample_pos}, "
              f"{visits} visits every {args.interval:g} s ({args.hours:g} h), "
              f"speed = {args.speed}, accel = {args.accel or 'default'}, open loop")
        if args.compensate:
            print(f"compensation: encoder more than {args.comp_counts} counts off its reference, "
                  f"up to {max_comp} steps at once and {args.max_offset} in total; above "
                  f"{args.lost_steps} steps = lost-step event, recovered at "
                  f"{min(args.speed, args.recover_speed)} steps/s in chunks of "
                  f"{args.recover_chunk} steps, the run ends at the first chunk the "
                  f"encoder does not follow. The first leg to each place (its encoder "
                  f"reference) is driven at {min(args.speed, args.recover_speed)} steps/s")
        if status["calibrated"] and abs(status["posErr"]) > 10000:
            # more than the firmware's scale error can explain: lost steps
            raise SystemExit(
                f"the step counter ({current}) and the encoder ({status['measured']}) disagree "
                f"by {status['posErr']} steps: X lost steps and the counter was not re-synced, "
                f"the two places are not where the counter says. Run with --resync first")
        for name, pos in places.items():
            if abs(pos - current) > args.limit:
                raise SystemExit(f"{name} position {pos} is more than {args.limit} steps "
                                 f"away from the axis ({current}), see --limit")

        def y_to(target):
            """Open-loop Y move (no encoder), returns the Y step position."""
            return yaxis.move_to(target, args.y_speed, args.y_accel, 0, 0.5,
                                 30 + 3 * abs(target - ypos[0]) / args.y_speed, encoder=False)[0]

        def z_to(target):
            """Open-loop focus move (no encoder), returns the Z step position."""
            return zaxis.move_to(target, args.z_speed, args.z_accel, 0, 0.5,
                                 30 + 3 * abs(target - zpos[0]) / args.z_speed, encoder=False)[0]

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
        if args.safe_z is None:
            print("no safe Z set: X and Y move with Z at the focus of the place they leave")
        else:
            zpos[0] = z_to(args.safe_z)
        if status["axismode"] != 0:  # this script compensates, not the firmware
            print(f"axismode was {status['axismode']}: setting it to 0 (open loop)")
            axis.set_mode(0)

        with open(out, "w", newline="") as f:
            f.write(f"# test=24hrstest_can_linear_x port={args.port} node={args.node} "
                    f"started={datetime.now().isoformat(timespec='seconds')}\n")
            f.write(f"# slide_pos={args.slide_pos} sample_pos={args.sample_pos} "
                    f"interval={args.interval:g} hours={args.hours:g} speed={args.speed} "
                    f"accel={args.accel} mode=0 camera={camera.settings}\n")
            f.write(f"# slide_y={args.slide_y} sample_y={args.sample_y} y_node={args.y_node} "
                    f"y_speed={args.y_speed} y_accel={args.y_accel}\n")
            f.write(f"# slide_z={args.slide_z} sample_z={args.sample_z} safe_z={args.safe_z} "
                    f"z_node={args.z_node} z_speed={args.z_speed} z_accel={args.z_accel}\n")
            f.write(f"# compensate={args.compensate} comp_counts={args.comp_counts} "
                    f"comp_tries={args.comp_tries} preload={args.preload} "
                    f"lost_steps={args.lost_steps} recover_speed={args.recover_speed} "
                    f"recover_chunk={args.recover_chunk} "
                    f"max_comp={max_comp} max_offset={args.max_offset}\n")
            w = csv.writer(f)
            # the columns of a lost-step event, empty in the other rows (the start
            # and the duration of the X move and compTries are always filled in)
            event_cols = ["event", "xMoveStart", "xMoveSec", "lostSteps", "travelledPct",
                          "stallRawCounts", "stallPos", "stallAfterSec", "stallAfterSteps",
                          "compTries", "compSpeed"]
            w.writerow(["time_iso", "speed", "visit", "place", "move", "target",
                        "commanded", "rawCounts", "measured", "posErr",
                        "encDriftCounts", "compSteps", "residualCounts", "offsetSteps",
                        "y", "z", "photo"] + event_cols)
            f.flush()
            if os.path.exists(trace_path):  # left over from a run with the same --out
                os.remove(trace_path)
            lost_events = [0]
            raw0 = {}      # place -> raw encoder count at the first arrival (the reference)
            offset = [0]   # steps added to every X target: the sum of our corrections
            # every place is reached coming from the other one
            side = {"calibration slide": 1 if args.sample_pos > args.slide_pos else -1,
                    "sample": 1 if args.slide_pos > args.sample_pos else -1}
            fw_steps_per_count = 65536 / (axis.sdo(OD_CPS_Q16) or 65536)  # signed

            def steps_per_count():
                """Steps per encoder count from our own two references (the
                travel between the two places), the firmware's value until then."""
                if len(raw0) == 2 and raw0["calibration slide"] != raw0["sample"]:
                    return (args.slide_pos - args.sample_pos) / (raw0["calibration slide"] - raw0["sample"])
                return fw_steps_per_count

            def x_to(x, distance, speed=None, trace=None):
                speed = speed or args.speed
                return axis.move_to(x, speed, args.accel, 0, 1.0,
                                    30 + 3 * abs(distance) / speed, trace=trace)

            def recover_to(goal, speed, visit, place, k):
                """Lost-step recovery: to `goal` in checked chunks of
                --recover-chunk steps. Returns what is wrong, or None."""
                t_rec, rec = datetime.now(), []
                x0, r0 = axis.sdo(OD_POSITION), axis.sdo(OD_RAW)
                try:
                    problem = chunked_move(axis, goal, speed, args.accel, args.recover_chunk, k,
                                           f"recovery at '{place}'", rec)
                finally:
                    save_trace(t_rec, visit, place, "recover", rec, x0, r0, k)
                return problem and problem + ", the run ends here"

            def lag(sample, x_start, raw_start, k):
                """Steps the step counter has done since the start of a move
                minus what the encoder has seen, at one sample of its trace."""
                return round((sample[1] - x_start) - (sample[2] - raw_start) * k)

            def save_trace(t_move, visit, place, phase, trace, x_start, raw_start, k):
                """Append the samples of one X move to <csv name>_lost_steps.csv."""
                new = not os.path.exists(trace_path)
                with open(trace_path, "a", newline="") as tf:
                    tw = csv.writer(tf)
                    if new:
                        tw.writerow(["moveStart_iso", "visit", "place", "phase", "t_s",
                                     "counter", "rawCounts", "lagSteps"])
                    for s in trace:
                        tw.writerow([t_move.isoformat(timespec="milliseconds"), visit, place,
                                     phase, s[0], s[1], s[2], lag(s, x_start, raw_start, k)])

            def report_lost(ev, visit, place, target, t_move, trace, x_start, raw_start, raw,
                            k, sent, err):
                """Print and record a lost-step event: the move of `sent` steps left
                the stage `err` steps away from the place (the motor stalled)."""
                lost_events[0] += 1
                short = round(-err if sent > 0 else err)  # > 0: stopped before the place
                ev.update(event="LOST_STEPS", lostSteps=short, stallRawCounts=raw,
                          stallPos=round(target + err),
                          travelledPct=round(100 * (1 - short / abs(sent)), 1) if sent else "")
                last = (0.0, x_start)  # last sample at which the encoder still followed
                for s in trace:
                    if abs(lag(s, x_start, raw_start, k)) > min(10000, abs(err) / 2):
                        ev.update(stallAfterSec=last[0], stallAfterSteps=abs(last[1] - x_start))
                        break
                    last = s
                print(f"    ! LOST STEPS on the way to '{place}' (visit {visit}): the move "
                      f"of {sent:+d} steps started {t_move:%m-%d %H:%M:%S} and took "
                      f"{ev['xMoveSec']} s")
                print(f"    !   {abs(short)} steps {'missing' if short > 0 else 'too far'}, the "
                      f"stage did {ev['travelledPct']} % of the move")
                print(f"    !   it stopped at encoder {raw} = step position {round(target + err)} "
                      f"(the place is at {target})")
                if ev["stallAfterSec"] != "":
                    print(f"    !   the encoder still followed {ev['stallAfterSec']} s / "
                          f"{ev['stallAfterSteps']} steps into the move, at the next sample "
                          f"it did not")
                for s in trace:
                    print(f"    !     t = {s[0]:5.2f} s  counter {s[1]:>8}  encoder {s[2]:>7}  "
                          f"lag {lag(s, x_start, raw_start, k):>7} steps")
                save_trace(t_move, visit, place, "move", trace, x_start, raw_start, k)

            def go(visit, place, photo=True, into_focus=False):
                target = places[place]
                move = target - prev[0]
                if args.safe_z is not None:  # retract, so nothing can touch while X / Y move
                    zpos[0] = z_to(args.safe_z)
                x_start, raw_start = axis.sdo(OD_POSITION), axis.sdo(OD_RAW)
                t_move, trace = datetime.now(), []
                # the first arrival at a place is its encoder reference: go there
                # slowly, lost steps on this leg could not be compensated
                slow = min(args.speed, args.recover_speed) if place not in raw0 else None
                commanded, measured, pos_err, raw = x_to(target + offset[0], move, slow, trace)
                prev[0] = target
                ev = dict.fromkeys(event_cols, "")
                ev["xMoveStart"] = t_move.isoformat(timespec="milliseconds")
                ev["xMoveSec"] = round((datetime.now() - t_move).total_seconds(), 1)
                abort = None  # set: the row of this visit is written, then the run ends
                enc_drift = resid = comp = ""
                sent, seen = commanded - x_start, raw - raw_start
                comp0 = tries0 = 0  # what a repeated first leg added
                if args.compensate and place not in raw0:
                    # This arrival becomes the encoder reference of the place, so the
                    # encoder must have seen the steps of the leg. If not: back to
                    # where the leg started (that count is known), and once more.
                    k = steps_per_count()
                    while abs(seen * k - sent) > 0.05 * abs(sent) + 200:
                        if tries0:
                            abort = (f"first arrival at '{place}': {sent} steps were sent, but the "
                                     f"encoder saw {seen} counts = {round(seen * k)} steps, at the "
                                     f"second attempt too. The encoder reference of this place "
                                     f"cannot be taken, the run ends here")
                            save_trace(t_move, visit, place, "move again", trace, x_start, raw_start, k)
                            break
                        report_lost(ev, visit, place, target, t_move, trace, x_start, raw_start,
                                    raw, k, sent, seen * k - sent)
                        tries0 = 1
                        speed = ev["compSpeed"] = min(args.speed, args.recover_speed)
                        print(f"    back to where the leg started (encoder {raw_start}) at {speed} "
                              f"steps/s, then the leg once more")
                        t_rec, rec, x0, r0 = datetime.now(), [], commanded, raw
                        try:
                            try:
                                abort = to_raw(axis, raw_start, k, abs(k), speed, args.accel,
                                               args.recover_chunk,
                                               f"recovery on the way to '{place}'", rec)
                            finally:
                                save_trace(t_rec, visit, place, "recover", rec, x0, r0, k)
                            if abort:
                                abort += ", the run ends here"
                                break
                            pos = axis.sdo(OD_POSITION)  # the counter is off by what was lost
                            comp0 += pos - x_start
                            offset[0] += pos - x_start
                            x_start, raw_start = pos, axis.sdo(OD_RAW)
                            t_move, trace = datetime.now(), []
                            commanded, measured, pos_err, raw = x_to(target + offset[0], move,
                                                                     speed, trace)
                        except (RuntimeError, TimeoutError) as e:
                            abort = f"recovery on the way to '{place}' failed, the run ends here: {e}"
                            break
                        sent, seen = commanded - x_start, raw - raw_start
                        print(f"    !   again: {sent:+d} steps sent, the encoder saw {seen:+d} counts "
                              f"= {round(seen * k):+d} steps (offset now {offset[0]:+d})")
                    ev["compTries"] = tries0
                if not abort:
                    enc_drift = resid = raw - raw0.setdefault(place, raw)
                    comp, tries = comp0, tries0
                    k = steps_per_count()
                    while args.compensate and abs(resid) > args.comp_counts and tries < args.comp_tries:
                        step = -round(resid * k)
                        lost = abs(step) > args.lost_steps
                        if lost and ev["event"]:
                            abort = (f"'{place}' is still {resid:+d} counts ({abs(step)} steps) off "
                                     f"after the recovery, the run ends here")
                            break
                        if lost:  # the motor stalled in this move: when, where, how much
                            report_lost(ev, visit, place, target, t_move, trace, x_start, raw_start,
                                        raw, k, sent, -step)
                        if abs(step) > max_comp or abs(offset[0] + step) > args.max_offset:
                            abort = (f"encoder at '{place}' is {resid} counts off its reference: that "
                                     f"needs {step} steps (limit {max_comp}, --max-comp) and a "
                                     f"total offset of {offset[0] + step} steps (limit {args.max_offset}, "
                                     f"--max-offset). Not compensating, the run ends here")
                            break
                        offset[0] += step
                        comp += step
                        tries += 1
                        # after a lost-step event everything at this visit is slow
                        speed = min(args.speed, args.recover_speed) if ev["event"] else args.speed
                        print(f"    compensating '{place}': encoder {resid:+d} counts off -> "
                              f"{step:+d} steps (offset now {offset[0]:+d})"
                              + (f", at {speed} steps/s" if ev["event"] else ""))
                        approach = target + offset[0] + side[place] * args.preload
                        x0, r0 = commanded, raw
                        try:
                            if lost:
                                ev["compSpeed"] = speed
                                abort = recover_to(approach, speed, visit, place, k)
                                if abort:
                                    break
                            else:
                                x_to(approach, abs(step) + args.preload, speed)
                            commanded, measured, pos_err, raw = x_to(target + offset[0],
                                                                     args.preload, speed)
                        except (RuntimeError, TimeoutError) as e:
                            abort = f"compensation at '{place}' failed, the run ends here: {e}"
                            break
                        resid = raw - raw0[place]
                        if lost:
                            print(f"    !   recovery: counter {x0} -> {commanded}, encoder "
                                  f"{r0} -> {raw}, now {resid:+d} counts off the reference")
                    ev["compTries"] = tries
                    if not abort and args.compensate and abs(resid) > args.comp_counts:
                        if abs(resid * k) > args.lost_steps:
                            abort = (f"'{place}' is still {resid:+d} counts ({round(abs(resid * k))} "
                                     f"steps) off after {tries} corrections: the axis does not get "
                                     f"there, the run ends here")
                        else:
                            print(f"    ! WARNING: '{place}' still {resid:+d} counts off after "
                                  f"{tries} corrections")
                if abort:  # the row shows where the axis is now
                    try:
                        commanded, measured, pos_err, raw = axis.feedback()
                        if place in raw0:
                            resid = raw - raw0[place]
                    except RuntimeError:
                        pass
                if not abort:
                    ypos[0] = y_to(ys[place])
                    if photo or into_focus or args.safe_z is None:  # into focus (from the safe Z, if any)
                        zpos[0] = z_to(focus[place])
                now = datetime.now()
                path = ""
                if photo and not abort:
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
                      f"{commanded:>10} {measured:>10} {pos_err:>7} {raw:>10} "
                      f"{enc_drift:>8} {comp:>6} {resid:>6} {ypos[0]:>8} {zpos[0]:>8}")
                w.writerow([now.isoformat(timespec="milliseconds"), args.speed, visit, place,
                            move, target, commanded, raw, measured, pos_err,
                            enc_drift, comp, resid, offset[0], ypos[0], zpos[0], path]
                           + [ev[c] for c in event_cols])
                f.flush()
                if abort:
                    raise SystemExit(abort)

            print(f"\n{'time':>14} {'visit':>5} {'place':>17} {'move':>8} {'target':>10} "
                  f"{'commanded':>10} {'measured':>10} {'posErr':>7} {'rawCounts':>10} {'encDrift':>8} {'comp':>6} {'resid':>6} {'y':>8} {'z':>8}")
            prev = [current]
            # every place is reached from the other one, the very first time too:
            # if X starts close to the sample, back off towards the slide first
            if (current - args.sample_pos) * side["sample"] < args.preload:
                back = args.sample_pos + side["sample"] * args.preload
                x_to(back, back - current)
                prev[0] = current = back
            go(-1, "sample", photo=False)

            t0 = time.time()
            for visit in range(visits):
                # fixed schedule from t0, so the move and photo times do not add up
                while time.time() < t0 + visit * args.interval:
                    time.sleep(min(1.0, max(0.0, t0 + visit * args.interval - time.time())))
                go(visit, "calibration slide" if visit % 2 == 0 else "sample")
            if args.return_to_slide and (visits - 1) % 2:
                # the last visit was the sample: end where the test started,
                # at the calibration slide (no photo, logged as one more row)
                go(visits, "calibration slide", photo=False, into_focus=True)
            print(f"\ndone, {visits} visits, {lost_events[0]} lost-step events"
                  + (f" (samples in {trace_path})" if lost_events[0] else ""))
            if lost_events[0]:
                # the firmware's step counter is off by what was compensated
                if prev[0] == args.slide_pos and abs(raw0["calibration slide"]) <= 50:
                    axis.sdo(OD_RESET, 1, "u8")  # at the calibration slide: counter := encoder
                    time.sleep(1.0)
                    print(f"step counter (it was {offset[0]:+d} steps off) set from the encoder: "
                          f"{axis.sdo(OD_POSITION)}")
                else:
                    print(f"WARNING: the step counter is {offset[0]:+d} steps off, run with "
                          f"--resync before the next run")
    except KeyboardInterrupt:
        print("\nstopped by Ctrl+C")
    finally:
        keep_awake(False)
        ser.close()
        camera.close()

    print(f"raw data saved to {out}, photos in {photos}")


if __name__ == "__main__":
    main()
