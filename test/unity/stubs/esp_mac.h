// Host stub: esp_mac.h
#pragma once
// T4.4 4c: dual include so C TUs (canopen_tpdo.c) get <stdint.h>; <cstdint> is
// a C++ STL header and #errors (STL1003) when included in C mode. Matches the
// pattern already used by the esp_err.h / esp_system.h stubs.
#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

typedef uint8_t esp_mac_type_t;
#define ESP_MAC_WIFI_STA 0
#define ESP_MAC_WIFI_SOFTAP 1
#define ESP_MAC_BT 2
#define ESP_MAC_ETH 3

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t esp_read_mac(uint8_t* mac_addr, esp_mac_type_t type);
esp_err_t esp_base_mac_addr_get(uint8_t* mac);
esp_err_t esp_get_mac(uint8_t* mac);

#ifdef __cplusplus
}
#endif
