#!/usr/bin/env python3
"""Generate the openUC2 CANopen EDS file for CANopenEditor."""
import datetime
now = datetime.datetime.now()
date_str = now.strftime("%m-%d-%Y")
time_str = now.strftime("%I:%M%p")

DT_I8="0x0002"; DT_I16="0x0003"; DT_I32="0x0004"
DT_U8="0x0005"; DT_U16="0x0006"; DT_U32="0x0007"
DT_STR="0x0009"; DT_DOMAIN="0x000F"
L=[]
def w(s=""): L.append(s)
def var(idx,sub,name,dt,access,default,pdomap=0,storage="RAM",desc=""):
    tag=f"{idx:04X}" if sub is None else f"{idx:04X}sub{sub:X}"
    w(f"[{tag}]"); w(f"ParameterName={name}"); w("ObjectType=0x7")
    w(f";StorageLocation={storage}"); w(f"DataType={dt}"); w(f"AccessType={access}")
    w(f"DefaultValue={default}"); w(f"PDOMapping={pdomap}")
    if desc: w(f";Description={desc}")
    w()
def rec(idx,name,nsubs,st="PERSIST_COMM"):
    w(f"[{idx:04X}]"); w(f"ParameterName={name}"); w("ObjectType=0x9")
    w(f";StorageLocation={st}"); w(f"SubNumber=0x{nsubs:X}"); w()
def arr(idx,name,nsubs,st="RAM"):
    w(f"[{idx:04X}]"); w(f"ParameterName={name}"); w("ObjectType=0x8")
    w(f";StorageLocation={st}"); w(f"SubNumber=0x{nsubs:X}"); w()
def sarr(idx,name,dt,access,n,default,pdomap=1,st="RAM"):
    arr(idx,name,n+1,st)
    var(idx,0,"Number of entries",DT_U8,"ro",f"0x{n:02X}")
    for i in range(1,n+1): var(idx,i,f"{name} {i}",dt,access,default,pdomap)
def pdo_comm(idx,name,cobid,evttimer=0):
    rec(idx,name,6)
    var(idx,0,"Highest sub-index supported",DT_U8,"ro","0x06")
    var(idx,1,"COB-ID used by PDO",DT_U32,"rw",cobid)
    var(idx,2,"Transmission type",DT_U8,"rw","254")
    var(idx,3,"Inhibit time",DT_U16,"rw","0")
    var(idx,5,"Event timer",DT_U16,"rw",str(evttimer))
    var(idx,6,"SYNC start value",DT_U8,"rw","0")
def pdo_map(idx,name,maps):
    rec(idx,name,9)
    used=len([m for m in maps if m!=0])
    var(idx,0,"Number of mapped objects",DT_U8,"rw",str(used))
    for i in range(1,9):
        if i<=len(maps) and maps[i-1]!=0:
            oi,os,ob=maps[i-1]; val=f"0x{oi:04X}{os:02X}{ob:02X}"
        else: val="0x00000000"
        var(idx,i,f"Application object {i}",DT_U32,"rw",val)

# === FILE HEADER ===
w("[FileInfo]"); w("FileName=openUC2_satellite.eds"); w("FileVersion=1"); w("FileRevision=1")
w("LastEDS="); w("EDSVersion=4.0")
w("Description=openUC2 universal satellite node - motor laser LED IO galvo scanner OTA")
w(f"CreationTime={time_str}"); w(f"CreationDate={date_str}"); w("CreatedBy=openUC2")
w(f"ModificationTime={time_str}"); w(f"ModificationDate={date_str}"); w("ModifiedBy=openUC2"); w()

w("[DeviceInfo]"); w("VendorName=openUC2 GmbH"); w("VendorNumber=0x55C2")
w("ProductName=UC2 ESP32-S3 Satellite Node"); w("ProductNumber=0x00010001")
w("RevisionNumber=0x00000001")
for b in [10,20,50]: w(f"BaudRate_{b}=0")
for b in [125,250,500]: w(f"BaudRate_{b}=1")
w("BaudRate_800=0"); w("BaudRate_1000=1")
w("SimpleBootUpMaster=0"); w("SimpleBootUpSlave=1"); w("Granularity=8")
w("DynamicChannelsSupported=0"); w("CompactPDO=0"); w("GroupMessaging=0")
w("NrOfRXPDO=4"); w("NrOfTXPDO=4"); w("LSS_Supported=1"); w("NG_Slave=0"); w()

