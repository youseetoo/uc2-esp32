// Host stub: driver/temperature_sensor.h (IDF temperature_sensor component)
#pragma once
#ifdef __cplusplus
extern "C" {
#endif

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

typedef struct temperature_sensor_t *temperature_sensor_handle_t;

typedef struct {
    float low_range;
    float high_range;
    uint8_t resolution_bits;
    uint32_t frequency_hz;
    uint32_t noise_filter_level;
} temperature_sensor_config_t;

#define TEMPERATURE_SENSOR_CONFIG_DEFAULT(low, high)                       \
    {                                                                      \
        .low_range = (low),                                                \
        .high_range = (high),                                              \
        .resolution_bits = 11,                                             \
        .frequency_hz = 10000,                                             \
        .noise_filter_level = 5,                                           \
    }

esp_err_t temperature_sensor_install(const temperature_sensor_config_t *config,
                                     temperature_sensor_handle_t *handle);
esp_err_t temperature_sensor_enable(temperature_sensor_handle_t handle);
esp_err_t temperature_sensor_disable(temperature_sensor_handle_t handle);
esp_err_t temperature_sensor_get_celsius(temperature_sensor_handle_t handle,
                                         float *celsius);
esp_err_t temperature_sensor_deinit(temperature_sensor_handle_t handle);

#ifdef __cplusplus
}
#endif
