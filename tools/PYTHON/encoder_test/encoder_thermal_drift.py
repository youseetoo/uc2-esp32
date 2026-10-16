#!/usr/bin/env python3
# /// script
# requires-python = ">=3.12"
# dependencies = ["pyserial>=3.5"]
# ///
"""
X-axis 12 hour thermal drift test over the CAN bus: the stage stands still
and every --interval seconds (default 10 min, 73 samples in 12 h) a photo is
taken and the X encoder and step counter are read.

Meant for the linear (magnetic) encoder: it sits on the stage and also sees
the lead screw. The raw count (0x204B) is whichever encoder the X slave's
firmware was built with (ENC_X_A / ENC_X_B in its PinConfig.h, linear =
GPIO 40 / 41).

Nothing moves: no /motor_act is sent, only SDO reads through the master. The
one write: if X is not in axismode 0 at the start it is set to 0, since a
closed-loop mode could move the axis by itself. Bring the stage to the place
to watch, in focus, before the start.

The first sample is the reference: dCounts / dSteps are the raw count / step
counter minus their first value, dUm = dCounts * --um-per-count (1.95 um for
the linear encoder; smaller drift only shows in the photos). dSteps must stay
0, otherwise something moved the stage and the row is marked. The Y and Z step
counters are recorded too if those slaves answer.

Only Ctrl+C ends the run: a failed read or photo leaves that field empty.
Photos (Hikrobot camera, MVS SDK, client closed) go to
<csv name>_photos/0000_<time>.png, about 30 MB each.

Usage:
    uv run tools/PYTHON/encoder_test/encoder_thermal_drift.py --photo-test --gain 23
    uv run tools/PYTHON/encoder_test/encoder_thermal_drift.py --port COM7 --gain 23 --quick
    uv run tools/PYTHON/encoder_test/encoder_thermal_drift.py --port COM7 --gain 23

Output: one CSV row per sample, '#' lines are run metadata
(pandas: read_csv(..., comment='#')). encoder_test.ipynb (part 6, DRIFT)
correlates the photos and plots the drift against the encoder.
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

MODES = {0: "OPEN_LOOP", 1: "MONITOR", 2: "CORRECT", 3: "SERVO"}
# stage travel per count of the linear encoder
UM_PER_COUNT = 2000 / 1024

# slave object dictionary (main/src/canopen/UC2_OD_Indices.h)
OD_POSITION, OD_STATUS = 0x2001, 0x2004
OD_MODE, OD_RAW = 0x2042, 0x204B


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

    def __init__(self, ser, node, sub):
        self.ser, self.node, self.sub = ser, node, sub

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
    ap.add_argument("--y-node", type=int, default=12, help="CAN node id of the Y slave")
    ap.add_argument("--y-sub", type=int, default=3,
                    help="SDO sub-index of the axis on the Y slave (axis id + 1, Y = 3)")
    ap.add_argument("--z-node", type=int, default=13, help="CAN node id of the Z slave")
    ap.add_argument("--z-sub", type=int, default=4,
                    help="SDO sub-index of the axis on the Z slave (axis id + 1, Z = 4)")
    ap.add_argument("--interval", type=float, default=600,
                    help="seconds between two samples (default 600 = 10 min)")
    ap.add_argument("--hours", type=float, default=12, help="length of the test")
    ap.add_argument("--um-per-count", type=float, default=UM_PER_COUNT,
                    help="stage travel per encoder count in um, for the dUm column "
                         "(default: the linear encoder)")
    ap.add_argument("--quick", action="store_true",
                    help="rehearsal: the same sequence, but only 4 samples 20 s apart")
    ap.add_argument("--gain", type=float, default=None,
                    help="camera gain in dB (default: as set in the camera)")
    ap.add_argument("--exposure", type=float, default=None,
                    help="camera exposure time in microseconds (default: as set "
                         "in the camera)")
    ap.add_argument("--photo-format", choices=("bmp", "png", "jpg", "tif"), default="png")
    ap.add_argument("--photo-test", action="store_true",
                    help="only take one photo and exit")
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
    if not args.port:
        ap.error("--port is required")
    if args.interval <= 0 or args.hours <= 0:
        ap.error("--interval and --hours must be > 0")
    if args.quick:
        args.interval, args.hours = 20.0, 3 * 20 / 3600
    samples = int(args.hours * 3600 / args.interval + 1e-6) + 1
    out = args.out or os.path.join(DATA_DIR, f"12hrstest_thermal_drift_x_{datetime.now():%Y%m%d_%H%M%S}.csv")

    camera = Camera(args.gain, args.exposure)  # first, so a camera problem shows up early
    photos = os.path.splitext(out)[0] + "_photos"
    os.makedirs(photos, exist_ok=True)
    ser = open_master(args.port, args.baud)
    keep_awake(True)
    done = 0
    try:
        time.sleep(2.0)
        axis = Axis(ser, args.node, args.sub)
        pos, running = axis.position()
        mode = axis.sdo(OD_MODE, typ="u8")
        print(f"X (node {args.node}): position = {pos}, running = {int(running)}, "
              f"rawCounts = {axis.sdo(OD_RAW)}, axismode = {mode} ({MODES.get(mode, '?')})")
        if running:
            raise SystemExit("the X axis is running: start when the stage stands still")
        if mode != 0:
            # in the closed-loop modes the firmware could move the axis by itself
            print(f"axismode was {mode}: setting it to 0 (open loop), no motion")
            axis.sdo(OD_MODE, 0, "u8")
            time.sleep(0.5)
        # the other two axes have no encoder: only their step counters, if they answer
        others = {}
        for name, a in (("y", Axis(ser, args.y_node, args.y_sub)),
                        ("z", Axis(ser, args.z_node, args.z_sub))):
            try:
                print(f"{name.upper()} (node {a.node}): position, running = {a.position()}")
                others[name] = a
            except RuntimeError as e:
                print(f"{name.upper()} is not recorded: {e}")
        print(f"{samples} samples every {args.interval:g} s ({args.hours:g} h), nothing moves")

        with open(out, "w", newline="") as f:
            f.write(f"# test=12hrstest_thermal_drift_x port={args.port} node={args.node} "
                    f"started={datetime.now().isoformat(timespec='seconds')}\n")
            f.write(f"# interval={args.interval:g} hours={args.hours:g} mode=0 "
                    f"mode_at_start={mode} encoder=linear um_per_count={args.um_per_count:.5f} "
                    f"camera={camera.settings}\n")
            f.write(f"# y_node={args.y_node if 'y' in others else None} "
                    f"z_node={args.z_node if 'z' in others else None}\n")
            w = csv.writer(f)
            w.writerow(["time_iso", "sample", "elapsed_s", "commanded", "rawCounts",
                        "dSteps", "dCounts", "dUm", "isRunning", "y", "z", "photo", "note"])
            f.flush()
            print(f"\n{'time':>14} {'sample':>6} {'elapsed':>8} {'commanded':>10} "
                  f"{'rawCounts':>10} {'dSteps':>7} {'dCounts':>8} {'dUm':>8} {'y':>8} {'z':>8}")
            ref = None  # (step position, raw count) at the first sample
            drift = []
            t0 = time.time()
            for i in range(samples):
                # fixed schedule from t0, so the read and photo times do not add up
                while time.time() < t0 + i * args.interval:
                    time.sleep(min(1.0, max(0.0, t0 + i * args.interval - time.time())))
                now = datetime.now()
                elapsed = round(time.time() - t0, 1)
                pos = raw = d_steps = d_counts = d_um = running = ""
                note = []
                try:
                    raw = axis.sdo(OD_RAW)
                    pos, running = axis.position()
                    running = int(running)
                    ref = ref or (pos, raw)
                    d_steps, d_counts = pos - ref[0], raw - ref[1]
                    d_um = round(d_counts * args.um_per_count, 2)
                    drift.append(d_counts)
                    if d_steps or running:
                        note.append("X was moved" if d_steps else "X is running")
                except RuntimeError as e:  # one missing reading should not end 12 h
                    note.append(str(e))
                yz = {"y": "", "z": ""}
                for name, a in others.items():
                    try:
                        yz[name] = a.sdo(OD_POSITION)
                    except RuntimeError as e:
                        note.append(str(e))
                path = os.path.join(photos, f"{i:04d}_{now:%Y%m%d_%H%M%S}.{args.photo_format}")
                try:
                    camera.photo(path)
                except RuntimeError as e:  # one missing photo should not end 12 h
                    note.append(str(e))
                    path = f"FAILED: {e}"
                print(f"{now:%m-%d %H:%M:%S} {i:>6} {elapsed:>8} {pos:>10} {raw:>10} "
                      f"{d_steps:>7} {d_counts:>8} {d_um:>8} {yz['y']:>8} {yz['z']:>8}")
                for n in note:
                    print(f"    ! {n}")
                w.writerow([now.isoformat(timespec="milliseconds"), i, elapsed, pos, raw,
                            d_steps, d_counts, d_um, running, yz["y"], yz["z"], path,
                            "; ".join(note)])
                f.flush()
                done += 1
            print(f"\ndone, {done} samples" + (
                f", encoder drift {min(drift):+d} .. {max(drift):+d} counts = "
                f"{min(drift) * args.um_per_count:+.2f} .. {max(drift) * args.um_per_count:+.2f} um "
                f"against the first sample" if drift else ""))
    except KeyboardInterrupt:
        print(f"\nstopped by Ctrl+C after {done} samples")
    finally:
        keep_awake(False)
        ser.close()
        camera.close()

    print(f"raw data saved to {out}, photos in {photos}")


if __name__ == "__main__":
    main()