w("[DummyUsage]"); w("Dummy0001=0")
for i in range(2,8): w(f"Dummy000{i}=1")
w()
w("[Comments]"); w("Lines=5")
w("Line1=openUC2 CANopen Object Dictionary for all satellite node types")
w("Line2=Motor(4ax), Laser(3ch), LED, DigIO, AnalogIO, Encoder, Joystick, Galvo, OTA")
w("Line3=Node-ID: 1=master 11=X 12=Y 13=Z 14=A 20=LED 21=joy 30=galvo")
w("Line4=One EDS for all nodes. Active modules detected at boot from PinConfig.")
w("Line5=https://github.com/youseetoo/uc2-esp32"); w()

# === MANDATORY ===
w("[MandatoryObjects]"); w("SupportedObjects=3"); w("1=0x1000"); w("2=0x1001"); w("3=0x1018"); w()
var(0x1000,None,"Device type",DT_U32,"ro","0x000F0191",storage="PERSIST_COMM",desc="CiA 401 generic I/O")
var(0x1001,None,"Error register",DT_U8,"ro","0x00",pdomap=1)
rec(0x1018,"Identity",5,"PERSIST_COMM")
var(0x1018,0,"Highest sub-index supported",DT_U8,"ro","0x04")
var(0x1018,1,"Vendor-ID",DT_U32,"ro","0x000055C2",desc="openUC2 GmbH")
var(0x1018,2,"Product code",DT_U32,"ro","0x00010001")
var(0x1018,3,"Revision number",DT_U32,"ro","0x00000001")
var(0x1018,4,"Serial number",DT_U32,"ro","0x00000000",desc="Read from NVS at boot")

# === OPTIONAL STD OBJECTS ===
opt=[0x1003,0x1005,0x1006,0x1007,0x1010,0x1011,0x1012,0x1014,0x1015,0x1016,0x1017,0x1019,
     0x1200,0x1280,0x1400,0x1401,0x1402,0x1403,0x1600,0x1601,0x1602,0x1603,
     0x1800,0x1801,0x1802,0x1803,0x1A00,0x1A01,0x1A02,0x1A03]
w("[OptionalObjects]"); w(f"SupportedObjects={len(opt)}")
for i,x in enumerate(opt,1): w(f"{i}=0x{x:04X}")
w()

# 0x1003 error field
arr(0x1003,"Pre-defined error field",17)
var(0x1003,0,"Number of errors",DT_U8,"rw","")
for i in range(1,17): var(0x1003,i,"Standard error field",DT_U32,"ro","")

var(0x1005,None,"COB-ID SYNC message",DT_U32,"rw","0x00000080",storage="PERSIST_COMM")
var(0x1006,None,"Communication cycle period",DT_U32,"rw","0",storage="PERSIST_COMM")
var(0x1007,None,"Synchronous window length",DT_U32,"rw","0",storage="PERSIST_COMM")

arr(0x1010,"Store parameters",5)
var(0x1010,0,"Highest sub-index supported",DT_U8,"ro","0x04")
for i in range(1,5): var(0x1010,i,f"Save group {i}",DT_U32,"rw","0x00000001")
arr(0x1011,"Restore default parameters",5)
var(0x1011,0,"Highest sub-index supported",DT_U8,"ro","0x04")
for i in range(1,5): var(0x1011,i,f"Restore group {i}",DT_U32,"rw","0x00000001")

var(0x1012,None,"COB-ID time stamp object",DT_U32,"rw","0x00000100",storage="PERSIST_COMM")
var(0x1014,None,"COB-ID EMCY",DT_U32,"rw","$NODEID+0x80",storage="PERSIST_COMM")
var(0x1015,None,"Inhibit time EMCY",DT_U16,"rw","0",storage="PERSIST_COMM")
arr(0x1016,"Consumer heartbeat time",9,"PERSIST_COMM")
var(0x1016,0,"Number of entries",DT_U8,"ro","0x08")
for i in range(1,9): var(0x1016,i,f"Consumer heartbeat time {i}",DT_U32,"rw","0x00000000")
var(0x1017,None,"Producer heartbeat time",DT_U16,"rw","500",storage="PERSIST_COMM")
var(0x1019,None,"Synchronous counter overflow value",DT_U8,"rw","0",storage="PERSIST_COMM")

