Rest Api Endpoint Description
==============================

| Endpoint                    | Type |
| --------------------------- | ---- |
| [/ledarr_act](#ledarr_act) | Post |
| [/ledarr_get](#ledarr_get) | Get  |
| [/ledarr_set](#ledarr_set) | Post |
|  |  |
| [/motor_act](#motor_act)   | Post |
| [/motor_get](#motor_get)   | Get  |
| [/motor_set](#motor_set)   | Post |
|   |  |
| [/wifi/scan](#wifiscan)         | Get  |
| [/wifi/connect](#wificonnect)   | Post |
|   |  |
| [/bt_scan](#bt_scan)         | Get  |
| [/bt_connect](#bt_connect)         | Post  |
| [/bt_remove](#bt_remove)         | Post  |
| [/bt_paireddevices](#bt_paireddevices)         | Post  |


/ledarr_act
===========
| Item                    | Description |
| --------------------------- | ---- |
| LEDArrMode | //enum "array", "full",  "single", "off", "left", "right", "top", "bottom" |
| led_array | The rgb data foreach led, depending on the mode only one color item get set |
POST
```
{
    "led": {
        "LEDArrMode": 1,
        "led_array": [
            {
                "blue": 0,
                "green": 0,
                "id": 0,
                "red": 0
            }
        ]
    }
}
```

/ledarr_get
===========
GET
```
{
    ledArrNum : 64,
    ledArrPin : 27
}
```

/ledarr_set
===========
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
/motor_act
===========
POST
```
{
    motor:
    {
        steppers: [
            { stepperid: 0, speed: 0, isforever: 0 }, // a
            { stepperid: 1, speed: 0, isforever: 0 }, // x
            { stepperid: 2, speed: 0, isforever: 0 }, // y
            { stepperid: 3, speed: 0, isforever: 0 }  // z
        ]
    }
}
```
/motor_get
===========
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

/wifi/scan
===========
GET
```
{
    [
        "ssid1",
        "ssid2"
    ]
}
```
/wifi/connect
===========
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
===========
POST
```
{ 
    mac: "01:02:03:04:05:06", 
    psx: 0 
}
```
/bt_remove
===========
POST
```
{ 
    mac: "01:02:03:04:05:06",
}
```