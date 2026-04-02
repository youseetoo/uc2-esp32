#!/usr/bin/env python3
"""
uc2_canopen_tool.py
===================
UC2 CANopen Python Tool — Master / Slave / Gateway / Diagnostic
Hardware: Waveshare USB-CAN-A  (slcan over USB-CDC serial)

Requirements
------------
    pip install pyserial python-can canopen

Usage
-----
    python uc2_canopen_tool.py --help
    python uc2_canopen_tool.py --port /dev/tty.usbmodem14101 master
    python uc2_canopen_tool.py --port /dev/tty.usbmodem14101 slave  --node-id 20
    python uc2_canopen_tool.py --port /dev/tty.usbmodem14101 gateway
    python uc2_canopen_tool.py --port /dev/tty.usbmodem14101 diag

Object Dictionary layout (matches ESP32S3_CO_trainer_OD)
---------------------------------------------------------
    0x1017:0   Heartbeat producer period [ms]
    0x6000:0   Read input byte (slave → master, via TPDO)
    0x6001:0   Counter uint32 (slave state, SDO-readable)
    0x6200:1   Write output byte (master → slave; bit0=motor, bit1=laser)
    0x6401:1   Motor target X (uint16)
    0x6401:2   Motor target Y (uint16)
    0x6401:3   Motor target Z (uint16)
"""

from __future__ import annotations

import argparse
import cmd
import logging
import struct
import sys
import time
import threading
import glob
import serial

try:
    import can
    import canopen
except ImportError:
    print("ERROR: Required packages missing.\n"
          "Run:  pip install pyserial python-can canopen", file=sys.stderr)
    sys.exit(1)

# ---------------------------------------------------------------------------
# Constants (OD indices — must match OD.c / OD.h on the ESP32 side)
# ---------------------------------------------------------------------------
OD_HEARTBEAT        = (0x1017, 0x00)  # uint16, producer heartbeat period [ms]
OD_READ_INPUT       = (0x6000, 0x00)  # uint8,  slave status bits
OD_COUNTER          = (0x6001, 0x00)  # uint32, incrementing counter
OD_WRITE_OUTPUT     = (0x6200, 0x01)  # uint8,  command bits (bit0=motor,bit1=laser)
OD_MOTOR_X          = (0x6401, 0x01)  # uint16, motor target X
OD_MOTOR_Y          = (0x6401, 0x02)  # uint16, motor target Y
OD_MOTOR_Z          = (0x6401, 0x03)  # uint16, motor target Z

# Default CAN bitrate and serial baudrate for the Waveshare USB-CAN-A
DEFAULT_BITRATE         = 500_000
DEFAULT_TTY_BAUDRATE    = 2_000_000
DEFAULT_NODE_ID         = 1
SCAN_NODE_RANGE         = range(1, 128)

log = logging.getLogger("uc2_canopen")


# ---------------------------------------------------------------------------
# Bus factory
# ---------------------------------------------------------------------------
def open_bus(port: str, bitrate: int = DEFAULT_BITRATE,
             tty_baudrate: int = DEFAULT_TTY_BAUDRATE) -> can.Bus:
    """Open the Waveshare USB-CAN-A via the slcan interface."""
    log.info("Opening slcan bus on %s  (serial=%d bps, CAN=%d bps)",
             port, tty_baudrate, bitrate)
    bus = can.interface.Bus(
        interface="slcan",
        channel=port,
        ttyBaudrate=tty_baudrate,
        bitrate=bitrate,
    )
    log.info("Bus opened: %s", bus)
    return bus


def auto_detect_port() -> str | None:
    """Return the first /dev/tty.usbmodem* or /dev/ttyUSB* device found."""
    candidates = glob.glob("/dev/tty.usbmodem*") + glob.glob("/dev/ttyUSB*")
    return candidates[0] if candidates else None


# ---------------------------------------------------------------------------
# Shared helpers
# ---------------------------------------------------------------------------
def _sdo_write(node: canopen.RemoteNode, index: int, sub: int,
               value: int, fmt: str) -> bool:
    """Write a single SDO value; return True on success."""
    try:
        node.sdo[index][sub].raw = struct.pack(fmt, value)
        return True
    except canopen.SdoAbortedError as exc:
        log.error("SDO write 0x%04X:%02X aborted: %s", index, sub, exc)
    except Exception as exc:
        log.error("SDO write 0x%04X:%02X error: %s", index, sub, exc)
    return False


