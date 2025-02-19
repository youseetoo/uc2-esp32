import time

class CanFrame:
    def __init__(self, identifier, data):
        """
        identifier: The numeric CAN ID (e.g. 0x200) to which this frame is addressed
        data: A bytes/bytearray containing the frame payload
        """
        self.identifier = identifier
        self.data = data

class SimulatedBus:
    def __init__(self):
        self.queue = []

    def send(self, frame: CanFrame):
        print(f"[BUS] TX => ID=0x{frame.identifier:X}, data={frame.data}")
        self.queue.append(frame)

    def read(self, filter_id: int, timeout: float = 1.0) -> CanFrame:
        """
        Emulate “blocking” read for up to `timeout` seconds,
        returning the first frame that matches `filter_id` (the CAN ID we want to accept).
        """
        start = time.time()
        while time.time() - start < timeout:
            for i, f in enumerate(self.queue):
                if f.identifier == filter_id:
                    self.queue.pop(i)
                    return f
            time.sleep(0.01)
        return None

class IsoTpMaster:
    """
    The 'master' node has its own ID (my_id).
    When sending to a 'slave_id', it places 'slave_id' in frame.identifier
    so that the slave sees it (i.e. the slave is filtering on its own ID).
    It expects replies (FlowControl, etc.) on its own ID (my_id).
    """
    def __init__(self, my_id: int, bus: SimulatedBus):
        self.my_id = my_id
        self.bus = bus

    def sendDataSingleFrame(self, slave_id: int, payload: bytes):
        """
        Example: send a single-frame message to 'slave_id'.
        - frame.identifier = slave_id
        - the slave sees it when it calls `bus.read(slave_id, ...)`
        - We then wait for a reply on our own ID.
        """
        if len(payload) > 7:
            raise ValueError("This example only handles single-frame if payload <= 7 bytes")

        # Single-frame PCI: 0x00 | (length_of_payload)
        pci_byte = (0x00 | len(payload)) & 0x0F
        data = bytes([pci_byte]) + payload

        # Build frame: CAN ID = slave_id
        frame = CanFrame(identifier=slave_id, data=data)
        self.bus.send(frame)

        # Wait for response (FlowControl or any ack) on our ID
        resp = self.bus.read(self.my_id, timeout=1.0)
        if resp is None:
            print(f"[MASTER 0x{self.my_id:X}] No response from slave 0x{slave_id:X}")
        else:
            print(f"[MASTER 0x{self.my_id:X}] Received response from ID=0x{resp.identifier:X}, data={resp.data}")

class IsoTpSlave:
    """
    The 'slave' node has a 'my_id' and knows the master's ID.
    It filters frames by calling `bus.read(my_id, ...)`.
    When a valid ISO-TP frame arrives, it interprets PCI and,
    if needed, sends a FlowControl frame or an acknowledgment
    by placing 'master_id' in the CAN frame’s identifier.
    """
    def __init__(self, my_id: int, master_id: int, bus: SimulatedBus):
        self.my_id = my_id
        self.master_id = master_id
        self.bus = bus

    def poll(self):
        """
        Periodically called to see if there's any new frame on the bus
        addressed to my_id (the slave).
        """
        incoming_frame = self.bus.read(self.my_id, timeout=0.2)
        if not incoming_frame:
            return  # No new frame

        # Interpret the "PCI" (protocol control info)
        pci = incoming_frame.data[0] & 0xF0
        if pci == 0x00:
            # Single-frame
            length = incoming_frame.data[0] & 0x0F
            payload = incoming_frame.data[1:1+length]
            print(f"[SLAVE 0x{self.my_id:X}] Received Single Frame from ID=0x{incoming_frame.identifier:X}, "
                  f"payload={payload}")

            # Send a FlowControl or ack back to the master
            # Typically 0x30 for FC + 0x00 blocksize + 0x00 ST
            ack_data = bytes([0x30, 0x00, 0x00])  # FlowControl CTS
            ack_frame = CanFrame(identifier=self.master_id, data=ack_data)
            self.bus.send(ack_frame)
        else:
            # For brevity, we only show single-frame logic
            print(f"[SLAVE 0x{self.my_id:X}] Received an unhandled PCI: {pci:X}")

def main():
    # Create a shared “bus”
    bus = SimulatedBus()

    # Let's say the MASTER ID is 0x100, the SLAVE ID is 0x200
    master_id = 0x100
    slave_id = 0x200

    master = IsoTpMaster(my_id=master_id, bus=bus)
    slave = IsoTpSlave(my_id=slave_id, master_id=master_id, bus=bus)

    # Master -> Slave
    # Master sends data with “frame.identifier = slave_id”
    # Slave sees it by calling “bus.read(slave_id, ...)”
    # Then slave replies “frame.identifier = master_id”
    payload = b'\x11\x22\x33\x44'
    master.sendDataSingleFrame(slave_id, payload)

    # Slave checks if there's something for it (address=0x200)
    slave.poll()

    # For demonstration, the master read is inside sendDataSingleFrame(),
    # which waits for the slave's FC on ID=0x100.

if __name__ == "__main__":
    main()
