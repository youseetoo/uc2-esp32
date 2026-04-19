#!/usr/bin/env python3
"""
UC2 CANopen Motor Control Example
==================================
Drives a stepper motor on a UC2 CANopen slave via a Waveshare USB-CAN-A adapter.

Hardware:
    - Waveshare USB-CAN-A (proprietary serial protocol, NOT slcan)
    - UC2 ESP32 slave running the CANopen firmware (rewrite-canopen branch)

The UC2 firmware uses raw PDO frames (not standard CANopenNode SDO/PDO mapping)
for real-time motor control. This script sends RPDO1 + RPDO2 frames to trigger
a motor move, and listens for TPDO1 frames to confirm completion.

Requirements:
    pip install pyserial python-can

PDO layout (from ObjectDictionary.h):
    RPDO1 (0x200+nodeId):  int32 target_pos, int32 speed
    RPDO2 (0x300+nodeId):  uint8 axis, uint8 cmd, uint8 laser_id,
                           uint16 laser_pwm, uint8 led_mode, uint8 r, uint8 g
    RPDO3 (0x400+nodeId):  uint8 b, uint16 led_index, int16 qid, pad[3]
    TPDO1 (0x180+nodeId):  int32 actual_pos, uint8 axis, uint8 is_running, int16 qid
"""

import struct
import time
import threading
from typing import Optional

import can

# -- Import the Waveshare USB-CAN-A bus class from the local module --
# If waveshare_bus.py is in the same directory, just import it.
# Otherwise copy the WaveshareBus class here or adjust the path.
try:
    from waveshare_bus import WaveshareBus
except ImportError:
    # Fallback: assume slcan if WaveshareBus is not available
    WaveshareBus = None

# ============================================================================
# Constants matching ObjectDictionary.h
# ============================================================================

# COB-ID bases
RPDO1_BASE = 0x200
RPDO2_BASE = 0x300
RPDO3_BASE = 0x400
TPDO1_BASE = 0x180
TPDO2_BASE = 0x280
TPDO3_BASE = 0x380
NMT_COBID  = 0x000
HB_BASE    = 0x700

# Motor command bytes
MOTOR_CMD_STOP     = 0
MOTOR_CMD_MOVE_REL = 1
MOTOR_CMD_MOVE_ABS = 2
MOTOR_CMD_HOME     = 3

# NMT commands
NMT_CMD_START           = 0x01
NMT_CMD_STOP            = 0x02
NMT_CMD_PRE_OPERATIONAL = 0x80
NMT_CMD_RESET_NODE      = 0x81

# NMT states (heartbeat data byte)
NMT_STATE_INITIALIZING    = 0x00
NMT_STATE_PRE_OPERATIONAL = 0x7F
NMT_STATE_OPERATIONAL     = 0x05
NMT_STATE_STOPPED         = 0x04

# RPDO2 axis special values
AXIS_LASER = 0xFF
AXIS_LED   = 0xFE

# Capability bitmask
CAP_MOTOR   = 0x01
CAP_LASER   = 0x02
CAP_LED     = 0x04
CAP_ENCODER = 0x08


# ============================================================================
# UC2 CANopen Controller
# ============================================================================

