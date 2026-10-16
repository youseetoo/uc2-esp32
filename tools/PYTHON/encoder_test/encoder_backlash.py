#!/usr/bin/env python3
# /// script
# requires-python = ">=3.12"
# dependencies = ["pyserial>=3.5"]
# ///
"""
Backlash of the X or Y axis (AXIS): photos of the calibration slide and encoder
counts, taken after an approach from below and after an approach from above.

Before the start: calibration slide in the picture and in focus. Where the axis
is then is position 0 (pos0). The other axes are not moved.

ROUNDS times:
  1. slowly pos0 -> pos1 (+STEP)                    -> photo "below"
  2. fast pos1 -> pos1 + APPROACH -> pos1           -> photo "above"
  3. slowly pos1 -> pos0 (-STEP)

Every photo is one CSV row: commanded position, encoder count (empty for Y,
which has no encoder), photo file.
The image processing and the backlash are done in backlash.ipynb.

MODE is written to the axis before the test:
  0 open loop, 1 monitor, 2 correct, 3 servo

The PC is on the USB port of the CAN master, the axis sits on a CAN slave (AXES).
The MVS client must not have the camera open.

Usage: edit the settings below, then
    uv run tools/PYTHON/encoder_test/encoder_backlash.py

Output: encoder_test_data/backlash_<axis>_<time>.csv (+ _photos/). Lines starting
with '#' are metadata (pandas: read_csv(..., comment='#')).
"""
import csv
import ctypes
import json
import os
import sys
import time
from datetime import datetime

import serial

# ESP32
PORT = "COM6"          # serial port of the CAN master
BAUD = 921600

# motor
AXIS = "X"             # axis under test: "X" or "Y"
MODE = 0               # axis mode: 0 open loop, 1 monitor, 2 correct, 3 servo
STEP = 256             # pos1 = pos0 + STEP
SLOW_SPEED = 1000      # speed of pos0 -> pos1 and pos1 -> pos0
APPROACH = 20000       # pos1 -> pos1 + APPROACH -> pos1 (> backlash)
SPEED = 100000         # speed of that excursion
ACCEL = 1000000         # acceleration of all moves
ROUNDS = 10

# camera
GAIN = 5.0             # camera gain in dB
EXPOSURE = 100000        # exposure in us, None = as set in the camera
PHOTO_DELAY = 2.0      # seconds between the end of a move and the photo

#       CAN node id, SDO sub-index on the slave, stepperid on the master, has an encoder
AXES = {"X": (11, 2, 1, True),
        "Y": (12, 3, 2, False)}
NODE, SUB, STEPPER, HAS_ENCODER = AXES[AXIS]

# results (CSV, photos) go to encoder_test_data/ next to this script
DATA_DIR = os.path.join(os.path.dirname(os.path.abspath(__file__)), "encoder_test_data")

# slave object dictionary (main/src/canopen/UC2_OD_Indices.h)
OD_POSITION, OD_STATUS, OD_MODE, OD_RAW = 0x2001, 0x2004, 0x2042, 0x204B


# ---- the axis over CAN (copied from 24hrstest_can_rotational.py, trimmed) ------
class Axis:
    def __init__(self):
        self.ser = serial.Serial()
        self.ser.port, self.ser.baudrate, self.ser.timeout = PORT, BAUD, 0.2
        self.ser.dtr = self.ser.rts = False  # DTR / RTS would reset the ESP32
        self.ser.open()
        time.sleep(2.0)

    def send(self, msg):
        self.ser.write((json.dumps(msg) + "\n").encode())

    def sdo(self, index, value=None, typ="i32"):
        """Read (value None) or write one object of the slave via the master."""
        req = {"node": NODE, "index": index, "sub": SUB, "type": typ, "op": "r"}
        if value is not None:
            req["op"], req["value"] = "w", value
        for _ in range(3):  # an SDO sometimes fails on the bus: try again
            self.ser.reset_input_buffer()
            self.send({"task": "/can_act", "sdo": req})
            t_end = time.time() + 3.0
            while time.time() < t_end:
                line = self.ser.readline().decode(errors="ignore")
                try:
                    resp = json.loads(line[line.index("{"):])
                except ValueError:
                    continue
                if isinstance(resp, dict) and resp.get("index") == index and "status" in resp:
                    if resp["status"] == "ok":
                        return resp.get("value")
                    break
        raise RuntimeError(f"SDO 0x{index:04X} on node {NODE} failed")

    def position(self):
        return self.sdo(OD_POSITION)

    def encoder(self):
        return self.sdo(OD_RAW)

    def set_mode(self, mode):
        self.sdo(OD_MODE, mode, "u8")

    def move_to(self, target, speed):
        """Absolute move, returns when the axis has stopped."""
        self.send({"task": "/motor_act", "motor": {"steppers": [{
            "stepperid": STEPPER, "position": target, "speed": speed,
            "acceleration": ACCEL, "isabs": 1}]}})
        time.sleep(0.5)  # give the move time to start
        while self.sdo(OD_STATUS, typ="u8") & 1:  # bit 0 = running
            time.sleep(0.2)