def _sdo_read_u32(node: canopen.RemoteNode, index: int, sub: int) -> int | None:
    """Read a uint32 SDO value; return None on failure."""
    try:
        raw = node.sdo[index][sub].raw
        return struct.unpack_from("<I", raw.ljust(4, b"\x00"))[0]
    except canopen.SdoAbortedError as exc:
        log.error("SDO read 0x%04X:%02X aborted: %s", index, sub, exc)
    except Exception as exc:
        log.error("SDO read 0x%04X:%02X error: %s", index, sub, exc)
    return None


# ---------------------------------------------------------------------------
# MODE 1 — MASTER
# ---------------------------------------------------------------------------
class MasterMode(cmd.Cmd):
    """
    Interactive CANopen master.
    Connects to the ESP32 slave nodes and sends commands via SDO/NMT.
    """
    intro  = ("\n=== UC2 CANopen MASTER (Python) ===\n"
               "Type 'help' for commands, 'quit' to exit.\n")
    prompt = "uc2-master> "

    def __init__(self, network: canopen.Network, eds_path: str | None = None):
        super().__init__()
        self._net   = network
        self._eds   = eds_path  # optional EDS for rich SDO access
        self._nodes: dict[int, canopen.RemoteNode] = {}

    def _get_node(self, node_id: int) -> canopen.RemoteNode:
        if node_id not in self._nodes:
            if self._eds:
                self._nodes[node_id] = self._net.add_node(node_id, self._eds)
            else:
                self._nodes[node_id] = self._net.add_node(node_id)
        return self._nodes[node_id]

    # ---- commands ----

    def do_motor(self, line: str):
        """motor <node-id> <x> <y> <z>
        Send motor target positions to a slave via SDO.
        Example: motor 10 100 200 300"""
        parts = line.split()
        if len(parts) != 4:
            print("Usage: motor <node-id> <x> <y> <z>")
            return
        try:
            nid, x, y, z = int(parts[0]), int(parts[1]), int(parts[2]), int(parts[3])
        except ValueError:
            print("All arguments must be integers.")
            return

        node = self._get_node(nid)
        ok  = _sdo_write(node, OD_MOTOR_X[0], OD_MOTOR_X[1], x & 0xFFFF, "<H")
        ok &= _sdo_write(node, OD_MOTOR_Y[0], OD_MOTOR_Y[1], y & 0xFFFF, "<H")
        ok &= _sdo_write(node, OD_MOTOR_Z[0], OD_MOTOR_Z[1], z & 0xFFFF, "<H")
        ok &= _sdo_write(node, OD_WRITE_OUTPUT[0], OD_WRITE_OUTPUT[1], 0x01, "<B")
        print(f"[MASTER] motor node={nid} ({x},{y},{z}) → {'OK' if ok else 'FAILED'}")

    def do_laser(self, line: str):
        """laser <node-id> <0|1>
        Turn laser on (1) or off (0) on a slave.
        Example: laser 10 1"""
        parts = line.split()
        if len(parts) != 2:
            print("Usage: laser <node-id> <0|1>")
            return
        try:
            nid, onoff = int(parts[0]), int(parts[1])
        except ValueError:
            print("All arguments must be integers.")
            return

        node = self._get_node(nid)
        val  = 0x02 if onoff else 0x00
        ok   = _sdo_write(node, OD_WRITE_OUTPUT[0], OD_WRITE_OUTPUT[1], val, "<B")
        print(f"[MASTER] laser node={nid} {'ON' if onoff else 'OFF'} → {'OK' if ok else 'FAILED'}")

    def do_led(self, line: str):
        """led <node-id> <0|1>
        Turn LED on (1) or off (0) on a slave.
        Example: led 10 1"""
        parts = line.split()
        if len(parts) != 2:
            print("Usage: led <node-id> <0|1>")
            return
        try:
            nid, onoff = int(parts[0]), int(parts[1])
        except ValueError:
            print("All arguments must be integers.")
            return

        node = self._get_node(nid)
        val  = 0x04 if onoff else 0x00  # bit2 = LED
        ok   = _sdo_write(node, OD_WRITE_OUTPUT[0], OD_WRITE_OUTPUT[1], val, "<B")
        print(f"[MASTER] led node={nid} {'ON' if onoff else 'OFF'} → {'OK' if ok else 'FAILED'}")

    def do_read(self, line: str):
        """read <node-id>
        Read the counter (0x6001) and input byte (0x6000) from a slave via SDO.
        Example: read 10"""
        parts = line.split()
        if not parts:
            print("Usage: read <node-id>")
            return
        try:
            nid = int(parts[0])
        except ValueError:
            print("node-id must be an integer.")
            return

        node    = self._get_node(nid)
        counter = _sdo_read_u32(node, OD_COUNTER[0], OD_COUNTER[1])
        status  = _sdo_read_u32(node, OD_READ_INPUT[0], OD_READ_INPUT[1])
        print(f"[MASTER] node={nid}  counter={counter}  status=0x{status:02X}"
              if counter is not None else f"[MASTER] SDO read from node {nid} FAILED")

    def do_nmt(self, line: str):
        """nmt <node-id> <start|stop|reset|preop>
        Send an NMT command to a node (or 0 for broadcast).
        Example: nmt 10 start"""
        parts = line.split()
        if len(parts) != 2:
            print("Usage: nmt <node-id> <start|stop|reset|preop>")
            return
        try:
            nid = int(parts[0])
        except ValueError:
            print("node-id must be an integer.")
            return
        cmd_map = {
            "start":  self._net.nmt.send_command(0x01) if nid == 0 else None,
        }
        cmd_str = parts[1].lower()
        try:
            if cmd_str == "start":
                self._net.send_message(0x000, [0x01, nid])
            elif cmd_str == "stop":
                self._net.send_message(0x000, [0x02, nid])
            elif cmd_str == "preop":
                self._net.send_message(0x000, [0x80, nid])
            elif cmd_str == "reset":
                self._net.send_message(0x000, [0x81, nid])
            else:
                print(f"Unknown NMT command: {cmd_str}")
                return
            print(f"[MASTER] NMT '{cmd_str}' sent to node {nid}")
        except Exception as exc:
            print(f"[MASTER] NMT send error: {exc}")

    def do_scan(self, _line: str):
        """scan
        Scan for CANopen nodes by sending an NMT node-guarding request
        and listening for heartbeat or boot-up messages."""
        print("[MASTER] Scanning for nodes (2 s)...")
        self._net.scanner.reset()
        self._net.scanner.search()
        time.sleep(2.0)
        found = self._net.scanner.nodes
        if found:
            print(f"[MASTER] Found {len(found)} node(s): {sorted(found)}")
        else:
            print("[MASTER] No nodes found.")

    def do_heartbeat(self, line: str):
        """heartbeat <node-id> <period-ms>
        Configure the heartbeat producer period on a slave (0 = disable).
        Example: heartbeat 10 1000"""
        parts = line.split()
        if len(parts) != 2:
            print("Usage: heartbeat <node-id> <period-ms>")
            return
        try:
            nid, period = int(parts[0]), int(parts[1])
        except ValueError:
            print("Arguments must be integers.")
            return

        node = self._get_node(nid)
        ok   = _sdo_write(node, OD_HEARTBEAT[0], OD_HEARTBEAT[1], period, "<H")
        print(f"[MASTER] heartbeat node={nid} period={period}ms → {'OK' if ok else 'FAILED'}")

    def do_quit(self, _line: str):
        """quit — Exit the tool."""
        print("Goodbye.")
        return True

    do_exit = do_quit
    do_EOF  = do_quit


