#pragma once
// T4.1: C (was C++). namespace Config -> free functions (config_*).
// setup gains a pindef_name parameter (the original read the C++ global
// pinConfig.pindefName; a .c TU cannot touch a C++ object, so the C++ caller
// passes it). getPsxMac returns a pointer to a static buffer (was a heap
// char* that the caller never freed — a latent leak now removed).
#include <stdbool.h>

#ifdef __cplusplus
extern "C" {
#endif

// Initialize NVS + log the board/compile info.
void config_setup(const char *pindef_name);

// Clear the "UC2" preference namespace.
bool config_reset_prefs(void);

// PS4 controller pairing (stored in NVS namespace "UC2").
void config_psx_mac_set(const char *mac);
// Returns a pointer to a static buffer (was a heap char* the caller never freed).
char *config_psx_mac_get(void);
void config_psx_type_set(int type);
int config_psx_type_get(void);

#ifdef __cplusplus
}
#endif
