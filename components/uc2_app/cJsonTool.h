// T4.1: cJsonTool — C-compatible JSON helpers (Phase 4 C conversion).
// The six helpers are C `static inline` functions (per-TU, no link dependency)
// so the C-converted modules (pid / axis / signal / state) can call them
// directly. A thin C++ namespace shim (bottom) keeps every existing
// `cJsonTool::getJsonInt(...)` call site compiling unchanged for the C++
// modules not yet converted.
//
// Semantics are unchanged from the pre-C-conversion namespace version:
//   * getJsonInt / getJsonBool return `defaultVal` on null doc / missing / type mismatch
//   * getJsonFloat returns 0.0 on missing / type mismatch (null doc safe: cJSON
//     GetObjectItemCaseSensitive(NULL, ...) is NULL)
//   * set* create a new cJSON node and add it under `key` (no dedup, as before)
#pragma once
#include <stdbool.h>
#include "cJSON.h"

// ── C helpers (static inline, per-TU — no link dependency) ─────────────────
static inline int cjson_tool_get_int(cJSON *job, const char *key, int defaultVal)
{
    if (job == NULL) return defaultVal;
    cJSON *val = cJSON_GetObjectItemCaseSensitive(job, key);
    if (val == NULL) return defaultVal;
    if (cJSON_IsNumber(val)) return cJSON_GetNumberValue(val);
    return defaultVal;
}

static inline void cjson_tool_set_int(cJSON *job, const char *key, int valToSet)
{
    cJSON *v = cJSON_CreateNumber(valToSet);
    cJSON_AddItemToObject(job, key, v);
}

static inline void cjson_tool_set_float(cJSON *job, const char *key, float valToSet)
{
    cJSON *v = cJSON_CreateNumber(valToSet);
    cJSON_AddItemToObject(job, key, v);
}

static inline float cjson_tool_get_float(cJSON *job, const char *key)
{
    cJSON *val = cJSON_GetObjectItemCaseSensitive(job, key);
    if (cJSON_IsNumber(val)) return val->valuedouble;
    return 0.0f;
}

static inline bool cjson_tool_get_bool(cJSON *job, const char *key, bool defaultVal)
{
    if (job == NULL) return defaultVal;
    cJSON *val = cJSON_GetObjectItemCaseSensitive(job, key);
    if (val == NULL) return defaultVal;
    if (cJSON_IsBool(val)) return cJSON_IsTrue(val);
    // Also handle integer values (0 = false, non-zero = true)
    if (cJSON_IsNumber(val)) return cJSON_GetNumberValue(val) != 0;
    return defaultVal;
}

static inline void cjson_tool_set_bool(cJSON *job, const char *key, bool valToSet)
{
    cJSON *v = cJSON_CreateBool(valToSet);
    cJSON_AddItemToObject(job, key, v);
}

#ifdef __cplusplus
// ── C++ shim (keeps `cJsonTool::...` call sites compiling) ─────────────────
namespace cJsonTool
{
    inline int getJsonInt(cJSON *job, const char *key, int defaultVal = 0)
    { return cjson_tool_get_int(job, key, defaultVal); }
    inline void setJsonInt(cJSON *job, const char *key, int valToSet)
    { cjson_tool_set_int(job, key, valToSet); }
    inline void setJsonFloat(cJSON *job, const char *key, float valToSet)
    { cjson_tool_set_float(job, key, valToSet); }
    inline float getJsonFloat(cJSON *job, const char *key)
    { return cjson_tool_get_float(job, key); }
    inline bool getJsonBool(cJSON *job, const char *key, bool defaultVal = false)
    { return cjson_tool_get_bool(job, key, defaultVal); }
    inline void setJsonBool(cJSON *job, const char *key, bool valToSet)
    { cjson_tool_set_bool(job, key, valToSet); }
}
#endif
