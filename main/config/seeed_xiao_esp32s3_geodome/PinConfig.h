#pragma once
#include "Arduino.h"
#include "PinConfigDefault.h"
#undef PSXCONTROLLER

// ATTENTION: THIS IS ONLY FOR LINTING!
// #define CORE_DEBUG_LEVEL
#define ESP32S3_MODEL_XIAO
#define LED_CONTROLLER
#define MESSAGE_CONTROLLER

// openUC2 geodesic LED dome (flex PCB, 80 x SK6812-EC20, NA 0.12 ... 0.72), one chain, GRB.
// Source: mAIkroscope-electronics-hardware, kicad/openuc2-geodesic-ledring, production build
// (geodome.py --na .75 --top triangles --freq 5), kicad/dome.json == led_positions.csv.
// One row per LED in chain order: {x, y, ring}. x/y = direction from the focus to the LED
// (x/dist, y/dist) * 1000 in the dome frame (+z = zenith = optical axis), ring 0 = the
// innermost 5 LEDs. Halves/quadrants/rings/circles pick LEDs from this table, ImSwitch uses
// the full 3D positions (imswitch/_data/led_layouts/geodome80.csv) for DPC/FPM.
static const int16_t GEODOME_LED_GEOMETRY[80][3] = {
    {  286,  657,3}, {  330,  549,3}, {  391,  284,2}, {  230,  477,2}, {  263,  343,2}, {  180,  131,1}, {  163,  271,1}, {   99,   72,0},  // 0-7
    {   27,  316,1}, {   94,  521,2}, {  -11,  432,2}, {   56,  638,3}, {  154,  700,3}, {  -83,  666,3}, { -188,  577,3}, { -325,  587,3},  // 8-15
    { -536,  475,3}, { -420,  483,3}, { -149,  460,2}, { -382,  366,2}, { -245,  356,2}, {  -69,  211,1}, { -208,  239,1}, {  -38,  116,0},  // 16-23
    { -292,  123,1}, { -466,  251,2}, { -414,  123,2}, { -590,  251,3}, { -618,  363,3}, { -659,  127,3}, { -607,    0,3}, { -659, -127,3},  // 24-31
    { -618, -363,3}, { -590, -251,3}, { -484,    0,2}, { -466, -251,2}, { -414, -123,2}, { -222,    0,1}, { -292, -123,1}, { -122,    0,0},  // 32-39
    { -208, -239,1}, { -382, -366,2}, { -245, -356,2}, { -420, -483,3}, { -536, -475,3}, { -325, -587,3}, { -188, -577,3}, {  -83, -666,3},  // 40-47
    {  154, -700,3}, {   56, -638,3}, { -149, -460,2}, {   94, -521,2}, {  -11, -432,2}, {  -69, -211,1}, {   27, -316,1}, {  -38, -116,0},  // 48-55
    {   99,  -72,0}, {  163, -271,1}, {  230, -477,2}, {  263, -343,2}, {  330, -549,3}, {  286, -657,3}, {  458, -490,3}, {  491, -357,3},  // 56-63
    {  608, -284,3}, {  713,  -69,3}, {  624, -144,3}, {  391, -284,2}, {  524,  -71,2}, {  408, -144,2}, {  180, -131,1}, {  309,  -72,1},  // 64-71
    {  309,   72,1}, {  524,   71,2}, {  408,  144,2}, {  624,  144,3}, {  713,   69,3}, {  608,  284,3}, {  491,  357,3}, {  458,  490,3},  // 72-79
};

struct seeed_xiao_esp32s3_geodome : PinConfig
{
    /*
     Seeed XIAO ESP32S3 on USB serial, dome data on the same pin as the illumination board
     (UC2_canopen_slave_led): D2 = GPIO3 -> tail pad DI (470R on the flex). 5V/GND from an
     external supply, >= AWG22: 80 LEDs full white are ~2.9 A.

     {"task":"/ledarr_act","led":{"action":"halves","region":"left","r":0,"g":64,"b":0}}
     {"task":"/ledarr_act","led":{"action":"rings","radius":0,"r":0,"g":64,"b":0}}     // ring 0 only
     {"task":"/ledarr_act","led":{"action":"circles","radius":2,"r":0,"g":64,"b":0}}   // rings 0+1
     {"task":"/ledarr_act","led":{"action":"array","led_array":[{"id":7,"r":0,"g":255,"b":0}]}}
    */
    const char *pindefName = "seeed_xiao_esp32s3_geodome";
    const unsigned long BAUDRATE = 115200;

    uint8_t LED_PIN = GPIO_NUM_3; // D2
    const uint16_t LED_COUNT = 80;
    const uint8_t MATRIX_W = 10;  // LedController sizes the strip as MATRIX_W * MATRIX_H
    const uint8_t MATRIX_H = 8;
    const int16_t (*LED_GEOMETRY)[3] = GEODOME_LED_GEOMETRY;
    const uint16_t LED_GEOMETRY_COUNT = 80;

    int8_t I2C_SCL = disabled;
    int8_t I2C_SDA = disabled;
};

const seeed_xiao_esp32s3_geodome pinConfig;
