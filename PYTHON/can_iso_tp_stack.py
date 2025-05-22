# can_iso_tp_stack.py
#  – full bidirectional CAN‑ISO‑TP stack for your ESP32 control library –
#    message IDs: can_messagetype.h :contentReference[oaicite:0]{index=0}:contentReference[oaicite:1]{index=1}
#    LedCommand layout: can_controller.cpp :contentReference[oaicite:2]{index=2}:contentReference[oaicite:3]{index=3}

from __future__ import annotations
from enum   import IntEnum
from dataclasses import dataclass
import struct, time
from usb_can_adapter_v1 import UsbCanAdapter

# ─────────────────────────────────── datatypes ──────────────────────────────────
class CANMessageTypeID(IntEnum):
    HEARTBEAT        = 0x00
    MOTOR_ACT        = 0x10
    MOTOR_GET        = 0x11
    MOTOR_STATE      = 0x12
    HOME_ACT         = 0x13
    HOME_STATE       = 0x14
    MOTOR_ACT_REDUCED= 0x15
    LASER_ACT        = 0x20
    LASER_STATE      = 0x21
    LED_ACT          = 0x30
    LED_STATE        = 0x31
    SENSOR_GET       = 0x40
    SENSOR_STATE     = 0x41
    BROADCAST        = 0xF0
    ERROR            = 0xF1
    SYNC             = 0xF2


class LedMode(IntEnum):  # matches firmware enum
    OFF=0; SOLID=1; RING=2; CIRCLE=3; HALF=4; SINGLE=5; ARRAY=6


@dataclass
class LedCommand:
    qid:int
    mode:LedMode
    r:int; g:int; b:int
    radius:int
    region:str     # ≤8 chars
    ledIndex:int

    _FMT = struct.Struct("<HBBBBB8sxH")   # 18 bytes incl. 1 pad

    @classmethod
    def from_bytes(cls, raw:bytes) -> "LedCommand":
        if len(raw)!=cls._FMT.size: raise ValueError("len!=18")
        qid,mode,r,g,b,radius,reg,led = cls._FMT.unpack(raw)
        return cls(qid,LedMode(mode),r,g,b,radius,reg.split(b'\x00')[0].decode(),led)

    def to_bytes(self)->bytes:
        region = self.region.encode()[:8].ljust(8,b'\x00')
        return self._FMT.pack(self.qid,int(self.mode),self.r,self.g,
                              self.b,self.radius,region,self.ledIndex)


# ───────────────────────────── ISO‑TP receiver (reassembler) ────────────────────
class IsoTpReceiver:
    def __init__(self, on_complete):
        self.cb   = on_complete
        self.buff = {}          # id -> dict

    def feed(self, can_id:int, dat:bytes):
        pci = dat[0] & 0xF0
        if pci==0x00:                               # SF
            size = dat[0] & 0x0F
            self.cb(can_id, dat[1:1+size])
        elif pci==0x10:                             # FF
            size  = ((dat[0]&0x0F)<<8)|dat[1]
            self.buff[can_id]={"rem":size-6,"next":1,"buf":bytearray(dat[2:8])}
        elif pci==0x20:                             # CF
            s=self.buff.get(can_id)
            if not s or (dat[0]&0x0F)!=s["next"]%16: return
            s["buf"].extend(dat[1:])
            s["rem"]-=len(dat)-1
            s["next"]+=1
            if s["rem"]<=0:
                self.cb(can_id,bytes(s["buf"])[:len(s["buf"])+s["rem"]])
                del self.buff[can_id]


# ───────────────────────────── ISO‑TP sender (blocking) ─────────────────────────
class IsoTpSender:
    _SF_MAX  = 7
    _ST_MIN  = 0      # ms
    _BS_DEF  = 0      # 0 = unlimited

    def __init__(self, uca:UsbCanAdapter, tx_id:int, rx_id:int):
        self.u  = uca
        self.tx = tx_id
        self.rx = rx_id

    @staticmethod
    def _frame(id11:int, payload:bytes)->bytearray:
        hdr = 0xC0 | len(payload)
        return bytearray([0xAA,hdr,(id11>>8)&0x7F,id11&0xFF,*payload,0x55])

    def _send_raw(self, payload:bytes): self.u.frame_send(self._frame(self.tx,payload))

    def _wait_fc(self, tout=0.3):
        t=time.time()
        while time.time()-t<tout:
            if self.u.frame_receive()!=-1:
                fr=self.u.frame; dl=fr[1]&0x0F
                rid=((fr[2]&0x7F)<<8)|fr[3]
                if rid==self.rx and (fr[4]&0xF0)==0x30:   # FC
                    return fr[5],fr[6]
        raise TimeoutError

    def send(self, data:bytes):
        if len(data)<=self._SF_MAX:
            self._send_raw(bytes([0x00|len(data)])+data)
            return
        # FF
        rem=len(data); seq=1
        self._send_raw(bytes([0x10|((rem>>8)&0x0F),rem&0xFF])+data[:6])
        ptr=6
        bs,st=self._wait_fc(); bs=bs or 4095; st=st or self._ST_MIN
        while ptr<rem:
            take=min(7,rem-ptr)
            self._send_raw(bytes([0x20|(seq&0x0F)])+data[ptr:ptr+take])
            ptr+=take; seq=(seq+1)&0x0F; bs-=1
            if bs==0 and ptr<rem:
                bs,st=self._wait_fc(); bs=bs or 4095
            time.sleep(st/1000.0)


# ───────────────────────────── application‑layer glue ───────────────────────────
class CanIsoTpStack:
    def __init__(self, port:str, bitrate:int=500_000,
                 central_id:int=0x100, led_node:int=0x015):
        self.u = UsbCanAdapter(); self.u.set_port(port)
        self.u.adapter_init(); self.u.command_settings(speed=bitrate)
        self.rx = IsoTpReceiver(self._dispatch)
        self.tx = IsoTpSender  (self.u, tx_id=led_node, rx_id=central_id)

    def _dispatch(self, can_id:int, payload:bytes):
        mtype=CANMessageTypeID(payload[0])
        body =payload[1:]
        if mtype==CANMessageTypeID.LED_ACT:
            cmd=LedCommand.from_bytes(body)
            print("RX LED",cmd)

    # example api
    def send_led(self, cmd:LedCommand): self.tx.send(bytes([CANMessageTypeID.LED_ACT])+cmd.to_bytes())

    def loop(self):
        while True:
            if self.u.frame_receive()!=-1:
                fr=self.u.extract_data(self.u.frame)
                cid=int(fr["id"],16); self.rx.feed(cid,fr["data"])


# ───────────────────────────── demo main ────────────────────────────────────────
if __name__=="__main__":
    can = CanIsoTpStack("/dev/ttyUSB0")
    can.send_led(LedCommand(1,LedMode.SOLID,255,50,50,0,"all",0))
    can.loop()
