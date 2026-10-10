# Minimal Arduino.h shim so already-C components that #include <Arduino.h>
# (canopen_node) build under pure ESP-IDF. The full Arduino->IDF mapping is
# uc2_hal (T1.3); this only satisfies the include + common timing helpers.
idf_component_register(INCLUDE_DIRS "." REQUIRES log)