# SDO server/client
rec(0x1200,"SDO server parameter",3,"RAM")
var(0x1200,0,"Highest sub-index supported",DT_U8,"ro","0x02")
var(0x1200,1,"COB-ID client to server (rx)",DT_U32,"ro","$NODEID+0x600")
var(0x1200,2,"COB-ID server to client (tx)",DT_U32,"ro","$NODEID+0x580")
rec(0x1280,"SDO client parameter",4,"PERSIST_COMM")
var(0x1280,0,"Highest sub-index supported",DT_U8,"ro","0x03")
var(0x1280,1,"COB-ID client to server (tx)",DT_U32,"rw","$NODEID+0x600")
var(0x1280,2,"COB-ID server to client (rx)",DT_U32,"rw","$NODEID+0x580")
var(0x1280,3,"Node-ID of the SDO server",DT_U8,"rw","1")

# === RPDOs ===
pdo_comm(0x1400,"RPDO1 comm param","$NODEID+0x200")
pdo_map(0x1600,"RPDO1 mapping",[(0x2000,1,32),(0x2002,1,32)])
pdo_comm(0x1401,"RPDO2 comm param","$NODEID+0xC0000300")
pdo_map(0x1601,"RPDO2 mapping",[(0x2003,0,8),(0x2007,0,8),(0x2005,0,8),(0x2100,1,16),(0x2100,2,16)])
pdo_comm(0x1402,"RPDO3 comm param","$NODEID+0xC0000400")
pdo_map(0x1602,"RPDO3 mapping",[(0x2200,0,8),(0x2201,0,8),(0x2202,0,32)])
pdo_comm(0x1403,"RPDO4 comm param","$NODEID+0xC0000500")
pdo_map(0x1603,"RPDO4 mapping",[(0x2600,1,32),(0x2600,2,32)])

# === TPDOs ===
pdo_comm(0x1800,"TPDO1 comm param","$NODEID+0x180",evttimer=100)
pdo_map(0x1A00,"TPDO1 mapping",[(0x2001,1,32),(0x2004,0,8)])
pdo_comm(0x1801,"TPDO2 comm param","$NODEID+0xC0000280",evttimer=200)
pdo_map(0x1A01,"TPDO2 mapping",[(0x2300,1,8),(0x2300,2,8),(0x2310,1,16),(0x2310,2,16)])
pdo_comm(0x1802,"TPDO3 comm param","$NODEID+0xC0000380",evttimer=50)
pdo_map(0x1A02,"TPDO3 mapping",[(0x2340,1,32),(0x2340,2,32)])
pdo_comm(0x1803,"TPDO4 comm param","$NODEID+0xC0000480",evttimer=50)
pdo_map(0x1A03,"TPDO4 mapping",[(0x2400,1,16),(0x2400,2,16),(0x2400,3,16),(0x2400,4,16)])

# === MANUFACTURER OBJECTS ===
mfr=[0x2000,0x2001,0x2002,0x2003,0x2004,0x2005,0x2006,0x2007,
     0x2010,0x2011,0x2012,0x2013,0x2014,
     0x2020,0x2021,0x2022,0x2023,
     0x2100,0x2101,0x2102,
     0x2200,0x2201,0x2202,0x2203,0x2204,
     0x2300,0x2301,0x2310,0x2320,0x2330,0x2340,
     0x2400,0x2401,0x2402,
     0x2500,0x2501,0x2502,0x2503,0x2504,0x2505,
     0x2600,0x2601,0x2602,0x2603,0x2604,0x2605,0x2606,0x2607,0x2608,
     0x2F00,0x2F01,0x2F02,0x2F03]
w("[ManufacturerObjects]"); w(f"SupportedObjects={len(mfr)}")
for i,x in enumerate(mfr,1): w(f"{i}=0x{x:04X}")
w()

# Motor
sarr(0x2000,"Motor target position",DT_I32,"rw",4,"0",1)
sarr(0x2001,"Motor actual position",DT_I32,"ro",4,"0",1)
sarr(0x2002,"Motor speed steps/s",DT_U32,"rw",4,"15000",1)
var(0x2003,None,"Motor command word",DT_U8,"rw","0",1,desc="0=stop 1=rel 2=abs 3=home 4=setpos")
var(0x2004,None,"Motor status word",DT_U8,"ro","0",1,desc="b0=run b1=homed b2=endstop b3=err")
var(0x2005,None,"Motor enable",DT_U8,"rw","0",1,desc="1=drivers on (/motor_set isen)")
sarr(0x2006,"Motor acceleration",DT_U32,"rw",4,"100000",0)
var(0x2007,None,"Motor is-absolute flag",DT_U8,"rw","0",1)

# Homing
var(0x2010,None,"Homing command",DT_U8,"rw","0",desc="0=idle 1=start (/home_act)")
var(0x2011,None,"Homing speed",DT_U32,"rw","15000")
var(0x2012,None,"Homing direction",DT_I8,"rw","1",desc="-1 or +1")
var(0x2013,None,"Homing timeout ms",DT_U32,"rw","20000")
var(0x2014,None,"Endstop release steps",DT_I32,"rw","3000")

