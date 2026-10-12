#!/usr/bin/env python3
"""
X-axis ultra test over the CAN bus: the 24 hour test (back and forth between
two positions, a photo at each visit) with random X moves between the visits.

Version for the linear (magnetic) encoder: 24hrstest_can_linear.py with the
random moves added. The first legs and the start check rely on the firmware's
encoder calibration (counts per step, posErr); ultra_test_can_rotational.py
does not.

Random moves: between two visits X makes --walk-min .. --walk-max (2 .. 5)
moves to random positions, one after the other, at --speed (100000) and
--accel (1000000). The targets are drawn anywhere between the two places, so the
moves have any length up to the whole travel, but never leave the two
places: outside them X hits something. The targets keep --walk-margin (2000)
steps away from both places. After the last one X goes back to the place of
the last photo (the move "back" in <csv name>_walk.csv: X only, no photo),
waits there and leaves for the other place when the visit is due, so the leg
to a place is always the whole travel, as in the 24 hour test. A random move
that would not be over by then, with the way back, is left out. --seed makes
the same random moves again (it is printed and written to the CSV),
--dry-run only prints them.
After every random move the raw encoder count is compared with the count
that target has by the encoder references of the two places (errCounts in
<csv name>_walk.csv, one row per random move). That is only recorded: at
the place the count is compared with its reference again, and between the
places a few counts off need not be a position error (in the run of
2026-10-07 the linear encoder was up to 50 um off the straight line between
the two places in the middle of the travel; with the offset taken from that,
nearly every visit had to take the same steps out again).
--walk-comp-counts N takes the offset from the encoder when a random move
ends more than N counts off, so the next move starts from where the stage
really is (walkComp of a visit = the steps this added during the random
moves before it). Without it only a move that lost steps does that. X does
not approach the random target again.
The way back is handled like a random move; its count is compared with the
reference of that place (errCounts = how far off X arrived there).
More than --lost-steps (2000) steps off = the move lost steps: it is printed
and recorded like a lost-step event of a leg (in the row of the next visit,
walkLost = which move: i/n or back), no more moves are made in this interval
(X does not go back either), X waits where it stopped and goes to the place
when the visit is due, at
--recover-speed. That leg is sent by the step counter: without the offset
from the encoder it would end as far from the place as the counter is off -
beyond the place, if the steps were lost moving away from it.
The run ends without another move if the encoder count after a random move
is more than --walk-stop-counts (500) counts outside the counts of the two
places (from the count of the one place - 500 to the count of the other
+ 500): X must not leave them. Inside that range the run goes on.
walkMoves (done/planned) of a visit are the random moves before it,
walkTravel their steps and those of the way back.
The 24 h version is the default: a visit every 10 min for 24 h. --quick: a
visit every 60 s for half an hour. --interval and --hours override both.

Same test as 24hrstest.py, but the PC is connected to the USB port of the
CAN MASTER and the X axis + encoder sit on a CAN slave (--node, default 11).
Moves go through the master's /motor_act (the master forwards them as SDO
writes), everything else is read and written in the slave's object
dictionary through the master's SDO bridge
({"task": "/can_act", "sdo": {...}}): position 0x2001, running 0x2004,
measured 0x2040, posErr 0x2041, axis mode 0x2042, health 0x2043,
fault 0x2044, calibrated 0x2046, calibrate 0x2048, counts per step 0x2049,
backlash 0x204A, raw counts 0x204B.
--status only prints the axis status read over CAN and does not move.

Two absolute positions are given: --slide-pos (the calibration slide) and
--sample-pos (the sample). Every --interval seconds
(10 min, with --quick 60) the axis moves to the other position and a
photo is taken there, for --hours hours (24, with --quick 0.5): calibration slide,
sample, calibration slide, sample, ... With the defaults that is 145 photos, 73 of the
calibration slide and 72 of the sample; with --quick 31.

All moves are done open loop (--mode 0, the only mode of this test): the
firmware does not use the encoder, this script does the compensation with
its own data.
The raw encoder count at the first arrival at each place is the reference
for that place. At every later arrival the count is compared with it
(encDriftCounts). If it is more than --comp-counts (4) counts off, all X
targets are shifted by the missing steps (offsetSteps), the axis backs off
--preload steps towards the place it came from and approaches again, so the
backlash stays the same; this is repeated up to --comp-tries (3) times
(compSteps = what was added at this visit, residualCounts = what is left).
The steps per count come from our own references (the travel between the two
places divided by the counts between them), not from the firmware's
calibration. --no-compensate only records.

Lost steps: the encoder is trusted. A correction of more than --lost-steps
(2000) steps means the motor stalled during the move while the step counter
ran on without it. It is compensated like any other error, but slowly and
checked: at --recover-speed (10000), in chunks of --recover-chunk (5000)
steps up to the approach point. After every chunk the encoder must have seen
the steps; at the first chunk it does not follow nothing more is sent and
the run ends (so a jammed stage or a dead encoder is pushed one chunk at
most). The run also ends if the place is still more than --lost-steps off
after the recovery, or if an SDO read or a move fails during it. What happened
is printed and recorded in the row of that visit: event = LOST_STEPS,
xMoveStart / xMoveSec (when the move started and how long it took),
lostSteps and travelledPct (how much of the move is missing), stallRawCounts
and stallPos (where the stage stopped: encoder count and step position),
stallAfterSec / stallAfterSteps (the last sample taken during the move at
which the encoder still followed the step counter), compTries, compSpeed,
compSteps, residualCounts and offsetSteps (how it was compensated). The
samples taken during the lost move and the state after every recovery chunk
(time, step counter, encoder) go to <csv name>_lost_steps.csv.
The first leg to each place is driven at --recover-speed too: that arrival
is the encoder reference of the place, so there is no count yet to
compensate to. If the encoder did not see the steps of that leg, X goes back
to where the leg started (that count is known) in checked chunks and the leg
is driven once more; a second failure ends the run. After a run with
lost-step events the firmware's step counter
is off by offsetSteps; at the end of the run, back at the calibration slide, it is
set from the encoder again. If a run ended somewhere else (error, Ctrl+C),
the next run refuses to start (step counter and encoder disagree): --resync
then brings X back to the calibration slide by the encoder (raw count
--resync-raw, slowly, in checked chunks), sets the step counter from the
encoder there and exits.
The run only ends when the first arrival at a place (its reference) did not
follow the steps, when a single correction is above --max-comp steps
(default: 1.1 x the travel between the two places, more cannot be lost in
one move), when the total is above --max-offset steps, or when the place is
still not reached after --comp-tries corrections. The row of that visit is
written before the run ends.

Safety, for an unattended run:
  - nothing moves before the camera and all three axes have answered and all
    targets passed the --limit / --z-limit checks;
  - the order is always X, then Y, then Z into focus (with a safe Z: Z
    retracts first); every move waits until the axis has stopped at its
    target before the next one starts. Z only ever goes to the two focus
    positions (and the safe Z, if set);
  - any SDO failure, move timeout or latched axis fault ends the run; the
    axes stay where they are, nothing is retried and no fault is cleared;
  - only a failed photo is skipped.

The two places are 3D positions (x, y, z), set as the constants SLIDE_XYZ
and SAMPLE_XYZ at the top of this file (the command line overrides them).
Y (--y-node, default 12) and the focus Z (--z-node, default 13) are further
CAN slaves, moved open loop (no encoder). At every visit: Z retracts to
SAFE_Z / --safe-z (if one is set), X moves, Y moves, Z goes to the focus of
that place, then the photo is taken. With a safe Z, Z is also retracted
before the calibration sweep and always comes into focus from the safe
position, so its backlash is the same at every photo. Without one, Z stays
in focus while X and Y move. After the last visit the stage goes back to the
calibration slide (x, y, z in focus), see --no-return-to-slide; if the run ends
early (error, Ctrl+C) the axes stay where they are.

Backlash: the calibration slide position is always approached from the sample position
and the other way round, also the very first time, so every photo of one
position is taken after the same leg.

The photos are taken with the Hikrobot camera (needs the MVS SDK installed,
and the MVS client must not have the camera open) and saved as
<csv name>_photos/calibration_slide/0000_calibration_slide_<time>.png and .../sample/0001_sample_<time>.png,
where 24hrstest_shift.py looks for them (the first photo of a place is its
reference). Exposure, gain etc. are used as they are set in the
camera unless --gain is given. --photo-test only takes one photo, to check
the camera and the picture, and does not move the axis.
A full-size png is about 30 MB, so 24 h need about 5 GB of disk space.

The PC is kept awake while the script runs. Stop early with Ctrl+C.

Connect to the USB port of the CAN master.

Usage:
    uv run tools/PYTHON/encoder_test/ultra_test_can_linear.py --seed 42 --port COM6
    uv run tools/PYTHON/encoder_test/ultra_test_can_linear.py --photo-test --gain 23
    uv run tools/PYTHON/encoder_test/ultra_test_can_linear.py --port COM7 --status
    uv run tools/PYTHON/encoder_test/ultra_test_can_linear.py --seed 42 --dry-run
    uv run tools/PYTHON/encoder_test/ultra_test_can_linear.py --port COM7 --gain 23
    uv run tools/PYTHON/encoder_test/ultra_test_can_linear.py --port COM7 --gain 23 --quick

Output: the table is printed and also saved as CSV, one row per visit.
Lines starting with '#' are run metadata (pandas: read_csv(..., comment='#')).
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

# results (CSV, photos) go to <repo>/encoder_test_data, which git ignores
DATA_DIR = os.path.normpath(os.path.join(
    os.path.dirname(os.path.abspath(__file__)), "..", "..", "..", "encoder_test_data"))

# ---- the two places: absolute step positions (x, y, z) ----------------------
# Used when --slide-pos/-y/-z and --sample-pos/-y/-z are not given.
# The places of the 24 h rotational run that ended on 2026-10-07
# (24hrstest_can_rotational_x_20261006_135353), taught on 2026-10-06. The
# sample is -296064 steps in X away from the calibration slide.
SLIDE_XYZ = (-736122, 328450, 41712)    # calibration slide
SAMPLE_XYZ = (-1032186, 327942, 43462)  # sample
# None = Z is not retracted: X and Y move with Z still at the focus of the
# place they leave, then Z goes to the new focus. X has been traversed by hand
# at both focus heights without touching anything, so this is the default.
# A number = Z goes there before X / Y move, and comes into focus afterwards.
SAFE_Z = None
# ------------------------------------------------------------------------------

MODES = {0: "OPEN_LOOP", 1: "MONITOR", 2: "CORRECT", 3: "SERVO"}
FAULTS = {0: "NONE", 1: "STALL", 2: "LOST_STEPS", 3: "DIVERGENCE", 4: "TIMEOUT",
          5: "CAL_INVALID", 6: "CAL_FAILED", 7: "ENC_NOISE"}
# CORRECT / SERVO adopt the encoder position as the new step counter, so the
# move ends near the target, not exactly on it
ARRIVE_TOL = 200
# the firmware's calibration sweeps preload 5000 + 10 x 5000 steps in + and back
CAL_SWEEP = 55000

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
        backlash). The axis sweeps +55000 steps from where it is and comes back.
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

    def move_to(self, target, speed, accel, tol=0, settle=1.0, timeout=60.0, encoder=True,
                trace=None):
        """Absolute move in the axis's current mode, wait until arrived,
        return (commanded, measured, posErr, rawCounts). encoder=False is for
        an axis without encoder (focus): only the step position is used.
        trace: a list, gets (seconds since the start, step position, raw
        encoder count) for every poll during the move."""
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
                if time.time() - arrived > settle:  # corrections / adoption follow the move
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
                    help="mode 0: correct X ourselves when the encoder at a place is "
                         "off its first arrival there (--no-compensate: only record)")
    ap.add_argument("--comp-counts", type=int, default=4,
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
    ap.add_argument("--max-drift", type=int, default=80,
                    help="closed-loop runs only: largest posErr change allowed on "
                         "the open-loop check leg. The firmware latches a "
                         "LOST_STEPS fault as soon as posErr passes its lag limit "
                         "(about 136 steps with the present calibration)")
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
    ap.add_argument("--interval", type=float, default=None,
                    help="seconds between two visits (default: 600 = 10 min, with "
                         "--quick 60)")
    ap.add_argument("--hours", type=float, default=None,
                    help="length of the test (default: 24, with --quick 0.5)")
    ap.add_argument("--return-to-slide", action=argparse.BooleanOptionalAction, default=True,
                    help="after the last visit go back to the calibration slide (x, y, z), "
                         "if the test did not end there anyway")
    ap.add_argument("--quick", action=argparse.BooleanOptionalAction, default=False,
                    help="quick version: a visit every 60 s for half an hour "
                         "(default: every 10 min for 24 h). --interval and "
                         "--hours override both")
    ap.add_argument("--walk-min", type=int, default=2,
                    help="random X moves between two visits: at least this many")
    ap.add_argument("--walk-max", type=int, default=5,
                    help="random X moves between two visits: at most this many")
    ap.add_argument("--walk-margin", type=int, default=2000,
                    help="the random targets stay this many steps inside the two "
                         "places (not less than --lost-steps: until the step counter "
                         "is that far off, no lost-step event is seen)")
    ap.add_argument("--walk-stop-counts", type=int, default=500,
                    help="the run ends if the encoder count after a random move is "
                         "more than this many counts outside the counts of the two "
                         "places: X must stay between them")
    ap.add_argument("--walk-comp-counts", type=int, default=None,
                    help="take the offset from the encoder when a random move ends "
                         "more than this many counts off (default: only when the "
                         "move lost steps, --lost-steps)")
    ap.add_argument("--seed", type=int, default=None,
                    help="random seed of the random moves (default: picked and printed)")
    ap.add_argument("--dry-run", action="store_true",
                    help="only print the random targets of every interval: no port, "
                         "no camera, nothing moves")
    ap.add_argument("--limit", type=int, default=400000,
                    help="refuse X / Y positions farther than this from where "
                         "the axis is now (guards against a typo)")
    ap.add_argument("--mode", type=int, choices=sorted(MODES), default=0,
                    help="axis mode for every move: 0 = open loop (default; the "
                         "encoder is only recorded), 1 = monitor, 2 = correct, "
                         "3 = servo. In 1..3 the firmware stops the axis when "
                         "posErr passes its lag limit")
    ap.add_argument("--calibrate", action=argparse.BooleanOptionalAction, default=None,
                    help="run the firmware's encoder calibration first (default: "
                         "yes if --mode is 1..3, --no-calibrate to skip)")
    ap.add_argument("--speed", type=int, default=100000)
    ap.add_argument("--accel", type=int, default=1000000,
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
    if not args.port and not args.dry_run:
        ap.error("--port is required")
    if args.slide_pos == args.sample_pos:
        ap.error("--slide-pos and --sample-pos are the same")
    if args.interval is None:
        args.interval = 60.0 if args.quick else 600.0
    if args.hours is None:
        args.hours = 0.5 if args.quick else 24
    if args.interval <= 0 or args.hours <= 0:
        ap.error("--interval and --hours must be > 0")
    if args.mode:
        ap.error("the ultra test is open loop only (--mode 0): the random moves are "
                 "checked and compensated with the raw encoder count")
    if not 0 <= args.walk_min <= args.walk_max:
        ap.error("need 0 <= --walk-min <= --walk-max")
    # the random moves stay between the two places: outside them X hits something
    lo, hi = sorted((args.slide_pos, args.sample_pos))
    walk_lo, walk_hi = lo + args.walk_margin, hi - args.walk_margin
    if args.walk_margin < args.lost_steps:
        ap.error("--walk-margin must not be less than --lost-steps")
    if walk_lo >= walk_hi:
        ap.error("--walk-margin does not leave a window between the two places")
    if args.seed is None:
        args.seed = random.SystemRandom().randrange(2 ** 32)
    rng = random.Random(args.seed)

    def walk_plan():
        """The random targets of one interval: --walk-min .. --walk-max
        positions anywhere in the window between the two places."""
        plan = [rng.randint(walk_lo, walk_hi)
                for _ in range(rng.randint(args.walk_min, args.walk_max))]
        assert all(lo < t < hi for t in plan)  # whatever the options were
        return plan
    if args.safe_z is not None and min(args.slide_z, args.sample_z) < args.safe_z \
            < max(args.slide_z, args.sample_z):
        ap.error("--safe-z lies between the two focus positions, so it is closer "
                 "than the focus of one of the places")
    if args.calibrate is None:
        args.calibrate = args.mode > 0
    max_comp = args.max_comp or round(1.1 * abs(args.slide_pos - args.sample_pos))
    out = args.out or os.path.join(DATA_DIR, f"ultra_test_can_linear_x_{datetime.now():%Y%m%d_%H%M%S}.csv")
    trace_path = os.path.splitext(out)[0] + "_lost_steps.csv"
    walk_path = os.path.splitext(out)[0] + "_walk.csv"
    places = {"calibration slide": args.slide_pos, "sample": args.sample_pos}
    ys = {"calibration slide": args.slide_y, "sample": args.sample_y}
    focus = {"calibration slide": args.slide_z, "sample": args.sample_z}
    visits = round(args.hours * 3600 / args.interval) + 1 if args.quick \
        else int(args.hours * 3600 / args.interval) + 1
    if args.dry_run:
        print(f"seed = {args.seed}, random targets inside {walk_lo} .. {walk_hi} "
              f"(calibration slide = {args.slide_pos}, sample = {args.sample_pos})")
        for visit in range(1, visits):
            print(f"before visit {visit:>4} ({'calibration slide' if visit % 2 == 0 else 'sample':>17}): "
                  + "  ".join(str(t) for t in walk_plan()))
        return

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
              f"speed = {args.speed}, accel = {args.accel or 'default'}, "
              f"mode = {args.mode} ({MODES[args.mode]})")
        if args.mode == 0 and args.compensate:
            print(f"compensation: encoder more than {args.comp_counts} counts off its reference, "
                  f"up to {max_comp} steps at once and {args.max_offset} in total; above "
                  f"{args.lost_steps} steps = lost-step event, recovered at "
                  f"{min(args.speed, args.recover_speed)} steps/s in chunks of "
                  f"{args.recover_chunk} steps, the run ends at the first chunk the "
                  f"encoder does not follow. The first leg to each place (its encoder "
                  f"reference) is driven at {min(args.speed, args.recover_speed)} steps/s")
        if args.mode == 0 and status["calibrated"] and abs(status["posErr"]) > 10000:
            # more than the firmware's scale error can explain: lost steps
            raise SystemExit(
                f"the step counter ({current}) and the encoder ({status['measured']}) disagree "
                f"by {status['posErr']} steps: X lost steps and the counter was not re-synced, "
                f"the two places are not where the counter says. Run with --resync first")
        print(f"random walk: {args.walk_min} .. {args.walk_max} moves between two visits, "
              f"targets inside {walk_lo} .. {walk_hi}, then back to the place of the last "
              f"photo, seed = {args.seed}")
        print("after a random move the offset is taken from the encoder "
              + ("only if the move lost steps" if args.walk_comp_counts is None
                 else f"if it is more than {args.walk_comp_counts} counts off"))
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
            # retract before anything moves in X (the calibration sweep too)
            zpos[0] = z_to(args.safe_z)

        cal = None
        if args.calibrate:
            print(f"calibrating the encoder (the axis sweeps +{CAL_SWEEP} steps and back) ...")
            cal = axis.calibrate()
            print(f"calibration ok: countsPerStep = {cal['countsPerStep']:.5f}, "
                  f"countSign = {cal['countSign']}, backlash = {cal['backlashSteps']} steps")
            status = axis.status()
        elif args.mode:
            # the two places were taught in as step positions: re-anchor the encoder
            # to the step counter (no motion), so an old posErr is not "corrected"
            print(f"posErr was {status['posErr']} steps: re-anchoring the encoder to the step counter")
            axis.sdo(OD_RESET, 2, "u8")
            time.sleep(1.0)
            status = axis.status()
        if status.get("fault") == 5:
            print("WARNING: the firmware reports CAL_INVALID - its calibration was only "
                  "rescaled after a microstep change, run with --calibrate")
        if args.mode and not status.get("calibrated"):
            raise SystemExit("the axis is not calibrated: the firmware would run every move "
                             "open loop. Run with --calibrate")

        with open(out, "w", newline="") as f, open(walk_path, "w", newline="") as wf:
            f.write(f"# test=ultra_test_can_linear_x port={args.port} node={args.node} "
                    f"started={datetime.now().isoformat(timespec='seconds')}\n")
            f.write(f"# slide_pos={args.slide_pos} sample_pos={args.sample_pos} "
                    f"interval={args.interval:g} hours={args.hours:g} speed={args.speed} "
                    f"accel={args.accel} mode={args.mode} camera={camera.settings}\n")
            f.write(f"# slide_y={args.slide_y} sample_y={args.sample_y} y_node={args.y_node} "
                    f"y_speed={args.y_speed} y_accel={args.y_accel}\n")
            f.write(f"# slide_z={args.slide_z} sample_z={args.sample_z} safe_z={args.safe_z} "
                    f"z_node={args.z_node} z_speed={args.z_speed} z_accel={args.z_accel}\n")
            f.write(f"# compensate={args.compensate} comp_counts={args.comp_counts} "
                    f"comp_tries={args.comp_tries} preload={args.preload} "
                    f"lost_steps={args.lost_steps} recover_speed={args.recover_speed} "
                    f"recover_chunk={args.recover_chunk} "
                    f"max_comp={max_comp} max_offset={args.max_offset}\n")
            f.write(f"# walk_min={args.walk_min} walk_max={args.walk_max} "
                    f"walk_window={walk_lo}..{walk_hi} "
                    f"walk_stop_counts={args.walk_stop_counts} "
                    f"walk_comp_counts={args.walk_comp_counts} "
                    f"seed={args.seed}\n")
            if cal:
                f.write("# calibration " + " ".join(f"{k}={v}" for k, v in cal.items()) + "\n")
            w = csv.writer(f)
            # the columns of a lost-step event, empty in the other rows (the start
            # and the duration of the X move and compTries are always filled in)
            event_cols = ["event", "xMoveStart", "xMoveSec", "lostSteps", "travelledPct",
                          "stallRawCounts", "stallPos", "stallAfterSec", "stallAfterSteps",
                          "compTries", "compSpeed"]
            w.writerow(["time_iso", "speed", "visit", "place", "move", "target",
                        "commanded", "rawCounts", "measured", "posErr",
                        "encDriftCounts", "compSteps", "residualCounts", "offsetSteps",
                        "y", "z", "photo"] + event_cols
                       + ["walkMoves", "walkTravel", "walkComp", "walkLost"])
            f.flush()
            # one row per random move; errCounts / errSteps = where the encoder
            # says the stage is minus the target
            ww = csv.writer(wf)
            ww.writerow(["time_iso", "visit", "place", "i", "n", "move", "target",
                         "commanded", "rawCounts", "errCounts", "errSteps", "offsetSteps",
                         "moveSec", "event"])
            wf.flush()
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

            def x_to(x, mode, distance, speed=None, trace=None, settle=None):
                speed = speed or args.speed
                return axis.move_to(x, speed, args.accel, ARRIVE_TOL if mode >= 2 else 0,
                                    settle or (2.0 if mode >= 2 else 1.0),
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
                            k, sent, err, phase="move"):
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
                save_trace(t_move, visit, place, phase, trace, x_start, raw_start, k)

            def go(visit, place, mode=args.mode, photo=True, into_focus=False, wk=None):
                # wk: what walk() returned for the random moves before this visit
                target = places[place]
                move = target - prev[0]
                if args.safe_z is not None:  # retract, so nothing can touch while X / Y move
                    zpos[0] = z_to(args.safe_z)
                if mode != cur_mode[0]:  # over CAN a move follows the axis mode
                    axis.set_mode(mode)
                    cur_mode[0] = mode
                x_start, raw_start = axis.sdo(OD_POSITION), axis.sdo(OD_RAW)
                t_move, trace = datetime.now(), []
                # the first arrival at a place is its encoder reference: go there
                # slowly, lost steps on this leg could not be compensated. The leg
                # after a random move that lost steps is its recovery: slowly too
                slow = min(args.speed, args.recover_speed) \
                    if mode == 0 and (place not in raw0 or wk and wk["ev"]) else None
                commanded, measured, pos_err, raw = x_to(target + offset[0], mode, move, slow, trace)
                prev[0] = target
                ev = dict.fromkeys(event_cols, "")
                ev["xMoveStart"] = t_move.isoformat(timespec="milliseconds")
                ev["xMoveSec"] = round((datetime.now() - t_move).total_seconds(), 1)
                if wk and wk["ev"]:  # the random move that lost steps is the event of this visit
                    ev.update(wk["ev"], compSpeed=slow)
                abort = None  # set: the row of this visit is written, then the run ends
                # Our own compensation (open loop only): the encoder must read what
                # it read at the first arrival at this place. If it is off by more
                # than --comp-counts, shift all X targets by the missing steps and
                # approach the place again from the same side (same backlash).
                enc_drift = resid = comp = ""
                sent, seen = commanded - x_start, raw - raw_start
                comp0 = tries0 = 0  # what a repeated first leg added
                if mode == 0 and args.compensate and place not in raw0:
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
                            commanded, measured, pos_err, raw = x_to(target + offset[0], mode, move,
                                                                     speed, trace)
                        except (RuntimeError, TimeoutError) as e:
                            abort = f"recovery on the way to '{place}' failed, the run ends here: {e}"
                            break
                        sent, seen = commanded - x_start, raw - raw_start
                        print(f"    !   again: {sent:+d} steps sent, the encoder saw {seen:+d} counts "
                              f"= {round(seen * k):+d} steps (offset now {offset[0]:+d})")
                    ev["compTries"] = tries0
                if mode == 0 and not abort:
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
                                x_to(approach, mode, abs(step) + args.preload, speed)
                            commanded, measured, pos_err, raw = x_to(target + offset[0], mode,
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
                           + [ev[c] for c in event_cols]
                           + ([f"{wk['done']}/{wk['planned']}", wk["travel"], wk["comp"],
                               wk["lost"]] if wk else [""] * 4))
                f.flush()
                if abort:
                    raise SystemExit(abort)
                return pos_err

            def walk_sec(d):
                """Longest a random move of d steps takes: the ramps of the move,
                and the settling and the polling of x_to."""
                v, a = args.speed, args.accel or args.speed
                return (d / v + v / a if d * a >= v * v else 2 * math.sqrt(d / a)) + 3.0

            def walk(visit, place, due):
                """The random X moves before visit `visit` (to `place`, due at the
                time `due`): to the targets of walk_plan(), one after the other, as
                long as the next one and the way back are over before the visit is
                due, then back to the place of the last photo: X waits there, so
                the leg to `place` is the whole travel. After every move the
                encoder says where the stage is. A move that lost steps is the last
                one: X waits where it stopped. Returns what go() needs."""
                plan = walk_plan()
                last = "sample" if place == "calibration slide" else "calibration slide"  # the place of the last photo
                home = places[last]
                wk = {"planned": len(plan), "done": 0, "travel": 0, "comp": 0, "lost": "",
                      "ev": None}
                if args.safe_z is not None:  # retract, so nothing can touch while X moves
                    zpos[0] = z_to(args.safe_z)
                k = steps_per_count()
                raw_lo, raw_hi = sorted((raw0["calibration slide"], raw0["sample"]))
                for i, t in enumerate(plan + [home], 1):
                    back = i > len(plan)  # the last move: back to the place of the last photo
                    name = "back" if back else f"{i}/{len(plan)}"
                    what = (f"the way back to the {last}" if back else f"random move {name}") \
                        + f" before visit {visit}"
                    d = abs(t - prev[0])
                    if back and not d:  # no random move was made
                        break
                    if not back and time.time() + walk_sec(d) + walk_sec(abs(home - t)) > due:
                        print(f"    walk {name} left out: visit {visit} is due")
                        continue
                    x_start, raw_start = axis.sdo(OD_POSITION), axis.sdo(OD_RAW)
                    t_move, trace = datetime.now(), []
                    commanded, _, _, raw = x_to(t + offset[0], 0, d, trace=trace)
                    sec = round((datetime.now() - t_move).total_seconds(), 1)
                    # the count that is read against the count this target has by the
                    # encoder references of the two places (back at the place: against
                    # its own reference): the counts and the steps the stage is away
                    # from the target
                    resid = raw - (raw0[last] if back
                                   else round(raw0["calibration slide"] + (t - args.slide_pos) / k))
                    err = round(resid * k)
                    lost = abs(err) > args.lost_steps
                    print(f"    walk {name}: {t - prev[0]:>+8} -> {t:>10}  counter "
                          f"{commanded:>10}  encoder {raw:>10}  {resid:>+6} counts off")
                    ww.writerow([datetime.now().isoformat(timespec="milliseconds"), visit, place,
                                 name if back else i, len(plan), t - prev[0], t, commanded,
                                 raw, resid, err, offset[0], sec, "LOST_STEPS" if lost else ""])
                    wf.flush()
                    wk["done"], wk["travel"] = wk["done"] + (not back), wk["travel"] + d
                    prev[0] = t
                    # X must not leave the two places: with the encoder more than
                    # --walk-stop-counts outside their counts the run ends
                    if not raw_lo - args.walk_stop_counts <= raw <= raw_hi + args.walk_stop_counts:
                        raise SystemExit(
                            f"{what}: the encoder ({raw}) is more than "
                            f"{args.walk_stop_counts} counts outside the "
                            f"two places (calibration slide {raw0['calibration slide']}, sample {raw0['sample']}). "
                            f"Nothing more is sent, X stays where it is; check where it is "
                            f"before the next run")
                    if lost and not args.compensate:
                        raise SystemExit(
                            f"{what} lost steps: the stage is {err:+d} steps "
                            f"from where the step counter says. Not "
                            f"compensating (--no-compensate), the run ends here")
                    off = (args.walk_comp_counts is not None
                           and abs(resid) > args.walk_comp_counts)
                    if args.compensate and (lost or off):
                        # The same compensation as at the two places: the offset is
                        # taken from the encoder. Always after lost steps: the next
                        # move and the leg to the place are sent by the step counter,
                        # so without it they would end as far from their target as
                        # the counter is off: beyond the place, if the steps were lost
                        # moving away from it. A smaller error only with
                        # --walk-comp-counts: the place is checked on arrival anyway
                        if abs(offset[0] - err) > args.max_offset:
                            raise SystemExit(
                                f"{what}: encoder {resid:+d} counts off, "
                                f"that needs a total offset of "
                                f"{offset[0] - err} steps (limit {args.max_offset}, "
                                f"--max-offset). Not compensating, the run ends here")
                        if lost:
                            ev = dict.fromkeys(event_cols, "")
                            ev["xMoveStart"] = t_move.isoformat(timespec="milliseconds")
                            ev["xMoveSec"] = sec
                            report_lost(ev, visit, f"walk {name}", t, t_move, trace,
                                        x_start, raw_start, raw, k, commanded - x_start, err,
                                        "walk")
                            wk["ev"], wk["lost"] = ev, name
                        offset[0] -= err
                        wk["comp"] -= err
                        prev[0] += err
                        print(f"    compensating walk {name}: encoder {resid:+d} counts "
                              f"off -> {-err:+d} steps (offset now {offset[0]:+d})")
                        if lost:
                            print(f"    !   no more moves, X waits here for visit {visit}")
                            break
                return wk

            print(f"\n{'time':>14} {'visit':>5} {'place':>17} {'move':>8} {'target':>10} "
                  f"{'commanded':>10} {'measured':>10} {'posErr':>7} {'rawCounts':>10} {'encDrift':>8} {'comp':>6} {'resid':>6} {'y':>8} {'z':>8}")
            prev = [current]
            cur_mode = [status["axismode"]]
            # the sample must be reached from the calibration slide side the first time too (its
            # encoder reference is taken there): if the axis starts close to it,
            # back off towards the calibration slide first
            if (current - args.sample_pos) * side["sample"] < args.preload:
                if cur_mode[0] != 0:
                    axis.set_mode(0)
                    cur_mode[0] = 0
                back = args.sample_pos + side["sample"] * args.preload
                x_to(back, 0, back - current)
                prev[0] = current = back
            # first to the sample, so the calibration slide is approached from the sample the
            # very first time too. This leg is open loop and is the scale check:
            # the encoder must see (about) as many steps as were commanded
            first = args.sample_pos - current
            err0 = axis.sdo(OD_POS_ERR)
            # (+ offset: after a repeated leg the counter is off by what was lost)
            drift = go(-1, "sample", mode=0, photo=False) - err0 + offset[0]
            print(f"scale check: after {first} open-loop steps posErr changed by {drift} steps "
                  f"({100 * drift / first if first else 0:+.3f} %)")
            if args.mode and abs(drift) > args.max_drift:
                # in modes 1..3 the firmware stops the axis and latches LOST_STEPS as
                # soon as posErr passes its lag limit, and CORRECT would move the axis
                # by this much away from the place that was taught in
                raise SystemExit(
                    f"scale check failed: posErr changed by {drift} steps (limit {args.max_drift}, "
                    f"--max-drift). The calibrated counts per step do not fit this "
                    f"{abs(first)}-step travel well enough for a closed-loop mode; "
                    f"the axis is left at the sample position. Use --mode 0, or see --max-drift")

            t0 = time.time()
            for visit in range(visits):
                place = "calibration slide" if visit % 2 == 0 else "sample"
                # fixed schedule from t0, so the move and photo times do not add up
                due = t0 + visit * args.interval
                # until then: the random moves, then X waits at the place of the last photo
                wk = walk(visit, place, due) if visit else None
                while time.time() < due:
                    time.sleep(min(1.0, max(0.0, due - time.time())))
                go(visit, place, wk=wk)
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

    print(f"raw data saved to {out}, the random moves to {walk_path}, photos in {photos}")
    if args.mode:
        print(f"note: the axis is left in axismode {args.mode} ({MODES[args.mode]})")


if __name__ == "__main__":
    main()