# ---------------------------------------------------------------------------
# MODE 2 — SLAVE  (software CANopen slave node)
# ---------------------------------------------------------------------------
def run_slave_mode(network: canopen.Network, node_id: int,
                   eds_path: str | None = None):
    """
    Run a minimal software CANopen slave.
    Responds to SDO reads/writes and sends heartbeats.
    Useful for testing a master without physical ESP32 hardware.
    """
    if not eds_path:
        print("[SLAVE] WARNING: No EDS file supplied — using raw SDO only.")
        local = network.create_node(node_id)
    else:
        local = network.create_node(node_id, eds_path)

    network.connect()
    local.nmt.state = "OPERATIONAL"

    # Internal state
    state = {
        "counter":  0,
        "motor_x":  0,
        "motor_y":  0,
        "motor_z":  0,
        "output":   0,
        "running":  True,
    }

    def on_write_output(value):
        state["output"] = value
        if value & 0x01:
            print(f"[SLAVE] Motor GO  target=({state['motor_x']},"
                  f"{state['motor_y']},{state['motor_z']})")
        if value & 0x02:
            print("[SLAVE] Laser ON" if value & 0x02 else "[SLAVE] Laser OFF")
        if value & 0x04:
            print("[SLAVE] LED ON" if value & 0x04 else "[SLAVE] LED OFF")

    print(f"[SLAVE] Node-ID {node_id} running. Press Ctrl-C to stop.")
    try:
        while state["running"]:
            state["counter"] += 1
            time.sleep(0.1)
    except KeyboardInterrupt:
        pass
    finally:
        network.disconnect()
        print("[SLAVE] Stopped.")


