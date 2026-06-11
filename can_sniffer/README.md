# UC2 CAN sniffer

Tiny passive CAN bus sniffer for debugging the UC2 CANopen network.

## Build & flash

```
cd can_sniffer
pio run -t upload
pio device monitor
```

## What you get

Every CAN frame on the bus printed as:

```
[  12345] ID=0x60B S  DL=8 data=23 00 20 01 B8 0B 00 00
  SDO_RX node=11 DOWNLOAD-REQ 0x2000:1
```

Plus heartbeat NMT state, SDO abort codes, EMCY, TPDO/RPDO direction,
and a `[STATUS]` line every 5 s with TWAI error counters.

## Config (platformio.ini build_flags)

| Flag                 | Default | Notes                                            |
|----------------------|---------|--------------------------------------------------|
| `CAN_TX_PIN`         | 17      | Match your transceiver TX                        |
| `CAN_RX_PIN`         | 18      | Match your transceiver RX                        |
| `CAN_BITRATE_KBPS`   | 500     | Must match the bus                               |
| `LISTEN_ONLY`        | 1       | 1 = passive (no ACK). 0 = NORMAL (sends ACK)     |

`LISTEN_ONLY=1` is safe — it never transmits, so it cannot affect the bus.
Set `LISTEN_ONLY=0` to also act as an ACKing node (useful if there are only
two nodes on the bus and one is silent).

## Debugging the current motor SDO timeout

1. Wire the ESP into the same CAN bus as the master + node 11.
2. Flash with `LISTEN_ONLY=1`.
3. Run `motor_demo.py` and watch the monitor:
   - You should see the master's `SDO_RX node=11 DOWNLOAD-REQ 0x2000:1`.
   - If the slave is processing the write you'll see a matching
     `SDO_TX node=11 DOWNLOAD-ACK 0x2000:1` within ~10 ms.
   - If only the request shows up, the slave isn't responding (firmware bug).
   - If neither shows up, the master's frame never hits the wire
     (Waveshare/wiring/termination issue).
