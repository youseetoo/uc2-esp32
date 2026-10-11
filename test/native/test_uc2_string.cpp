// Host-side tests for uc2::String (T1.3 uc2_hal). Pure C++, no IDF deps.
#include <cassert>
#include <cstdio>
#include <cstring>
#include <utility>
#include "uc2_hal/uc2_string.h"

static int g_fail = 0;
#define CHECK(expr)                                                    \
    do {                                                               \
        if (!(expr)) {                                                 \
            std::printf("FAIL %s:%d: %s\n", __FILE__, __LINE__, #expr); \
            ++g_fail;                                                  \
        }                                                              \
    } while (0)

int main()
{
    // ctors
    uc2::String a;
    CHECK(a.c_str()[0] == '\0');
    CHECK(a.length() == 0);

    uc2::String b("hello");
    CHECK(std::strcmp(b.c_str(), "hello") == 0);
    CHECK(b.length() == 5);

    uc2::String c = b;  // copy
    CHECK(c.length() == 5);
    uc2::String d(std::move(b));  // move
    CHECK(d.length() == 5);

    uc2::String n(42);
    CHECK(std::strcmp(n.c_str(), "42") == 0);
    uc2::String n2(-7);
    CHECK(std::strcmp(n2.c_str(), "-7") == 0);

    // bases
    uc2::String h1(255, HEX);
    CHECK(std::strcmp(h1.c_str(), "ff") == 0);
    uc2::String h2(static_cast<unsigned char>(0x05), HEX);  // Arduino pads a byte to 2 hex digits
    CHECK(std::strcmp(h2.c_str(), "05") == 0);
    uc2::String d10(255, DEC);
    CHECK(std::strcmp(d10.c_str(), "255") == 0);

    // concatenation — the dominant NVS-key pattern
    uc2::String key = "motor" + uc2::String(3);
    CHECK(std::strcmp(key.c_str(), "motor3") == 0);
    uc2::String key2 = uc2::String("a") + "b";
    CHECK(std::strcmp(key2.c_str(), "ab") == 0);
    uc2::String key3 = uc2::String("x") + uc2::String("y");
    CHECK(std::strcmp(key3.c_str(), "xy") == 0);
    key3 += "z";
    CHECK(std::strcmp(key3.c_str(), "xyz") == 0);
    key3 += '!';
    CHECK(std::strcmp(key3.c_str(), "xyz!") == 0);

    // toInt / toLowerCase
    CHECK(uc2::String("123").toInt() == 123);
    CHECK(uc2::String("-99").toInt() == -99);
    CHECK(uc2::String("12abc").toInt() == 12);
    uc2::String up = "MiXeD";
    up.toLowerCase();
    CHECK(std::strcmp(up.c_str(), "mixed") == 0);

    // assignment
    b = "reassigned";
    CHECK(std::strcmp(b.c_str(), "reassigned") == 0);
    b = uc2::String(9);
    CHECK(b.toInt() == 9);

    if (g_fail == 0) std::printf("test_uc2_string: all checks passed\n");
    return g_fail ? 1 : 0;
}
