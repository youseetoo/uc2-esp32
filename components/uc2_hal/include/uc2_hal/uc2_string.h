// T1.3 (no Arduino): uc2::String — minimal Arduino String replacement over a
// heap buffer. API surface = exactly what main/ uses (grep-verified
// 2026-10-11, ~70 lines / 13 files):
//   ctors (), (const char*), (int), (int, base), (unsigned char, base)
//   operator+ (String and const char*, both sides), operator+=
//   c_str(), length(), toInt(), toLowerCase() (in-place)
// Dominant usage pattern: NVS keys  ("motor" + String(axis)).c_str()
#pragma once

#include <cstddef>

namespace uc2 {

class String
{
public:
    String() = default;
    String(const char *s);
    String(const String &other);
    String(String &&other) noexcept;
    String &operator=(const String &other);
    String &operator=(String &&other) noexcept;
    String &operator=(const char *s);
    ~String();

    String(int value);                  // decimal
    String(int value, int base);        // base 10 or 16 (%x, lowercase, no pad)
    String(unsigned char value, int base);  // Arduino: HEX pads a byte to 2 digits

    String operator+(const String &rhs) const;
    String operator+(const char *rhs) const;
    String &operator+=(const char *s);
    String &operator+=(char c);

    const char *c_str() const { return buf_ ? buf_ : ""; }
    size_t length() const { return len_; }
    int toInt() const;
    void toLowerCase();  // in-place, like Arduino String::toLowerCase

private:
    char *buf_ = nullptr;  // null-terminated; owned
    size_t len_ = 0;

    void copyFrom(const char *s);
};

inline String operator+(const char *lhs, const String &rhs) { return rhs + lhs; }

}  // namespace uc2

// Arduino-compatible format bases (global, like the Arduino core).
// extern const (not inline constexpr) so the header stays C++11-clean.
extern const int DEC;
extern const int HEX;
