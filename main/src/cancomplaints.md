

how about additional parameters that were handled by can previously? Like the CAN for the galvo? Also can we actually transmit all the relevant information for the homing etc.? Parameters like direction, speed, etc. from host to slave? How about the TMC on the slave side? 

which motors are remote and local? What's the corresopnding json command? 

galvo can only be called locally on slave or from master to slave not on master 

double check when #ifdef MOTOR_CONTROLLER etc. are active - also on master? 

acceleration not part of the motor parameters?

Homing does not seem complete, need to send all the information about direction, speed, polarity of endstop, etc. Routine on slave (or in case of hybrid, master) has to be triggered, we don't want to poll the state of the endstop on the master periodically, should be done on slave as it was previously the case 

should load from pinconfig instead (as it was previuosly the case)
	-DUC2_CAN_TX_PIN=17
	-DUC2_CAN_RX_PIN=18

should rather use the pinconfig here too 

no need to read encoders as they are hardwired to the satelites and used in the cpu cycle 


#ifndef UC2_CAN_TX_PIN
#  define UC2_CAN_TX_PIN  17
#endif
#ifndef UC2_CAN_RX_PIN
#  define UC2_CAN_RX_PIN  18
#endif

we have to add the buildflags also to the individual pinconfig.h for proper linting 

what happens if the master reboots and all slaves are already booted - just catches the hearbeats and knows which capability is available? 

what happens to can_act - scan,etc to get the current ids?

is the ota still working and implemented? First initialization should be going through canopen too?

add docstrings to NMTManager.cpp functions

how are the canopen things actually be wired to the serial interface? Things like bool isOperational(uint8_t nodeId), getnodes, isOnline are not wired to anything, just empty functions; Also the functions to actually send commands e.g. motorMove() are no where used.. 

why this formating? ever used?!
#ifdef UC2_CANOPEN_ENABLED
#  ifdef UC2_CANOPEN_MASTER
#    include "src/CANopen/CanOpenMaster.h"
#  endif
#  ifdef UC2_CANOPEN_SLAVE
#    include "src/CANopen/CanOpenSlave.h"
#  endif

what is happening to the CAN Controller in /Users/bene/Dropbox/Dokumente/Promotion/PROJECTS/uc2-ESP/main/src/can/can_controller.cpp
we should move all the functions (!all!) to the canopen stack. The function set should be identical. each configuration that uses can, should use canopen so that there are not two can implemenatations in parallel 



=> have to add the canopen stack to, all are slaves 
UC2_3_CAN_HAT_Master
seeed_xiao_esp32s3_can_slave_illumination
seeed_xiao_esp32s3_can_slave_galvo
seeed_xiao_esp32s3_can_slave_illumination
seeed_xiao_esp32s3_can_slave_laser
seeed_xiao_esp32s3_can_slave_led
seeed_xiao_esp32s3_can_slave_motor
UC2_4_CAN_HYBRID


=> I guess the NVS is not really necessary since we anyway keep the pinconfig and build flags, so I think this can be reverted largely including removing unncessary code - even previous code and the config set/get endpoints and documentation


## WP1: Runtime Configuration (NVS)

The firmware stores module-enable flags in NVS (Non-Volatile Storage) under the
namespace `uc2cfg`.  At boot, `NVSConfig::load()` populates the global
`runtimeConfig` struct.  Fields that have never been written default to the
compile-time `#ifdef` value.

### Endpoints

| Endpoint        | Direction | Purpose                                 |
| --------------- | --------- | --------------------------------------- |
| `/config_get`   | GET       | Read all current runtime-config fields  |
| `/config_set`   | SET       | Merge partial changes and persist       |
| `/config_reset` | —         | Erase NVS namespace and **reboot**      |

### Testing via Serial (legacy JSON)

Open a serial terminal (115 200 baud) and send:

```json
{"task": "/config_get"}
```

Expected response (fields vary by build):

```json
{
  "motor": true,
  "laser": false,
  "led": true,
  "wifi": true,
  "can": false,
  "can_node_id": 1,
  "can_bitrate": 500000,
  "serial_protocol": 0
  ...
}
```

To change a value (e.g. enable laser, disable motor):

```json
{"task": "/config_set", "config": {"laser": true, "motor": false}}
```

Response:

```json
{"config_saved": true}
```

The change takes effect immediately and survives reboot.

To factory-reset back to compile-time defaults:

```json
{"task": "/config_reset"}
```

The ESP will erase the `uc2cfg` NVS namespace and reboot.

### Testing via Python (UC2-REST)

```python
import serial, json, time

ser = serial.Serial("/dev/ttyUSB0", 115200, timeout=1)
time.sleep(2.5)

# Read config
ser.write(b'{"task": "/config_get"}\n')
time.sleep(0.5)
print(ser.read(ser.in_waiting).decode())

# Set config
cmd = {"task": "/config_set", "config": {"motor": False}}
ser.write((json.dumps(cmd) + "\n").encode())
time.sleep(0.5)
print(ser.read(ser.in_waiting).decode())
```

### Monitoring NVS via ESP-IDF Monitor

When watching the serial output at boot, look for:

```
I (xxx) NVSConfig: RuntimeConfig loaded from NVS
```

If the namespace does not exist yet (first boot):

```
W (xxx) NVSConfig: NVS namespace 'uc2cfg' not found, using compile-time defaults
```