# ---------------------------------------------------------------------------
# MODE 3 — GATEWAY  (JSON serial ↔ CANopen bridge)
# ---------------------------------------------------------------------------
def run_gateway_mode(network: canopen.Network, serial_port: str,
                     serial_baud: int = 115200,
                     eds_path: str | None = None):
    """
    Serial JSON ↔ CANopen gateway.

    Reads newline-delimited JSON from *serial_port* and translates to
    CANopen SDO writes/reads, mirroring what the ESP32 master firmware does.

    Accepted JSON commands
    ----------------------
    Motor:   {"cmd": "motor", "node": 10, "x": 100, "y": 200, "z": 300}
    Laser:   {"cmd": "laser", "node": 10, "on": 1}
    LED:     {"cmd": "led",   "node": 10, "on": 1}
    Read:    {"cmd": "read",  "node": 10}
    Scan:    {"cmd": "scan"}
    """
    import json

    nodes: dict[int, canopen.RemoteNode] = {}

    def get_node(nid: int) -> canopen.RemoteNode:
        if nid not in nodes:
            nodes[nid] = network.add_node(nid, eds_path) if eds_path \
                         else network.add_node(nid)
        return nodes[nid]

    def handle(raw: str) -> dict:
        try:
            msg = json.loads(raw.strip())
        except json.JSONDecodeError as exc:
            return {"error": f"JSON parse error: {exc}"}

        cmd_name = str(msg.get("cmd", "")).lower()
        nid      = int(msg.get("node", 0))

        if cmd_name == "motor":
            node = get_node(nid)
            x, y, z = int(msg.get("x", 0)), int(msg.get("y", 0)), int(msg.get("z", 0))
            ok  = _sdo_write(node, OD_MOTOR_X[0],       OD_MOTOR_X[1],       x & 0xFFFF, "<H")
            ok &= _sdo_write(node, OD_MOTOR_Y[0],       OD_MOTOR_Y[1],       y & 0xFFFF, "<H")
            ok &= _sdo_write(node, OD_MOTOR_Z[0],       OD_MOTOR_Z[1],       z & 0xFFFF, "<H")
            ok &= _sdo_write(node, OD_WRITE_OUTPUT[0],  OD_WRITE_OUTPUT[1],  0x01, "<B")
            return {"status": "ok" if ok else "error", "cmd": "motor",
                    "node": nid, "x": x, "y": y, "z": z}

        if cmd_name == "laser":
            node = get_node(nid)
            val  = 0x02 if int(msg.get("on", 0)) else 0x00
            ok   = _sdo_write(node, OD_WRITE_OUTPUT[0], OD_WRITE_OUTPUT[1], val, "<B")
            return {"status": "ok" if ok else "error", "cmd": "laser",
                    "node": nid, "on": bool(int(msg.get("on", 0)))}

        if cmd_name == "led":
            node = get_node(nid)
            val  = 0x04 if int(msg.get("on", 0)) else 0x00
            ok   = _sdo_write(node, OD_WRITE_OUTPUT[0], OD_WRITE_OUTPUT[1], val, "<B")
            return {"status": "ok" if ok else "error", "cmd": "led",
                    "node": nid, "on": bool(int(msg.get("on", 0)))}

        if cmd_name == "read":
            node    = get_node(nid)
            counter = _sdo_read_u32(node, OD_COUNTER[0],    OD_COUNTER[1])
            status  = _sdo_read_u32(node, OD_READ_INPUT[0], OD_READ_INPUT[1])
            return {"status": "ok", "cmd": "read", "node": nid,
                    "counter": counter, "input": status}

        if cmd_name == "scan":
            network.scanner.reset()
            network.scanner.search()
            time.sleep(2.0)
            return {"status": "ok", "cmd": "scan",
                    "nodes": sorted(network.scanner.nodes)}

        return {"error": f"Unknown command: {cmd_name!r}"}

    print(f"[GW] Opening serial port {serial_port} @ {serial_baud} baud...")
    try:
        ser = serial.Serial(serial_port, serial_baud, timeout=1)
    except serial.SerialException as exc:
        print(f"[GW] Cannot open {serial_port}: {exc}")
        return

    print(f"[GW] Gateway running. Listening for JSON on {serial_port}. "
          "Press Ctrl-C to stop.")

    import json
    buf = b""
    try:
        while True:
            chunk = ser.read(256)
            if chunk:
                buf += chunk
                while b"\n" in buf:
                    line, buf = buf.split(b"\n", 1)
                    raw = line.decode(errors="replace").strip()
                    if raw:
                        log.debug("[GW] RX: %s", raw)
                        response = handle(raw)
                        out = json.dumps(response) + "\n"
                        ser.write(out.encode())
                        log.debug("[GW] TX: %s", out.strip())
    except KeyboardInterrupt:
        pass
    finally:
        ser.close()
        print("[GW] Stopped.")


