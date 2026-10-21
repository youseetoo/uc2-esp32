// DS18b20 temperature sensor (T4.1: pure-C API; implementation is a C++
// island over the vendored OneWire + DallasTemperature libs, onewire_ds18b20).
#pragma once

#include "cJSON.h"

#ifdef __cplusplus
extern "C" {
#endif

float ds18b20_read_temperature(void);

void ds18b20_setup(void);
int ds18b20_act(cJSON *json_document);
cJSON *ds18b20_get(cJSON *json_document);
float ds18b20_get_current_value_celcius(void);

#ifdef __cplusplus
}
#endif
