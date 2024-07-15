erRest Api Endpoint Description
==============================

| Endpoint                                  | Type | Socket|
| ------------------------------------------| ---- | ----  |
| [/modules_set](#modules_set)              | Post | false |
| [/modules_get](#modules_get)              | Get  | false |
|   |  |
| [/ledarr_act](#ledarr_act)                | Post | true  |
| [/ledarr_get](#ledarr_get)                | Get  | false |
| [/ledarr_set](#ledarr_set)                | Post | false |
|  |  |
| [/motor_act](#motor_act)                  | Post | true  |
| [/motor_get](#motor_get)                  | Get  | false |
| [/motor_set](#motor_set)                  | Post | false |
| [/motor_setcalibration](#motor_set)       | Post | false |
|   |  |
| [/wifi/scan](#wifiscan)                   | Get  | false |
| [/wifi/connect](#wificonnect)             | Post | false |
|   |  |
| [/bt_scan](#bt_scan)                      | Get  | false |
| [/bt_connect](#bt_connect)                | Post | false |
| [/bt_remove](#bt_remove)                  | Post | false |
| [/bt_paireddevices](#bt_paireddevices)    | Post | false |
|   |  |
| [/analog_joystick_set](#analog_joystick_set)    | Post | false |
| [/analog_joystick_get](#analog_joystick_get)    | Post | false |


## State

### /state_get

*SERIAL:*
```json
# returns 1 if busy, else 0
{"task":"/state_get", "isBusy":1}

# returns 1 if controlled by pscontroller
{"task":"/state_get", "pscontroller":1}

# returns compiling and fw information
{"task":"/state_get", "state":1}
```


### /state_set

*SERIAL:*
```json
// turns on/off debugging serial commands
{"task":"/state_set", "isdebug":1}
```


### /state_act

*SERIAL:*
```json
// restarts controller
{"task":"/state_act", "restart":1}

// adds delay in table processor
{"task":"/state_act", "delay":10}

// adds delay in table processor
{"task":"/state_act", "delay":10}

// resets the isBusy flag
{"task":"/state_act", "isBusy":0}

// sets control of PSController on/off
{"task":"/state_act", "pscontroller":0}
```

## Modules

### /modules_set


isdebug

POST
```
{
    "modules" :
    {
        "led" : 1,
        "motor": 1,
        "slm" : 0,
        "sensor" : 0,
        "pid" : 0,
        "laser" : 0,
        "dac" : 0,
        "analog" : 0,
        "digitalout" : 0,
        "scanner" : 0
    }

}
```

### /modules_get

GET
```
{
    "modules" :
    {
        "led" : 1,
        "motor": 1,
        "slm" : 0,
        "sensor" : 0,
        "pid" : 0,
        "laser" : 0,
        "dac" : 0,
        "analog" : 0,
        "digitalout" : 0,
        "scanner" : 0
    }

}
```

## LED Array

### /ledarr_act

| Item      | Type              | Description |
| ----------|----------------- | ---- |
| LEDArrMode | int | //enum "array", "full",  "single", "off", "left", "right", "top", "bottom" |
| led_array | Object[] | The rgb data foreach led, depending on the mode only one color item get set |

*SERIAL*

```json
# set multiple invididual LEDs with a certain colour
{"task": "/ledarr_act",   
  "led": {
      "LEDArrMode": 0,
      "led_array": [
          {
              "b": 255,
              "g": 255,
              "id": 0,
              "r": 255
          },
          {
              "b": 255,
              "g": 255,
              "id": 1,
              "r": 255
          }
      ]
  }}

```bash
curl -X POST http://192.168.4.2/ledarr_act \
     -H "Content-Type: application/json" \
     -d '{"led": { "LEDArrMode": 0, "led_array": [ { "b": 255, "g": 255, "id": 0, "r": 255 }, { "b": 255, "g": 255, "id": 1, "r": 255 } ] }}'
```


# Set all LEDs with a certain colour
{"task": "/ledarr_act",   
  "led": {
      "LEDArrMode": 1,
      "led_array": [
          {
              "b": 255,
              "g": 255,
              "r": 255
          }
      ]
  }}

# Set all LEDs (left half) with a certain colour
{"task": "/ledarr_act",   
  "led": {
      "LEDArrMode": 4,
      "led_array": [
          {
              "b": 255,
              "g": 255,
              "r": 255
          }
      ]
  }}
```

*HTTP*

POST
```
{
    "led": {
        "LEDArrMode": 1,
        "led_array": [
            {
                "b": 0,
                "g": 0,
                "id": 0,
                "r": 0
            }
        ]
    }
}
```

### /ledarr_get

*SERIAL:*

```json
{"task": "/ledarr_get"}
```

*HTTP:*

GET
```
{
    ledArrNum : 64,
    ledArrPin : 27
}
```

### /ledarr_set

*SERIAL:*

```json
{"task": "/ledarr_set", "led":{"ledArrPin":32, "ledArrNum":64}}
```

*HTTP:*
POST
```
{
    led:
    {
        ledArrPin: 27,
        ledArrNum: 64
    }
}
```

## Laser

### /laser_act

```json
{"task": "/laser_act", "LASERid":0, "LASERval":1000, "LASERdespeckle":1, "LASERdespecklePeriod":20}
```

```bash
curl -X POST http://192.168.4.1/laser_act \
     -H "Content-Type: application/json" \
     -d '{ "LASERid":0, "LASERval":1000, "LASERdespeckle":1, "LASERdespecklePeriod":20}'
```

### /laser_get

```json
{"task": "/laser_get"}
```

### /laser_set

```json
{"task": "/laser_set", "LASERid":2, "LASERpin":19}
```

## Motor

# X,Y,Z,A => 1,2,3,0

### /motor_act

*CURL:*

```bash
curl -X POST http://192.168.4.1/motor_act \
     -H "Content-Type: application/json" \
     -d '{"motor": { "steppers": [ { "stepperid": 1, "position": 10000, "speed": 15000, "isabs": 0, "isaccel":0} ] } }'
```


*SERIAL:*

```json
{"task":"/motor_act",
    "motor":
    {
        "steppers": [
            { "stepperid": 0, "position": 100, "speed": 2000, "isabs": 0, "isaccel":0},
            { "stepperid": 1, "speed": 2000, "isabs": 0, "isforever":0, "isaccel":0},
            { "stepperid": 2, "isstop": 1}
        ]
    }
}
```


*HTTP*
POST
```
{
    motor:
    {
        steppers: [
            { stepperid: 0, speed: 0, isforever: 0 },
            { stepperid: 1, speed: 0, isforever: 0 },
            { stepperid: 2, speed: 0, isforever: 0 },
            { stepperid: 3, speed: 0, isforever: 0 }
        ]
    }
}
```

### /motor_get

*SERIAL*:

```json
# all information
{"task":"/motor_get"}

# only position
{"task":"/motor_get", "position": 1}
```



*HTTP:*

GET
```
{
  "steppers": [
    {
      "stepperid": 0,
      "dir": 0,
      "step": 0,
      "enable": 0,
      "dir_inverted": false,
      "step_inverted": false,
      "enable_inverted": false,
      "position": 0,
      "speed": 0,
      "speedmax": 20000
    },
    {
      "stepperid": 1,
      "dir": 21,
      "step": 19,
      "enable": 18,
      "dir_inverted": false,
      "step_inverted": false,
      "enable_inverted": true,
      "position": 0,
      "speed": 0,
      "speedmax": 20000
    },
    {
      "stepperid": 2,
      "dir": 0,
      "step": 0,
      "enable": 0,
      "dir_inverted": false,
      "step_inverted": false,
      "enable_inverted": false,
      "position": 0,
      "speed": 0,
      "speedmax": 20000
    },
    {
      "stepperid": 3,
      "dir": 0,
      "step": 0,
      "enable": 0,
      "dir_inverted": false,
      "step_inverted": false,
      "enable_inverted": false,
      "position": 0,
      "speed": 0,
      "speedmax": 20000
    }
  ]
}
```

### /motor_set

*SERIAL:*

```json
{"task":"/motor_set",
    "motor":{
        "steppers": [
              { "stepperid": 1, "step": 26, "dir": 16, "enable": 12, "step_inverted": 0, "dir_inverted": 0, "enable_inverted": 0 , "min_pos":0, "max_pos":0},
			         { "stepperid": 2, "step": 25, "dir": 27, "enable": 12, "step_inverted": 0, "dir_inverted": 0, "enable_inverted": 0 , "min_pos":0, "max_pos":0},
			            { "stepperid": 3, "step": 17, "dir": 14, "enable": 12, "step_inverted": 0, "dir_inverted": 0, "enable_inverted": 0 , "min_pos":0, "max_pos":0},
			               { "stepperid": 0, "step": 19, "dir": 18, "enable": 12, "step_inverted": 0, "dir_inverted": 0, "enable_inverted": 0 , "min_pos":0, "max_pos":0}
        ]
    }
}
```


POST
```
{
    motor:
    {
        steppers: [
            { stepperid: 0, step: 0, dir: 0, enable: 0, step_inverted: 0, dir_inverted: 0, enable_inverted: 0 },
            { stepperid: 1, step: 0, dir: 0, enable: 0, step_inverted: 0, dir_inverted: 0, enable_inverted: 0 },
            { stepperid: 2, step: 0, dir: 0, enable: 0, step_inverted: 0, dir_inverted: 0, enable_inverted: 0 },
            { stepperid: 3, step: 0, dir: 0, enable: 0, step_inverted: 0, dir_inverted: 0, enable_inverted: 0 },
        ]
    }
}
```

### /motor_setcalibration

POST
set it like this to apply the current pos as min_pos. min pos gets defined as 0 position
```
{
    motor:
    {
        steppers: [
            { stepperid: id, min_pos: 0 }
        ]
    }
}
```
set it like this to apply the current pos as max_pos. max pos should be a positiv value.
```
{
    motor:
    {
        steppers: [
            { stepperid: id, max_pos: 0 }
        ]
    }
}
```
set min and max pos to reset it
```
{
    motor:
    {
        steppers: [
            { stepperid: id, max_pos: 0, min_pos:0 }
        ]
    }
}
```

## Homing

### /home_act

*SERIAL:*

```json
{"task":"/home_act",
    "home":
    {
        "steppers": [
            { "stepperid": 1, "timeout":1000, "speed":1000, "direction":1, "endposrelease":500}
        ]
    }
}
```

POST
```
{
    home:
    {
        steppers: [
            { "endpospin": 0, "timeout": 10000, "speed": 1000, "direction":1 }
        ]
    }
}
```

### /home_set

*Serial:*

```json
{"task": "/home_set"}
```

### /home_get

*Serial:*

```json
{"task": "/home_get"}
```


## Wifi

### /wifi/scan

GET
```
{
    [
        "ssid1",
        "ssid2"
    ]
}
```

### /wifi/connect

POST
```
{
    "ssid": "networkid"
    "PW" : "password"
    "AP" : 0
}
```
/bt_scan
===========
GET
```
{
    [
        {
            "name" :"HyperX",
            "mac": "01:02:03:04:05:06"
        }
        ,
        {
            "name": "",
            "mac": "01:02:03:04:05:06"
        },
    ]
}
```
/bt_paireddevices
===========
GET
```
{
    [
        "01:02:03:04:05:06",
        "01:02:03:04:05:06"
    ]
}
```
/bt_connect
psx = 1 Ps3Controller, psx = 2 Ps4Controller
===========
POST
```
{
    "mac": "01:02:03:04:05:06",
    "psx": 0
}
```
/bt_remove
===========
POST
```
{
    "mac": "01:02:03:04:05:06",
}
```


/analog_joystick_set
============
POST
```
{
    "joy" :
    {
        "joyX" : 35,
        "joyY": 34
    }

}
```

/analog_joystick_get
============
GET
```
{
    "joy" :
    {
        "joyX" : 35,
        "joyY": 34
    }

}
```


/digitalin_set
==========
POST
```
{
    "digitalinid":1,
    "digitalinpin":39
}
```

/digitalin_get
==========
POST
```
{
    "digitalinid":1
}
```


/message_act
==========
POST
```bash
curl -X POST http://192.168.4.1/message_act \
     -H "Content-Type: application/json" \
     -d '{"key":1, "value":1}'
```
