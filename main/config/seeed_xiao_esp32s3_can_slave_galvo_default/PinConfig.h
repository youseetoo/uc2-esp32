#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER
#define ESP32S3_MODEL_XIAO
#define GALVO_CONTROLLER
#define CAN_RECEIVE_GALVO
#define CAN_BUS_ENABLED

struct seeed_xiao_esp32s3_can_slave_galvo : PinConfig
{
     /*
     D0: 1
     D1: 2
     D2: 3
     D3: 4
     D4: 5
     D5: 6
     D6: 43
     D7: 44
     D8: 7
     D9: 8
     D10: 9

     # XIAO

     {"task":"/ledarr_act", "qid":1, "led":{"LEDArrMode":1, "led_array":[{"id":0, "r":255, "g":255, "b":255}]}}
     {"task":"/ledarr_act", "qid":1, "led":{"LEDArrMode":1, "led_array":[{"id":0, "r":0, "g":0, "b":255}]}}

     {"task": "/laser_act", "LASERid":1, "LASERval": 500, "LASERdespeckle": 500,  "LASERdespecklePeriod": 100}
     {"task": "/laser_act", "LASERid":2, "LASERval": 0}
     {"task": "/laser_act", "LASERid":1, "LASERval": 1024}
     {"task": "/laser_act", "LASERid":1, "LASERval": 0}
     {"task": "/laser_act", "LASERid":2, "LASERval": 1024}
     */
     const char *pindefName = "seeed_xiao_esp32s3_can_slave_galvo";
     const unsigned long BAUDRATE = 115200;

     uint8_t galvo_miso = -1;
     uint8_t galvo_sck = GPIO_NUM_8;
     uint8_t galvo_sdi = GPIO_NUM_7;
     uint8_t galvo_cs = GPIO_NUM_9;
     uint8_t galvo_ldac = GPIO_NUM_6;
     uint8_t galvo_laser = GPIO_NUM_43;
     uint8_t galvo_trig_pixel = GPIO_NUM_2;  // D1
     uint8_t galvo_trig_line = GPIO_NUM_3;   // D2
     uint8_t galvo_trig_frame = GPIO_NUM_4;  // D3
     
     uint32_t CAN_ID_CURRENT = CAN_ID_GALVO_0;

     
};
const seeed_xiao_esp32s3_can_slave_galvo pinConfig;