class UC2CANopen:
    """
    High-level controller for UC2 CANopen slaves via raw PDO frames.

    Sends RPDO1/2/3 for motor, laser, and LED commands.
    Listens for TPDO1/2 for status feedback.
    """

    def __init__(self, bus: can.BusABC):
        self.bus = bus
        self._listener_thread: Optional[threading.Thread] = None
        self._running = False
        self._motor_status: dict[tuple[int, int], dict] = {}  # (nodeId, axis) -> status
        self._node_status: dict[int, dict] = {}  # nodeId -> status
        self._lock = threading.Lock()

    # -- Listener for TPDO feedback --

    def start_listener(self):
        """Start background thread to receive TPDO frames."""
        self._running = True
        self._listener_thread = threading.Thread(target=self._rx_loop, daemon=True)
        self._listener_thread.start()

    def stop_listener(self):
        """Stop background listener."""
        self._running = False
        if self._listener_thread:
            self._listener_thread.join(timeout=2.0)

    def _rx_loop(self):
        while self._running:
            msg = self.bus.recv(timeout=0.5)
            if msg is None:
                continue
            cob_id = msg.arbitration_id
            fc = cob_id & 0x780
            node_id = cob_id & 0x07F

            if fc == TPDO1_BASE and len(msg.data) >= 8:
                # Motor status: int32 actual_pos, uint8 axis, uint8 is_running, int16 qid
                actual_pos, axis, is_running, qid = struct.unpack_from("<iBBh", msg.data)
                with self._lock:
                    self._motor_status[(node_id, axis)] = {
                        "actual_pos": actual_pos,
                        "is_running": bool(is_running),
                        "qid": qid,
                    }

            elif fc == TPDO2_BASE and len(msg.data) >= 8:
                # Node status: uint8 caps, uint8 node_id, uint8 error_reg,
                #              int16 temperature, int16 encoder_pos, uint8 nmt_state
                caps, nid, err, temp, enc_pos, nmt = struct.unpack_from("<BBBhhB", msg.data)
                with self._lock:
                    self._node_status[node_id] = {
                        "capabilities": caps,
                        "error_reg": err,
                        "temperature_c": temp / 10.0,
                        "encoder_pos": enc_pos,
                        "nmt_state": nmt,
                    }

            elif fc == HB_BASE and len(msg.data) >= 1:
                nmt_state = msg.data[0]
                with self._lock:
                    if node_id not in self._node_status:
                        self._node_status[node_id] = {}
                    self._node_status[node_id]["nmt_state"] = nmt_state

    # -- NMT commands --

    def send_nmt(self, node_id: int, command: int):
        """Send NMT command. node_id=0 broadcasts to all nodes."""
        msg = can.Message(
            arbitration_id=NMT_COBID,
            data=bytes([command, node_id]),
            is_extended_id=False,
        )
        self.bus.send(msg)

    def start_node(self, node_id: int = 0):
        """Send NMT Start (transition to OPERATIONAL)."""
        self.send_nmt(node_id, NMT_CMD_START)

    def stop_node(self, node_id: int = 0):
        """Send NMT Stop."""
        self.send_nmt(node_id, NMT_CMD_STOP)

    def reset_node(self, node_id: int = 0):
        """Send NMT Reset Node."""
        self.send_nmt(node_id, NMT_CMD_RESET_NODE)

    # -- Motor commands (RPDO1 + RPDO2 + RPDO3) --

    def motor_move(self, node_id: int, axis: int, target_pos: int,
                   speed: int, relative: bool = True, qid: int = -1):
        """
        Command a motor move on a slave node.

        Args:
            node_id:    CAN node ID of the slave (e.g. 0x10)
            axis:       Motor axis index (0-9)
            target_pos: Target position in steps
            speed:      Maximum speed in steps/s (0 = use slave default)
            relative:   True for relative move, False for absolute
            qid:        Query ID for acknowledgement (-1 = no ack)
        """
        cmd = MOTOR_CMD_MOVE_REL if relative else MOTOR_CMD_MOVE_ABS

        # RPDO1: target_pos (int32) + speed (int32)
        rpdo1_data = struct.pack("<ii", target_pos, speed)
        rpdo1 = can.Message(
            arbitration_id=RPDO1_BASE + node_id,
            data=rpdo1_data,
            is_extended_id=False,
        )

        # RPDO2: axis, cmd, laser_id=0, laser_pwm=0, led_mode=0, r=0, g=0
        rpdo2_data = struct.pack("<BBBHBBB", axis, cmd, 0, 0, 0, 0, 0)
        rpdo2 = can.Message(
            arbitration_id=RPDO2_BASE + node_id,
            data=rpdo2_data,
            is_extended_id=False,
        )

        # RPDO3: b=0, led_index=0, qid, pad[3]
        rpdo3_data = struct.pack("<BHhBBB", 0, 0, qid, 0, 0, 0)
        rpdo3 = can.Message(
            arbitration_id=RPDO3_BASE + node_id,
            data=rpdo3_data,
            is_extended_id=False,
        )

        self.bus.send(rpdo1)
        self.bus.send(rpdo2)
        self.bus.send(rpdo3)

    def motor_stop(self, node_id: int, axis: int, qid: int = -1):
        """Send a motor stop command."""
        rpdo1_data = struct.pack("<ii", 0, 0)
        rpdo1 = can.Message(
            arbitration_id=RPDO1_BASE + node_id,
            data=rpdo1_data,
            is_extended_id=False,
        )

        rpdo2_data = struct.pack("<BBBHBBB", axis, MOTOR_CMD_STOP, 0, 0, 0, 0, 0)
        rpdo2 = can.Message(
            arbitration_id=RPDO2_BASE + node_id,
            data=rpdo2_data,
            is_extended_id=False,
        )

        rpdo3_data = struct.pack("<BHhBBB", 0, 0, qid, 0, 0, 0)
        rpdo3 = can.Message(
            arbitration_id=RPDO3_BASE + node_id,
            data=rpdo3_data,
            is_extended_id=False,
        )

        self.bus.send(rpdo1)
        self.bus.send(rpdo2)
        self.bus.send(rpdo3)

    def motor_home(self, node_id: int, axis: int, qid: int = -1):
        """Send a homing command."""
        rpdo1_data = struct.pack("<ii", 0, 0)
        rpdo1 = can.Message(
            arbitration_id=RPDO1_BASE + node_id,
            data=rpdo1_data,
            is_extended_id=False,
        )

        rpdo2_data = struct.pack("<BBBHBBB", axis, MOTOR_CMD_HOME, 0, 0, 0, 0, 0)
        rpdo2 = can.Message(
            arbitration_id=RPDO2_BASE + node_id,
            data=rpdo2_data,
            is_extended_id=False,
        )

        rpdo3_data = struct.pack("<BHhBBB", 0, 0, qid, 0, 0, 0)
        rpdo3 = can.Message(
            arbitration_id=RPDO3_BASE + node_id,
            data=rpdo3_data,
            is_extended_id=False,
        )

        self.bus.send(rpdo1)
        self.bus.send(rpdo2)
        self.bus.send(rpdo3)

    # -- Laser commands --

    def laser_set(self, node_id: int, channel: int, pwm: int, qid: int = -1):
        """
        Set laser PWM on a slave node.

        Args:
            node_id: CAN node ID
            channel: Laser channel (0-9)
            pwm:     PWM value (0-1023, 0 = off)
            qid:     Query ID for acknowledgement
        """
        # RPDO1: unused for laser, send zeros
        rpdo1_data = struct.pack("<ii", 0, 0)
        rpdo1 = can.Message(
            arbitration_id=RPDO1_BASE + node_id,
            data=rpdo1_data,
            is_extended_id=False,
        )

        # RPDO2: axis=0xFF (laser cmd), cmd=0, laser_id, laser_pwm, led fields=0
        rpdo2_data = struct.pack("<BBBHBBB", AXIS_LASER, 0, channel, pwm, 0, 0, 0)
        rpdo2 = can.Message(
            arbitration_id=RPDO2_BASE + node_id,
            data=rpdo2_data,
            is_extended_id=False,
        )

        # RPDO3: qid
        rpdo3_data = struct.pack("<BHhBBB", 0, 0, qid, 0, 0, 0)
        rpdo3 = can.Message(
            arbitration_id=RPDO3_BASE + node_id,
            data=rpdo3_data,
            is_extended_id=False,
        )

        self.bus.send(rpdo1)
        self.bus.send(rpdo2)
        self.bus.send(rpdo3)

    # -- LED commands --

    def led_set(self, node_id: int, mode: int, r: int, g: int, b: int,
                led_index: int = 0, qid: int = -1):
        """
        Set LED color/mode on a slave node.

        Args:
            node_id:   CAN node ID
            mode:      LedMode enum (0=off, 1=fill, 2=single, ...)
            r, g, b:   Color values (0-255)
            led_index: Pixel index for single mode
            qid:       Query ID for acknowledgement
        """
        rpdo1_data = struct.pack("<ii", 0, 0)
        rpdo1 = can.Message(
            arbitration_id=RPDO1_BASE + node_id,
            data=rpdo1_data,
            is_extended_id=False,
        )

        # RPDO2: axis=0xFE (LED cmd), cmd=0, laser fields=0, led_mode, r, g
        rpdo2_data = struct.pack("<BBBHBBB", AXIS_LED, 0, 0, 0, mode, r, g)
        rpdo2 = can.Message(
            arbitration_id=RPDO2_BASE + node_id,
            data=rpdo2_data,
            is_extended_id=False,
        )

        # RPDO3: b, led_index, qid
        rpdo3_data = struct.pack("<BHhBBB", b, led_index, qid, 0, 0, 0)
        rpdo3 = can.Message(
            arbitration_id=RPDO3_BASE + node_id,
            data=rpdo3_data,
            is_extended_id=False,
        )

        self.bus.send(rpdo1)
        self.bus.send(rpdo2)
        self.bus.send(rpdo3)

    # -- Status queries --

    def get_motor_status(self, node_id: int, axis: int) -> Optional[dict]:
        """Get last known motor status from TPDO1 feedback."""
        with self._lock:
            return self._motor_status.get((node_id, axis))

    def get_node_status(self, node_id: int) -> Optional[dict]:
        """Get last known node status from TPDO2 / heartbeat feedback."""
        with self._lock:
            return self._node_status.get(node_id)

    def wait_for_motor_idle(self, node_id: int, axis: int,
                            timeout: float = 30.0) -> bool:
        """
        Block until motor reports idle (is_running=False) or timeout.
        Returns True if motor is idle, False on timeout.
        """
        deadline = time.time() + timeout
        while time.time() < deadline:
            status = self.get_motor_status(node_id, axis)
            if status and not status["is_running"]:
                return True
            time.sleep(0.05)
        return False

    def scan_nodes(self, timeout: float = 3.0) -> list[int]:
        """
        Scan for online nodes by listening for heartbeat frames.
        Returns list of discovered node IDs.
        """
        found = set()
        deadline = time.time() + timeout
        while time.time() < deadline:
            msg = self.bus.recv(timeout=0.5)
            if msg is None:
                continue
            fc = msg.arbitration_id & 0x780
            node_id = msg.arbitration_id & 0x07F
            if fc == HB_BASE and node_id > 0:
                found.add(node_id)
            elif fc == TPDO2_BASE and node_id > 0:
                found.add(node_id)
        return sorted(found)

    def shutdown(self):
        """Stop listener and clean up."""
        self.stop_listener()