# ---- Hikrobot camera (copied from 24hrstest_can_rotational.py, trimmed) ------
class Camera:
    def __init__(self):
        sdk = os.getenv("MVCAM_COMMON_RUNENV")
        if not sdk:
            raise SystemExit("MVCAM_COMMON_RUNENV is not set - is the Hikrobot MVS SDK installed?")
        sys.path.append(os.path.join(sdk, "Samples", "Python", "MvImport"))
        import MvCameraControl_class as mv
        self.mv = mv
        mv.MvCamera.MV_CC_Initialize()
        devices = mv.MV_CC_DEVICE_INFO_LIST()
        self.check(mv.MvCamera.MV_CC_EnumDevices(mv.MV_GIGE_DEVICE | mv.MV_USB_DEVICE, devices),
                   "enum devices")
        if devices.nDeviceNum == 0:
            raise SystemExit("no Hikrobot camera found")
        info = ctypes.cast(devices.pDeviceInfo[0], ctypes.POINTER(mv.MV_CC_DEVICE_INFO)).contents
        cam = self.cam = mv.MvCamera()
        self.check(cam.MV_CC_CreateHandle(info), "create handle")
        self.check(cam.MV_CC_OpenDevice(mv.MV_ACCESS_Exclusive, 0),
                   "open device (is it still open in the MVS client?)")
        self.check(cam.MV_CC_SetEnumValue("TriggerMode", mv.MV_TRIGGER_MODE_OFF), "trigger off")
        cam.MV_CC_SetEnumValueByString("GainAuto", "Off")
        self.check(cam.MV_CC_SetFloatValue("Gain", GAIN), "set gain")
        if EXPOSURE is not None:
            cam.MV_CC_SetEnumValueByString("ExposureAuto", "Off")
            self.check(cam.MV_CC_SetFloatValue("ExposureTime", EXPOSURE), "set exposure")

    @staticmethod
    def check(ret, what):
        if ret != 0:
            raise RuntimeError(f"camera: {what} failed, ret = 0x{ret:x}")

    def photo(self, path, skip=2):
        """Save one frame as png; the first `skip` frames are dropped (exposure settles)."""
        mv, cam = self.mv, self.cam
        self.check(cam.MV_CC_StartGrabbing(), "start grabbing")
        try:
            for i in range(skip + 1):
                frame = mv.MV_FRAME_OUT()
                ctypes.memset(ctypes.byref(frame), 0, ctypes.sizeof(frame))
                self.check(cam.MV_CC_GetImageBuffer(frame, 5000), "get frame")
                try:
                    if i < skip:
                        continue
                    info = frame.stFrameInfo
                    image = mv.MV_CC_IMAGE()
                    ctypes.memset(ctypes.byref(image), 0, ctypes.sizeof(image))
                    image.nWidth, image.nHeight = info.nExtendWidth, info.nExtendHeight
                    image.enPixelType, image.pImageBuf = info.enPixelType, frame.pBufAddr
                    image.nImageBufSize = image.nImageLen = info.nFrameLenEx
                    param = mv.MV_CC_SAVE_IMAGE_PARAM()
                    ctypes.memset(ctypes.byref(param), 0, ctypes.sizeof(param))
                    param.enImageType, param.iMethodValue, param.nQuality = mv.MV_Image_Png, 1, 99
                    self.check(cam.MV_CC_SaveImageToFileEx2(image, param, path), f"save {path}")
                finally:
                    cam.MV_CC_FreeImageBuffer(frame)
        finally:
            cam.MV_CC_StopGrabbing()

    def close(self):
        self.cam.MV_CC_CloseDevice()
        self.cam.MV_CC_DestroyHandle()
        self.mv.MvCamera.MV_CC_Finalize()


# ---- the test ----------------------------------------------------------------
def main():
    stamp = datetime.now()
    out = os.path.join(DATA_DIR, f"backlash_{AXIS.lower()}_{stamp:%Y%m%d_%H%M%S}.csv")
    photo_dir = os.path.splitext(out)[0] + "_photos"
    os.makedirs(photo_dir, exist_ok=True)

    camera = Camera()
    axis = Axis()
    axis.set_mode(MODE)
    pos0 = axis.position()
    pos1 = pos0 + STEP
    print(f"axis {AXIS}, mode {MODE}, pos0 = {pos0}, pos1 = {pos1}, {ROUNDS} rounds")

    with open(out, "w", newline="") as f:
        f.write(f"# test=backlash axis={AXIS} mode={MODE} port={PORT} node={NODE} "
                f"started={stamp.isoformat(timespec='seconds')}\n"
                f"# pos0={pos0} pos1={pos1} step={STEP} approach={APPROACH} rounds={ROUNDS} "
                f"speed={SPEED} accel={ACCEL} slow_speed={SLOW_SPEED} photo_delay={PHOTO_DELAY}\n")
        w = csv.writer(f)
        w.writerow(["time_iso", "round", "side", "commanded", "encoder", "photo"])

        def record(n, side, commanded):
            """Wait PHOTO_DELAY, take a photo, write one CSV row."""
            time.sleep(PHOTO_DELAY)
            name = f"{n:02d}_{side}.png"
            camera.photo(os.path.join(photo_dir, name))
            raw = axis.encoder() if HAS_ENCODER else ""  # no encoder: the cell stays empty
            w.writerow([datetime.now().isoformat(timespec="milliseconds"),
                        n, side, commanded, raw, name])
            f.flush()
            print(f"{n:>3} {side:>6}  commanded {commanded:>8}  encoder {raw:>8}")

        for n in range(1, ROUNDS + 1):
            # 1. slowly +STEP to pos1
            axis.move_to(pos1, SLOW_SPEED)
            record(n, "below", pos1)

            # 2. fast +APPROACH and back to pos1
            axis.move_to(pos1 + APPROACH, SPEED)
            axis.move_to(pos1, SPEED)
            record(n, "above", pos1)

            # 3. slowly -STEP back to pos0
            axis.move_to(pos0, SLOW_SPEED)

    print(f"\nsaved {out}, photos in {photo_dir}")
    camera.close()
    axis.ser.close()


if __name__ == "__main__":
    main()
