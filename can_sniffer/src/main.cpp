// Minimal CAN/CANopen sniffer for UC2 debugging.
// Prints every frame on the bus over Serial @115200, with CANopen decoding.
//
// Build: pio run -t upload && pio device monitor
// Pins/bitrate/mode are set via build_flags in platformio.ini.

#include <Arduino.h>
#include "driver/twai.h"

#ifndef CAN_TX_PIN
#define CAN_TX_PIN 17
#endif
#ifndef CAN_RX_PIN
#define CAN_RX_PIN 18
#endif
#ifndef CAN_BITRATE_KBPS
#define CAN_BITRATE_KBPS 500
#endif
#ifndef LISTEN_ONLY
#define LISTEN_ONLY 1
#endif

static const char* nmtState(uint8_t s) {
    switch (s) {
        case 0:   return "BOOT";
        case 4:   return "STOPPED";
        case 5:   return "OPERATIONAL";
        case 127: return "PRE-OP";
        default:  return "?";
    }
}

static void decodeCANopen(uint32_t id, uint8_t dlc, const uint8_t* d) {
    uint8_t node = id & 0x7F;
    uint32_t func = id & 0x780;

    switch (func) {
        case 0x000:
            Serial.printf("  NMT cmd cs=0x%02X target=%u\n",
                          dlc > 0 ? d[0] : 0, dlc > 1 ? d[1] : 0);
            return;
        case 0x080:
            if (id == 0x080) Serial.println("  SYNC");
            else             Serial.printf("  EMCY node=%u\n", node);
            return;
        case 0x180: Serial.printf("  TPDO1 node=%u\n", node); return;
        case 0x200: Serial.printf("  RPDO1 node=%u\n", node); return;
        case 0x280: Serial.printf("  TPDO2 node=%u\n", node); return;
        case 0x300: Serial.printf("  RPDO2 node=%u\n", node); return;
        case 0x380: Serial.printf("  TPDO3 node=%u\n", node); return;
        case 0x400: Serial.printf("  RPDO3 node=%u\n", node); return;
        case 0x480: Serial.printf("  TPDO4 node=%u\n", node); return;
        case 0x500: Serial.printf("  RPDO4 node=%u\n", node); return;
        case 0x580: {
            // SDO server -> client (response)
            uint8_t cs = dlc > 0 ? d[0] : 0;
            uint16_t idx = (dlc >= 3) ? (d[1] | (d[2] << 8)) : 0;
            uint8_t  sub = dlc >= 4 ? d[3] : 0;
            if ((cs & 0xE0) == 0x40)
                Serial.printf("  SDO_TX node=%u UPLOAD-RESP   0x%04X:%u\n", node, idx, sub);
            else if ((cs & 0xE0) == 0x60)
                Serial.printf("  SDO_TX node=%u DOWNLOAD-ACK 0x%04X:%u\n", node, idx, sub);
            else if (cs == 0x80) {
                uint32_t ab = dlc >= 8 ? (d[4] | (d[5]<<8) | (d[6]<<16) | ((uint32_t)d[7]<<24)) : 0;
                Serial.printf("  SDO_TX node=%u ABORT        0x%04X:%u code=0x%08X\n",
                              node, idx, sub, ab);
            } else
                Serial.printf("  SDO_TX node=%u cs=0x%02X 0x%04X:%u\n", node, cs, idx, sub);
            return;
        }
        case 0x600: {
            // SDO client -> server (request)
            uint8_t cs = dlc > 0 ? d[0] : 0;
            uint16_t idx = (dlc >= 3) ? (d[1] | (d[2] << 8)) : 0;
            uint8_t  sub = dlc >= 4 ? d[3] : 0;
            if ((cs & 0xE0) == 0x20)
                Serial.printf("  SDO_RX node=%u DOWNLOAD-REQ 0x%04X:%u\n", node, idx, sub);
            else if ((cs & 0xE0) == 0x40)
                Serial.printf("  SDO_RX node=%u UPLOAD-REQ   0x%04X:%u\n", node, idx, sub);
            else
                Serial.printf("  SDO_RX node=%u cs=0x%02X 0x%04X:%u\n", node, cs, idx, sub);
            return;
        }
        case 0x700:
            if (dlc >= 1)
                Serial.printf("  HEARTBEAT node=%u state=%s(0x%02X)\n",
                              node, nmtState(d[0]), d[0]);
            else
                Serial.printf("  HEARTBEAT node=%u (no data)\n", node);
            return;
        default:
            Serial.printf("  ?? cob=0x%03X node=%u\n", (unsigned)id, node);
    }
}

