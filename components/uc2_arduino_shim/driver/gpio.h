/* Shim: the pre-6.1 "driver/gpio.h" header (removed in IDF 6.1; GPIO moved to
 * esp_hal_gpio). canopen_node includes it but uses no gpio_* symbols, so an
 * empty header satisfies the include under pure ESP-IDF. */
#pragma once
