"""
Waveshare USB-CAN-A interface for python-can.

This module implements a python-can compatible interface for the Waveshare
USB-CAN-A adapter, which uses a proprietary serial protocol instead of slcan.

Protocol:
    Frame format: AA [type/len] [ID bytes] [data bytes] 55
    - AA: Start byte
    - type/len: bit5=extended, bit4=remote, bit0-3=data length
    - ID: 2 bytes (standard) or 4 bytes (extended), little-endian
    - data: 0-8 bytes
    - 55: End byte

Serial settings: 2000000 baud, 8N1

References:
    - https://www.waveshare.com/wiki/USB-CAN-A
    - https://github.com/RajithaRanasinghe/Python-Class-for-Waveshare-USB-CAN-A
"""
import glob
import struct
import threading
import time
from typing import Optional

import serial
from can import BusABC, Message


# Protocol constants
FRAME_START = 0xAA
FRAME_END = 0x55
TYPE_STANDARD = 0xC0  # bit5=0, bit4=0
TYPE_EXTENDED = 0xE0  # bit5=1, bit4=0
TYPE_REMOTE = 0x10    # bit4=1

# CAN speeds (index used in settings command)
CAN_SPEEDS = {
    1000000: 0x01,
    800000: 0x02,
    500000: 0x03,
    400000: 0x04,
    250000: 0x05,
    200000: 0x06,
    125000: 0x07,
    100000: 0x08,
    50000: 0x09,
    20000: 0x0A,
    10000: 0x0B,
    5000: 0x0C,
}


