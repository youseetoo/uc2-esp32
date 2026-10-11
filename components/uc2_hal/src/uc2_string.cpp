// T1.3 (no Arduino): uc2::String implementation. Pure C++11 (heap + cstdio)
// so it also compiles in the host-side test suite (test/native/run.sh).
#include "uc2_hal/uc2_string.h"

const int DEC = 10;
const int HEX = 16;

#include <cctype>
#include <cstdio>
#include <cstdlib>
#include <cstring>

namespace uc2 {

namespace {
size_t strLen(const char *s)
{
    return s ? std::strlen(s) : 0;
}
}  // namespace

String::String(const char *s)
{
    copyFrom(s);
}

String::String(const String &other)
{
    copyFrom(other.c_str());
}

String::String(String &&other) noexcept : buf_(other.buf_), len_(other.len_)
{
    other.buf_ = nullptr;
    other.len_ = 0;
}

String &String::operator=(const String &other)
{
    if (this != &other) {
        copyFrom(other.c_str());
    }
    return *this;
}

String &String::operator=(String &&other) noexcept
{
    if (this != &other) {
        delete[] buf_;
        buf_ = other.buf_;
        len_ = other.len_;
        other.buf_ = nullptr;
        other.len_ = 0;
    }
    return *this;
}

String &String::operator=(const char *s)
{
    copyFrom(s);
    return *this;
}

String::~String()
{
    delete[] buf_;
}

void String::copyFrom(const char *s)
{
    const size_t n = strLen(s);
    char *p = new char[n + 1];
    std::memcpy(p, s ? s : "", n + 1);
    delete[] buf_;
    buf_ = p;
    len_ = n;
}

String::String(int value)
{
    char b[16];
    std::snprintf(b, sizeof b, "%d", value);
    copyFrom(b);
}

String::String(int value, int base)
{
    char b[16];
    if (base == 16) {
        std::snprintf(b, sizeof b, "%x", value);
    } else {
        std::snprintf(b, sizeof b, "%d", value);
    }
    copyFrom(b);
}

String::String(unsigned char value, int base)
{
    char b[16];
    if (base == 16) {
        std::snprintf(b, sizeof b, "%02x", value);  // Arduino pads bytes to 2 hex digits
    } else {
        std::snprintf(b, sizeof b, "%u", value);
    }
    copyFrom(b);
}

String String::operator+(const String &rhs) const
{
    String out;
    out.copyFrom(c_str());
    out += rhs.c_str();
    return out;
}

String String::operator+(const char *rhs) const
{
    String out = *this;
    out += rhs;
    return out;
}

String &String::operator+=(const char *s)
{
    const size_t n = strLen(s);
    char *p = new char[len_ + n + 1];
    std::memcpy(p, c_str(), len_ + 1);
    std::memcpy(p + len_ + 1, s ? s : "", n + 1);
    delete[] buf_;
    buf_ = p;
    len_ += n;
    return *this;
}

String &String::operator+=(char c)
{
    const char two[2] = {c, '\0'};
    return operator+=(two);
}

int String::toInt() const
{
    return buf_ ? static_cast<int>(std::strtol(buf_, nullptr, 10)) : 0;
}

void String::toLowerCase()
{
    for (size_t i = 0; i < len_; ++i) {
        buf_[i] = static_cast<char>(std::tolower(static_cast<unsigned char>(buf_[i])));
    }
}

}  // namespace uc2
