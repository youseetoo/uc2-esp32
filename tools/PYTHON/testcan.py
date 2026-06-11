from usb_can_adapter_v1 import UsbCanAdapter

def handle_pdu(can_id: int, payload: bytes):
    # First byte in every UC2 payload is *always* a MessageTypeID
    msg_type = CANMessageTypeID(payload[0])
    body     = payload[1:]

    if msg_type == CANMessageTypeID.LED_ACT:
        led = LedCommand.from_bytes(body)
        print("LED cmd:", led)

    # Extend: elif msg_type == MOTOR_STATE … etc.

uca = UsbCanAdapter()
uca.set_port("/dev/ttyUSB0")        # or imu_f99xB20.get_port()
uca.adapter_init()
uca.command_settings(speed=500000)  # 500 kbit/s used in firmware

rx = IsoTpReceiver(handle_pdu)

while True:
    if uca.frame_receive() == -1:
        break
    frame = uca.extract_data(uca.frame)
    can_id = int(frame["id"], 16)
    data   = frame["data"]
    rx.feed(can_id, data)