class WaveshareBus(BusABC):
    """
    python-can interface for Waveshare USB-CAN-A adapter.

    Usage:
        bus = WaveshareBus(channel='/dev/ttyUSB0', bitrate=1000000)
        bus.send(Message(arbitration_id=0x123, data=[0x11, 0x22]))
        msg = bus.recv(timeout=1.0)
        bus.shutdown()
    """

    def __init__(
        self,
        channel: str,
        bitrate: int = 1000000,
        serial_baudrate: int = 2000000,
        **kwargs
    ):
        """
        Initialize the Waveshare USB-CAN-A adapter.

        Args:
            channel: Serial port (e.g., '/dev/ttyUSB0', '/dev/ttyUSB1')
            bitrate: CAN bus bitrate (default 1000000 for Piper arm)
            serial_baudrate: Serial port baudrate (default 2000000)
        """
        super().__init__(channel=channel, **kwargs)

        self.channel = channel
        self.bitrate = bitrate
        self.serial_baudrate = serial_baudrate

        # Open serial port
        self._ser = serial.Serial(
            port=channel,
            baudrate=serial_baudrate,
            bytesize=serial.EIGHTBITS,
            parity=serial.PARITY_NONE,
            stopbits=serial.STOPBITS_ONE,
            timeout=0.1,
        )

        # Buffer for incoming data
        self._recv_buffer = bytearray()
        self._recv_lock = threading.Lock()

        # Configure CAN speed
        self._configure_speed(bitrate)

    def _configure_speed(self, bitrate: int):
        """Send settings command to configure CAN speed."""
        if bitrate not in CAN_SPEEDS:
            raise ValueError(f"Unsupported bitrate: {bitrate}. Supported: {list(CAN_SPEEDS.keys())}")

        speed_code = CAN_SPEEDS[bitrate]
        # Settings frame: AA 55 12 [speed] [mode] [filter_id(4)] [mask_id(4)] [reserved(5)] 55
        cmd = bytearray([
            0xAA, 0x55, 0x12,
            speed_code,  # Speed
            0x00,        # Mode: normal
            0x00, 0x00, 0x00, 0x00,  # Filter ID (disabled)
            0x00, 0x00, 0x00, 0x00,  # Mask ID (disabled)
            0x00,        # Reserved
            0x00,        # Reserved
            0x00,        # Reserved
            0x00,        # Reserved
            0x00,        # Reserved
            0x55,        # End
        ])
        self._ser.write(cmd)
        self._ser.flush()
        time.sleep(0.1)  # Wait for adapter to apply settings

    def _encode_frame(self, msg: Message) -> bytes:
        """Encode a CAN message to Waveshare serial format."""
        type_byte = TYPE_STANDARD if not msg.is_extended_id else TYPE_EXTENDED
        if msg.is_remote_frame:
            type_byte |= TYPE_REMOTE
        type_byte |= len(msg.data) & 0x0F

        frame = bytearray([FRAME_START, type_byte])

        # Add ID (little-endian)
        if msg.is_extended_id:
            frame.extend(struct.pack('<I', msg.arbitration_id))
        else:
            frame.extend(struct.pack('<H', msg.arbitration_id))

        # Add data
        frame.extend(msg.data)

        # End byte
        frame.append(FRAME_END)

        return bytes(frame)

    def _decode_frame(self, data: bytes) -> Optional[Message]:
        """Decode a Waveshare serial frame to CAN message."""
        if len(data) < 4:  # Minimum: AA type ID(2) 55
            return None

        if data[0] != FRAME_START or data[-1] != FRAME_END:
            return None

        type_byte = data[1]
        is_extended = bool(type_byte & 0x20)
        is_remote = bool(type_byte & 0x10)
        dlc = type_byte & 0x0F

        # Extract ID
        if is_extended:
            if len(data) < 7:  # AA type ID(4) 55
                return None
            arb_id = struct.unpack('<I', data[2:6])[0]
            data_start = 6
        else:
            arb_id = struct.unpack('<H', data[2:4])[0]
            data_start = 4

        # Extract data
        data_bytes = data[data_start:-1][:dlc]

        return Message(
            arbitration_id=arb_id,
            data=bytes(data_bytes),
            is_extended_id=is_extended,
            is_remote_frame=is_remote,
            timestamp=time.time(),
        )

    def send(self, msg: Message, timeout: Optional[float] = None) -> None:
        """Send a CAN message."""
        frame = self._encode_frame(msg)
        self._ser.write(frame)
        self._ser.flush()

    def _recv_internal(self, timeout: Optional[float]) -> tuple[Optional[Message], bool]:
        """
        Read a message from the serial port.

        Returns:
            Tuple of (message or None, filtered: bool)
        """
        deadline = time.time() + (timeout if timeout else 0)

        with self._recv_lock:
            while True:
                # Try to find a complete frame in buffer
                start_idx = self._recv_buffer.find(FRAME_START)
                if start_idx >= 0:
                    # Discard anything before start
                    if start_idx > 0:
                        self._recv_buffer = self._recv_buffer[start_idx:]

                    # Look for end byte
                    end_idx = self._recv_buffer.find(FRAME_END, 1)
                    if end_idx >= 0:
                        # Extract frame
                        frame_data = bytes(self._recv_buffer[:end_idx + 1])
                        self._recv_buffer = self._recv_buffer[end_idx + 1:]

                        msg = self._decode_frame(frame_data)
                        if msg:
                            return msg, False

                # Read more data from serial
                if timeout is not None and time.time() >= deadline:
                    return None, False

                remaining = deadline - time.time() if timeout else 0.1
                if remaining <= 0 and timeout is not None:
                    return None, False

                self._ser.timeout = min(remaining, 0.1)
                chunk = self._ser.read(256)
                if chunk:
                    self._recv_buffer.extend(chunk)
                elif timeout is not None and time.time() >= deadline:
                    return None, False

    def shutdown(self) -> None:
        """Close the serial port."""
        super().shutdown()
        if self._ser and self._ser.is_open:
            self._ser.close()


def find_all_waveshare_ports() -> list[str]:
    """Find all available Waveshare USB-CAN-A adapter serial ports.

    Returns:
        List of serial port paths that can be opened at 2000000 baud.
    """
    found = []
    for pattern in ['/dev/ttyUSB*', '/dev/ttyACM*']:
        ports = sorted(glob.glob(pattern))
        for port in ports:
            try:
                ser = serial.Serial(port, 2000000, timeout=0.5)
                ser.close()
                found.append(port)
            except (serial.SerialException, OSError):
                continue
    return found


def find_waveshare_port(exclude: Optional[list[str]] = None) -> Optional[str]:
    """Find the first available Waveshare USB-CAN-A adapter serial port.

    Args:
        exclude: List of ports to skip (for dual-arm setups where one port is already in use)

    Returns:
        First available serial port path, or None if not found.
    """
    exclude_set = set(exclude) if exclude else set()
    for pattern in ['/dev/ttyUSB*', '/dev/ttyACM*']:
        ports = sorted(glob.glob(pattern))
        for port in ports:
            if port in exclude_set:
                continue
            try:
                ser = serial.Serial(port, 2000000, timeout=0.5)
                ser.close()
                return port
            except (serial.SerialException, OSError):
                continue
    return None