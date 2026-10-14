# T2.9: onewire_ds18b20 — vendored OneWire 2.3.8 + DallasTemperature 3.11.0 shimmed
# to raw IDF via uc2_hal. C++ leaf (portable as-is; C-ification deferred to T5.1).
#
# OneWire.cpp includes <Arduino.h> and uses pinMode/digitalWrite/digitalRead,
# delayMicroseconds, micros. DallasTemperature.cpp includes "Arduino.h" and uses
# pinMode/digitalWrite/millis/delay/yield. All are provided by uc2_hal.
idf_component_register(SRCS "src/OneWire.cpp" "src/DallasTemperature.cpp"
                       INCLUDE_DIRS "include"
                       REQUIRES uc2_hal)
# Suppress the OneWire fallback-mode #warning (treated as error by -Werror=cpp).
target_compile_options(__idf_onewire_ds18b20 PRIVATE -Wno-cpp)
