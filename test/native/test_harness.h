#pragma once
// Minimal host test harness shared by the native tests (no framework needed).
#include <cstdio>

static int g_failures = 0;
#define CHECK(cond) do { if (!(cond)) { std::printf("FAIL %s:%d  %s\n", __FILE__, __LINE__, #cond); ++g_failures; } } while (0)
#define FINISH(name) do { if (g_failures) { std::printf("%s: %d check(s) failed\n", name, g_failures); return 1; } \
                          std::printf("%s: all checks passed\n", name); return 0; } while (0)
