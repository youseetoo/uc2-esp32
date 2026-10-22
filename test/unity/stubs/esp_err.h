// Host stub: esp_err.h
#pragma once
#ifdef __cplusplus
#include <cstdint>
#else
#include <stdint.h>
#endif

typedef int esp_err_t;

#define ESP_OK 0
#define ESP_FAIL -1
#define ESP_ERR_NO_MEM 0x101
#define ESP_ERR_INVALID_ARG 0x102
#define ESP_ERR_INVALID_STATE 0x103
#define ESP_ERR_INVALID_SIZE 0x104
#define ESP_ERR_NOT_FOUND 0x105
#define ESP_ERR_NOT_SUPPORTED 0x106
#define ESP_ERR_TIMEOUT 0x107
#define ESP_ERR_INVALID_CRC 0x108
#define ESP_ERR_INVALID_RESPONSE 0x109
#define ESP_ERR_NOT_FINISHED 0x10a
#define ESP_ERR_NOT_ALLOWED 0x10b
#define ESP_ERR_INVALID_VERSION 0x10c
#define ESP_ERR_MAC_MISMATCH 0x10d
#define ESP_ERR_INVALID_FIELD 0x10e
#define ESP_ERR_INVALID_CHAR 0x10f
#define ESP_ERR_INVALID_MASK 0x110
#define ESP_ERR_INVALID_MARK 0x111
#define ESP_ERR_NOT_FINISHED 0x10a
#define ESP_ERR_NOT_ALLOWED 0x10b

#define ESP_ERROR_CHECK(x) (x)
#define ESP_ERROR_CHECK_HEX(x) (x)
#define ESP_ERROR_CHECK_TO_ERR(err, x, err_code) do { (err) = (x); } while (0)
#define ESP_ERROR_IS_ERR(x) ((x) != ESP_OK)
#define ESP_ERROR_IS_OK(x) ((x) == ESP_OK)

#ifdef __cplusplus
extern "C" {
#endif
const char *esp_err_to_name(esp_err_t code);
#ifdef __cplusplus
}
#endif