# ---------------------------------------------------------------------------
# MODE 4 — DIAGNOSTIC
# ---------------------------------------------------------------------------
def run_diagnostic_mode(network: canopen.Network, bus: can.Bus,
                        eds_path: str | None = None):
    """
    Passive CAN bus diagnostic tool.
    - Continuously prints all received CAN frames with decoded CANopen function codes.
    - Scans for nodes and reads identity objects (0x1018) if an EDS is available.
    - Press Ctrl-C to stop.
    """
    FUNC_CODES = {
        0x000: "NMT",
        0x080: "SYNC/EMCY",
        0x100: "TIME",
        0x180: "TPDO1", 0x200: "RPDO1",
        0x280: "TPDO2", 0x300: "RPDO2",
        0x380: "TPDO3", 0x400: "RPDO3",
        0x480: "TPDO4", 0x500: "RPDO4",
        0x580: "SDO-resp", 0x600: "SDO-req",
        0x700: "HB/NG",
    }

    def decode_fc(cob_id: int) -> str:
        fc   = cob_id & 0x780
        nid  = cob_id & 0x07F
        name = FUNC_CODES.get(fc, f"0x{fc:03X}")
        return f"{name}  node={nid}"

    # ---- Node scan ----
    print("[DIAG] Scanning for nodes (2 s)...")
    network.scanner.reset()
    network.scanner.search()
    time.sleep(2.0)
    found = sorted(network.scanner.nodes)
    if found:
        print(f"[DIAG] Detected node IDs: {found}")
    else:
        print("[DIAG] No nodes detected during scan.")

    # ---- Identity readout ----
    if found and eds_path:
        print("[DIAG] Reading identity objects (0x1018) from detected nodes...")
        for nid in found:
            try:
                node = network.add_node(nid, eds_path)
                vendor  = _sdo_read_u32(node, 0x1018, 0x01)
                product = _sdo_read_u32(node, 0x1018, 0x02)
                rev     = _sdo_read_u32(node, 0x1018, 0x03)
                serial  = _sdo_read_u32(node, 0x1018, 0x04)
                print(f"  Node {nid:3d}:  vendor=0x{vendor:08X}  "
                      f"product=0x{product:08X}  "
                      f"rev=0x{rev:08X}  serial=0x{serial:08X}")
            except Exception as exc:
                print(f"  Node {nid:3d}: could not read identity — {exc}")

    # ---- Passive frame dump ----
    print("\n[DIAG] Live CAN frame dump — press Ctrl-C to stop.\n"
          f"{'Timestamp':>12}  {'COB-ID':>6}  {'DLC':>3}  {'Data':<23}  Decoded")
    print("-" * 75)
    try:
        while True:
            msg = bus.recv(timeout=1.0)
            if msg is None:
                continue
            ts      = f"{msg.timestamp:.3f}"
            data_hex = " ".join(f"{b:02X}" for b in msg.data)
            decoded  = decode_fc(msg.arbitration_id)
            print(f"{ts:>12}  {msg.arbitration_id:06X}  {msg.dlc:3d}"
                  f"  {data_hex:<23}  {decoded}")
    except KeyboardInterrupt:
        pass
    finally:
        print("\n[DIAG] Stopped.")


