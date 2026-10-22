// Host stub: hal/twai_types.h — base TWAI types (IDF 6.1 flat message layout;
// CO_driver_ESP32.c and CANopenModule.cpp both use it on the real target).
#pragma once
#include <stdbool.h>
#include <stdint.h>

#define TWAI_MAX_DATA_LEN 8

typedef enum {
    TWAI_DATA_LENGTH_0 = 0,
    TWAI_DATA_LENGTH_1 = 1,
    TWAI_DATA_LENGTH_2 = 2,
    TWAI_DATA_LENGTH_3 = 3,
    TWAI_DATA_LENGTH_4 = 4,
    TWAI_DATA_LENGTH_5 = 5,
    TWAI_DATA_LENGTH_6 = 6,
    TWAI_DATA_LENGTH_7 = 7,
    TWAI_DATA_LENGTH_8 = 8
} twai_data_length_code_t;

// Flat message struct: identifier + bit-fields + DLC + data[8] + flags.
typedef struct {
    uint32_t identifier;
    bool extd;
    bool rtr;
    bool ss;
    bool self;
    bool dlc_non_comp;
    uint8_t data_length_code;
    uint8_t data[TWAI_MAX_DATA_LEN];
    uint32_t flags;
} twai_message_t;

// Message flag bits (CANopenModule.cpp checks TWAI_MSG_FLAG_EXTD).
#define TWAI_MSG_FLAG_EXTD     (1u << 0)
#define TWAI_MSG_FLAG_RTR      (1u << 1)
#define TWAI_MSG_FLAG_SS       (1u << 2)
#define TWAI_MSG_FLAG_SELF     (1u << 3)
#define TWAI_MSG_FLAG_DLC_NC   (1u << 4)