static void printAlerts(uint32_t a) {
    if (!a) return;
    Serial.print("[ALERT]");
    if (a & TWAI_ALERT_BUS_ERROR)       Serial.print(" BUS_ERROR");
    if (a & TWAI_ALERT_RX_QUEUE_FULL)   Serial.print(" RX_FULL");
    if (a & TWAI_ALERT_ERR_PASS)        Serial.print(" ERR_PASSIVE");
    if (a & TWAI_ALERT_BUS_OFF)         Serial.print(" BUS_OFF");
    if (a & TWAI_ALERT_BUS_RECOVERED)   Serial.print(" BUS_RECOVERED");
    if (a & TWAI_ALERT_ARB_LOST)        Serial.print(" ARB_LOST");
    if (a & TWAI_ALERT_RX_DATA)         {/* normal */}
    Serial.println();
}

void setup() {
    Serial.begin(115200);
    delay(500);
    Serial.println();
    Serial.println("==== UC2 CAN sniffer ====");
    Serial.printf("TX=%d RX=%d bitrate=%dk mode=%s\n",
                  CAN_TX_PIN, CAN_RX_PIN, CAN_BITRATE_KBPS,
                  LISTEN_ONLY ? "LISTEN_ONLY" : "NORMAL");

    twai_general_config_t g = TWAI_GENERAL_CONFIG_DEFAULT(
        (gpio_num_t)CAN_TX_PIN, (gpio_num_t)CAN_RX_PIN,
#if LISTEN_ONLY
        TWAI_MODE_LISTEN_ONLY
#else
        TWAI_MODE_NORMAL
#endif
    );
    g.rx_queue_len = 64;
    g.alerts_enabled = TWAI_ALERT_ALL;

    twai_timing_config_t t;
    switch (CAN_BITRATE_KBPS) {
        case 1000: t = TWAI_TIMING_CONFIG_1MBITS();   break;
        case 800:  t = TWAI_TIMING_CONFIG_800KBITS(); break;
        case 500:  t = TWAI_TIMING_CONFIG_500KBITS(); break;
        case 250:  t = TWAI_TIMING_CONFIG_250KBITS(); break;
        case 125:  t = TWAI_TIMING_CONFIG_125KBITS(); break;
        case 100:  t = TWAI_TIMING_CONFIG_100KBITS(); break;
        default:   t = TWAI_TIMING_CONFIG_500KBITS(); break;
    }
    twai_filter_config_t f = TWAI_FILTER_CONFIG_ACCEPT_ALL();

    if (twai_driver_install(&g, &t, &f) != ESP_OK) {
        Serial.println("twai_driver_install FAILED");
        while (1) delay(1000);
    }
    if (twai_start() != ESP_OK) {
        Serial.println("twai_start FAILED");
        while (1) delay(1000);
    }
    Serial.println("Sniffing...");
}

void loop() {
    uint32_t alerts = 0;
    if (twai_read_alerts(&alerts, pdMS_TO_TICKS(50)) == ESP_OK)
        printAlerts(alerts);

    twai_message_t m;
    while (twai_receive(&m, 0) == ESP_OK) {
        uint32_t t = millis();
        Serial.printf("[%8u] ID=0x%03X %s%s DL=%u",
                      (unsigned)t, (unsigned)m.identifier,
                      (m.flags & TWAI_MSG_FLAG_EXTD) ? "X" : "S",
                      (m.flags & TWAI_MSG_FLAG_RTR)  ? "R" : " ",
                      (unsigned)m.data_length_code);
        Serial.print(" data=");
        for (int i = 0; i < m.data_length_code; i++)
            Serial.printf("%02X ", m.data[i]);
        Serial.println();
        if (!(m.flags & TWAI_MSG_FLAG_EXTD) && !(m.flags & TWAI_MSG_FLAG_RTR))
            decodeCANopen(m.identifier, m.data_length_code, m.data);
    }

    static uint32_t lastStatus = 0;
    if (millis() - lastStatus > 5000) {
        lastStatus = millis();
        twai_status_info_t st;
        if (twai_get_status_info(&st) == ESP_OK) {
            Serial.printf("[STATUS] state=%d msgs_rx=%u rx_err=%u tx_err=%u "
                          "rx_missed=%u arb_lost=%u bus_err=%u\n",
                          st.state, st.msgs_to_rx, st.rx_error_counter,
                          st.tx_error_counter, st.rx_missed_count,
                          st.arb_lost_count, st.bus_error_count);
        }
    }
}
