// Host stub: strings.h — this MSVC Build Tools installation does not ship the
// standard <strings.h>; the uc2 components include it for strcasecmp().
// Map to the MSVC equivalents. (On ESP-IDF the real newlib <strings.h> is
// used — this directory is on the host test include path only.)
#pragma once
#include <string.h>

#ifdef _MSC_VER
#define strcasecmp _stricmp
#define strncasecmp _strnicmp
#endif