# ============================================================================
# Example usage
# ============================================================================

def main():
    # -- Configuration --
    SERIAL_PORT = "/dev/cu.wchusbserial10"    # Waveshare USB-CAN-A serial port
    CAN_BITRATE = 500_000           # Must match ESP32 firmware (500 kbps)
    SLAVE_NODE_ID = 0x10            # Target slave node ID (default: 16)
    MOTOR_AXIS = 0                  # Motor axis to control (0=X, 1=Y, 2=Z)
    MOVE_STEPS = 1000               # Steps to move (relative)
    MOVE_SPEED = 5000               # Speed in steps/s

    # -- Open CAN bus --
    # Option 1: Waveshare proprietary protocol (recommended for USB-CAN-A)
    if WaveshareBus is not None:
        bus = WaveshareBus(
            channel=SERIAL_PORT,
            bitrate=CAN_BITRATE,
            serial_baudrate=2_000_000,
        )
        print(f"Opened Waveshare USB-CAN-A on {SERIAL_PORT}")
    else:
        # Option 2: slcan fallback (if adapter supports it)
        bus = can.interface.Bus(
            interface="slcan",
            channel=SERIAL_PORT,
            ttyBaudrate=2_000_000,
            bitrate=CAN_BITRATE,
        )
        print(f"Opened slcan bus on {SERIAL_PORT}")

    # -- Create controller --
    uc2 = UC2CANopen(bus)
    uc2.start_listener()

    try:
        # 1) Start the slave node (NMT Start → OPERATIONAL)
        print(f"\n--- Sending NMT Start to node 0x{SLAVE_NODE_ID:02X} ---")
        uc2.start_node(SLAVE_NODE_ID)
        time.sleep(0.5)

        # 2) Check node status
        status = uc2.get_node_status(SLAVE_NODE_ID)
        if status:
            print(f"Node 0x{SLAVE_NODE_ID:02X} status: {status}")
        else:
            print(f"Node 0x{SLAVE_NODE_ID:02X}: no heartbeat received yet "
                  "(node may not be online)")

        # 3) Move motor: relative move of MOVE_STEPS at MOVE_SPEED
        print(f"\n--- Moving motor axis {MOTOR_AXIS}: "
              f"{MOVE_STEPS} steps @ {MOVE_SPEED} steps/s ---")
        uc2.motor_move(
            node_id=SLAVE_NODE_ID,
            axis=MOTOR_AXIS,
            target_pos=MOVE_STEPS,
            speed=MOVE_SPEED,
            relative=True,
            qid=1,
        )

        # 4) Wait for motor to finish
        print("Waiting for motor to complete...")
        if uc2.wait_for_motor_idle(SLAVE_NODE_ID, MOTOR_AXIS, timeout=30.0):
            motor_st = uc2.get_motor_status(SLAVE_NODE_ID, MOTOR_AXIS)
            print(f"Motor idle. Position: {motor_st['actual_pos']} steps")
        else:
            print("Timeout waiting for motor to finish!")

        # 5) Move back (negative relative move)
        print(f"\n--- Moving back: {-MOVE_STEPS} steps ---")
        uc2.motor_move(
            node_id=SLAVE_NODE_ID,
            axis=MOTOR_AXIS,
            target_pos=-MOVE_STEPS,
            speed=MOVE_SPEED,
            relative=True,
            qid=2,
        )

        if uc2.wait_for_motor_idle(SLAVE_NODE_ID, MOTOR_AXIS, timeout=30.0):
            motor_st = uc2.get_motor_status(SLAVE_NODE_ID, MOTOR_AXIS)
            print(f"Motor idle. Position: {motor_st['actual_pos']} steps")
        else:
            print("Timeout waiting for motor to finish!")

        # 6) Set LED to red
        print("\n--- Setting LED to red (fill mode) ---")
        uc2.led_set(
            node_id=SLAVE_NODE_ID,
            mode=1,   # FILL
            r=255, g=0, b=0,
            qid=3,
        )
        time.sleep(1.0)

        # 7) Turn LED off
        print("--- Turning LED off ---")
        uc2.led_set(
            node_id=SLAVE_NODE_ID,
            mode=0,   # OFF
            r=0, g=0, b=0,
            qid=4,
        )

        # 8) Set laser (channel 0, PWM 512 = ~50%)
        print("\n--- Setting laser channel 0 to 50% ---")
        uc2.laser_set(
            node_id=SLAVE_NODE_ID,
            channel=0,
            pwm=512,
            qid=5,
        )
        time.sleep(2.0)

        # 9) Turn laser off
        print("--- Turning laser off ---")
        uc2.laser_set(
            node_id=SLAVE_NODE_ID,
            channel=0,
            pwm=0,
            qid=6,
        )

        print("\n--- Done! ---")

    except KeyboardInterrupt:
        print("\nInterrupted by user.")
    finally:
        uc2.shutdown()
        bus.shutdown()
        print("Bus closed.")


if __name__ == "__main__":
    main()