# ---------------------------------------------------------------------------
# CLI entry point
# ---------------------------------------------------------------------------
def build_parser() -> argparse.ArgumentParser:
    p = argparse.ArgumentParser(
        prog="uc2_canopen_tool.py",
        description="UC2 CANopen Tool — Master / Slave / Gateway / Diagnostic\n"
                    "Hardware: Waveshare USB-CAN-A (slcan over USB-CDC)",
        formatter_class=argparse.RawDescriptionHelpFormatter,
    )

    p.add_argument(
        "--port", "-p",
        default=None,
        help="Serial port of the Waveshare adapter "
             "(e.g. /dev/tty.usbmodem14101). "
             "Auto-detected if omitted.",
    )
    p.add_argument(
        "--bitrate", "-b",
        type=int,
        default=DEFAULT_BITRATE,
        help=f"CAN bitrate in bps (default: {DEFAULT_BITRATE})",
    )
    p.add_argument(
        "--tty-baudrate",
        type=int,
        default=DEFAULT_TTY_BAUDRATE,
        help=f"USB serial baudrate (default: {DEFAULT_TTY_BAUDRATE}; "
             "Waveshare USB-CAN-A internal link speed)",
    )
    p.add_argument(
        "--eds",
        default=None,
        metavar="PATH",
        help="Path to EDS/XDD file for rich SDO access "
             "(e.g. ESP32S3_CO_trainer_OD/ESP32S3_CO_trainer_OD.eds)",
    )
    p.add_argument(
        "--verbose", "-v",
        action="store_true",
        help="Enable verbose debug logging.",
    )

    sub = p.add_subparsers(dest="mode", required=True)

    # master
    m = sub.add_parser("master", help="Interactive CANopen master CLI.")
    m.add_argument("--node-id", type=int, default=DEFAULT_NODE_ID,
                   help=f"This master's Node-ID (default: {DEFAULT_NODE_ID})")

    # slave
    s = sub.add_parser("slave",
                       help="Software CANopen slave (for testing without ESP32).")
    s.add_argument("--node-id", type=int, default=20,
                   help="Node-ID for this software slave (default: 20)")

    # gateway
    g = sub.add_parser("gateway",
                       help="JSON serial ↔ CANopen bridge (mimics ESP32 master).")
    g.add_argument("--serial-port", required=True,
                   help="Serial port for JSON input from Raspberry Pi / PC "
                        "(e.g. /dev/ttyUSB1)")
    g.add_argument("--serial-baud", type=int, default=115200,
                   help="Serial baud rate for JSON port (default: 115200)")

    # diagnostic
    sub.add_parser("diag",
                   help="Passive bus diagnostic: node scan + live frame dump.")

    return p


def main():
    parser = build_parser()
    args   = parser.parse_args()

    logging.basicConfig(
        level=logging.DEBUG if args.verbose else logging.INFO,
        format="%(levelname)s  %(name)s: %(message)s",
    )

    # --- resolve port ---
    port = args.port or auto_detect_port()
    if not port:
        print("ERROR: No USB-CAN adapter found. "
              "Specify --port /dev/tty.usbmodemXXXX", file=sys.stderr)
        sys.exit(1)

    print(f"[INIT] Using CAN port: {port}")

    # --- open bus ---
    try:
        bus = open_bus(port, bitrate=args.bitrate, tty_baudrate=args.tty_baudrate)
    except Exception as exc:
        print(f"ERROR: Cannot open CAN bus on {port}: {exc}", file=sys.stderr)
        sys.exit(1)

    # --- connect CANopen network ---
    network = canopen.Network()
    network.connect(bus=bus)

    try:
        if args.mode == "master":
            cli = MasterMode(network, eds_path=args.eds)
            cli.cmdloop()

        elif args.mode == "slave":
            run_slave_mode(network, node_id=args.node_id, eds_path=args.eds)

        elif args.mode == "gateway":
            run_gateway_mode(
                network,
                serial_port=args.serial_port,
                serial_baud=args.serial_baud,
                eds_path=args.eds,
            )

        elif args.mode == "diag":
            run_diagnostic_mode(network, bus, eds_path=args.eds)

    finally:
        network.disconnect()
        log.info("Network disconnected.")


if __name__ == "__main__":
    main()