# TMC
var(0x2020,None,"TMC microsteps",DT_U16,"rw","16")
var(0x2021,None,"TMC RMS current mA",DT_U16,"rw","700")
var(0x2022,None,"TMC StallGuard threshold",DT_U8,"rw","15")
var(0x2023,None,"TMC config word",DT_U32,"rw","0",desc="semin|semax|blank_time|toff packed")

# Laser
sarr(0x2100,"Laser PWM 0-1023",DT_U16,"rw",3,"0",1)
sarr(0x2101,"Laser pin assignment",DT_I8,"rw",3,"-1",0)
var(0x2102,None,"Laser despeckle us",DT_U32,"rw","0")

# LED
var(0x2200,None,"LED array mode",DT_U8,"rw","0",1,desc="0=off 1=on 2=left 3=right 4=top 5=bot 6=custom")
var(0x2201,None,"LED brightness",DT_U8,"rw","128",1)
var(0x2202,None,"LED colour 0x00RRGGBB",DT_U32,"rw","0x00FFFFFF",1)
var(0x2203,None,"LED array bulk data",DT_DOMAIN,"rw","",desc="SDO block: N x {id,r,g,b}")
var(0x2204,None,"LED pixel count",DT_U16,"ro","64")

# Digital I/O
sarr(0x2300,"Digital input state",DT_U8,"ro",4,"0",1)
sarr(0x2301,"Digital output cmd",DT_U8,"rw",4,"0",1)

# Analog
sarr(0x2310,"Analog input ADC",DT_U16,"ro",4,"0",1)
sarr(0x2320,"DAC output",DT_U16,"rw",2,"0",0)

# Temperature
var(0x2330,None,"Temperature x100 degC",DT_I16,"ro","0",desc="DS18B20: 2500=25.00C")

# Encoder
sarr(0x2340,"Encoder position",DT_I32,"ro",4,"0",1)

# Joystick
sarr(0x2400,"Joystick axis",DT_I16,"ro",4,"0",1)
var(0x2401,None,"Joystick buttons",DT_U16,"ro","0",1)
var(0x2402,None,"Joystick speed mult",DT_U8,"rw","2")

# System
var(0x2500,None,"Firmware version",DT_STR,"ro","v0.0.0")
var(0x2501,None,"Board type name",DT_STR,"ro","unknown")
var(0x2502,None,"Module enable bitmask",DT_U32,"rw","0",desc="b0=mot b1=las b2=led b3=din b4=dout b5=ain b6=dac b7=enc b8=joy b9=galvo b10=tmc")
var(0x2503,None,"Uptime seconds",DT_U32,"ro","0")
var(0x2504,None,"Free heap bytes",DT_U32,"ro","0")
var(0x2505,None,"CAN error counter",DT_U32,"ro","0")

# Galvo
sarr(0x2600,"Galvo target pos",DT_I32,"rw",2,"0",1)
sarr(0x2601,"Galvo actual pos",DT_I32,"ro",2,"0",1)
var(0x2602,None,"Galvo command",DT_U8,"rw","0",1,desc="0=idle 1=goto 2=line 3=raster 4=freerun")
var(0x2603,None,"Galvo status",DT_U8,"ro","0",1,desc="b0=moving b1=scanning b2=error")
var(0x2604,None,"Galvo scan speed pts/s",DT_U32,"rw","1000")
var(0x2605,None,"Galvo nStepsLine",DT_U16,"rw","100")
var(0x2606,None,"Galvo nStepsPixel",DT_U16,"rw","100")
var(0x2607,None,"Galvo dStepsLine",DT_U16,"rw","1")
var(0x2608,None,"Galvo dStepsPixel",DT_U16,"rw","1")

# OTA
var(0x2F00,None,"OTA firmware data",DT_DOMAIN,"wo","",desc="SDO block transfer target")
var(0x2F01,None,"OTA size bytes",DT_U32,"rw","0")
var(0x2F02,None,"OTA CRC32",DT_U32,"rw","0")
var(0x2F03,None,"OTA status",DT_U8,"ro","0",desc="0=idle 1=recv 2=verify 3=reboot 4=err")

out="/home/claude/openUC2_satellite.eds"
with open(out,"w") as f: f.write("\n".join(L))
print(f"Written {out}: {len(mfr)} manufacturer objects, {len(L)} lines")
