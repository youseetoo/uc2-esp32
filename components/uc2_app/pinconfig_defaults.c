// T4.1: PinConfig — safe all-disabled default table (Phase 4 C conversion).
//
// The field list lives in pinconfig_defaults.h (PINCONFIG_DEFAULTS_FIELDS) so
// each app's main/pinconfig.c can start from the same defaults and override
// only its board's fields in a single const initializer (C forbids function
// calls in file-scope initializers). This function form is for C callers that
// want to copy/patch the table at runtime.
//
// Pure C; compiles into libuc2_app.
#include "pinconfig_defaults.h"

struct PinConfig pinconfig_defaults(void)
{
    const struct PinConfig cfg = { PINCONFIG_DEFAULTS_FIELDS };
    return cfg;
}
