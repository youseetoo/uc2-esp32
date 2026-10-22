// Host-side stub of the Arduino-ESP32 API surface used by the uc2-esp32
// production sources under test. NOT a general Arduino implementation —
// only what main/src (serial/config/canopen/axis/led/laser/motor) compiles
// against. Keep additions minimal and compile-driven.
#pragma once

// MSVC's <algorithm> min/max function-like macros would break the
// production/test code's `std::min(a, b)` calls (C2589). Disable them;
// unqualified min()/max() (newlib globals on the ESP toolchain) are
// re-provided as functions below.
#ifndef NOMINMAX
#define NOMINMAX
#endif

// Pre-include the STL surface used by this stub and the code under test.
#include <algorithm>
#include <chrono>
#include <cmath>
#include <cstdarg>
#include <cstdint>
#include <cstddef>
#include <cstdio>
#include <cstdlib>
#include <cstring>
#include <map>
#include <string>
#include <vector>

// FreeRTOS types (TaskHandle_t, portMUX_TYPE, ...) — real Arduino-ESP32's
// Arduino.h pulls these in transitively; HomeMotor.h et al. use TaskHandle_t
// without including FreeRTOS.h directly.
#include "freertos/FreeRTOS.h"

// ── Types ───────────────────────────────────────────────────────────────────
typedef uint8_t byte;
typedef bool boolean;
// newlib/POSIX `uint` (unsigned int) used by some production headers
// (e.g. HomeMotor.h); not a C++ builtin on MSVC. Exact-token macro only, so
// uint8_t/uint16_t/... are unaffected.
#ifndef uint
#define uint unsigned int
#endif

// Arduino numeric bases (State.cpp uses HEX for MAC-derived SSIDs).
#define DEC 10
#define HEX 16
#define OCT 8
#define BIN 2

#define HIGH 1
#define LOW 0
#define INPUT 0
#define OUTPUT 1
#define INPUT_PULLUP 2
#define LSBFIRST 0
#define MSBFIRST 1

// GCC/Clang attribute pass-through (production headers use
// __attribute__((packed)) etc.; packing is irrelevant to the test surface).
#ifdef _MSC_VER
#ifndef __attribute__
#define __attribute__(x)
#endif
#endif

// ── Arduino core helpers ────────────────────────────────────────────────────
#define constrain(amt, low, high) \
    ((amt) < (low) ? (low) : ((amt) > (high) ? (high) : (amt)))
// NOTE: no min/max/abs macros — the real core defines only _min/_max and
// relies on newlib's global min()/max(); on MSVC we provide them as real
// functions (macros would break std::min/std::max in the code under test).
//
// The production code uses the C++ alternative token `and` (GCC accepts it
// unconditionally; MSVC only after <iso646.h>). Force it here; no `and`
// identifiers exist in the test surface (verified: CanOpenNode C files only
// use it in comments).
#define and &&
inline int min(int a, int b) { return (a < b) ? a : b; }
inline int max(int a, int b) { return (a > b) ? a : b; }
inline long min(long a, long b) { return (a < b) ? a : b; }
inline long max(long a, long b) { return (a > b) ? a : b; }
inline unsigned min(unsigned a, unsigned b) { return (a < b) ? a : b; }
inline unsigned max(unsigned a, unsigned b) { return (a > b) ? a : b; }
#define sq(x) ((x) * (x))

// POSIX string case-insensitive compare (newlib provides strcasecmp).
#ifdef _MSC_VER
#define strcasecmp(a, b) _stricmp(a, b)
#else
#include <strings.h>
#endif

// ── Logging (esp32-hal-log equivalent; routes through host_vprintf) ────────
void host_vprintf(const char* fmt, ...);
#define log_v(...) host_vprintf(__VA_ARGS__)
#define log_d(...) host_vprintf(__VA_ARGS__)
#define log_i(...) host_vprintf(__VA_ARGS__)
#define log_w(...) host_vprintf(__VA_ARGS__)
#define log_e(...) host_vprintf(__VA_ARGS__)

// ── Timing (mock_clock.h may #define millis() over this) ───────────────────
uint32_t millis();
uint32_t micros();
void delay(uint32_t ms);
void delayMicroseconds(uint32_t us);

// ── Pin I/O (no-op on host) ─────────────────────────────────────────────────
#include "driver/gpio.h"   // gpio_num_t / GPIO_NUM_* (esp32-hal-gpio in the real core)
#include "esp_system.h"    // ESP global object + ets_delay_us (real core pulls this in too)
void pinMode(uint8_t pin, uint8_t mode);
void digitalWrite(uint8_t pin, uint8_t val);
int digitalRead(uint8_t pin);
int analogRead(uint8_t pin);
void analogWrite(uint8_t pin, int val);
long map(long x, long in_min, long in_max, long out_min, long out_max);

// ── LEDC PWM (esp32-hal-ledc equivalent; no-op state on host) ──────────────
void ledcSetup(uint8_t timer, double freq, uint8_t duty_bit);
void ledcAttachPin(uint8_t pin, uint8_t timer);
void ledcDetachPin(uint8_t pin);
void ledcWrite(uint8_t pin, uint32_t duty);
uint32_t ledcRead(uint8_t pin);
// Arduino 2.0.x channel-based tone API (BuzzerController uses this).
void ledcWriteTone(uint8_t channel, uint32_t freq);

// ── Random (Arduino API) ────────────────────────────────────────────────────
uint32_t random(uint32_t max);
uint32_t random(uint32_t min, uint32_t max);
void randomSeed(uint32_t seed);

// ── String ──────────────────────────────────────────────────────────────────
// Convert a number to a string in an arbitrary base (Arduino's String(value, base)).
static void host_base_str(char* buf, size_t n, long v, unsigned base) {
    if (n == 0) return;
    if (base < 2) base = 10;
    if (base == 10) { snprintf(buf, n, "%ld", v); return; }
    char tmp[32]; int i = 31; tmp[31] = 0;
    if (v < 0) { tmp[i--] = '-'; v = -v; }
    do { int d = (int)(v % base); tmp[i--] = d < 10 ? (char)('0' + d) : (char)('A' + d - 10); v /= base; } while (v);
    size_t len = (size_t)(31 - i);
    if (len >= n) len = n - 1;
    memcpy(buf, tmp + i + 1, len);
    buf[len] = 0;
}

class String {
public:
    String() {}
    String(const char* s) : m_s(s ? s : "") {}
    String(const std::string& s) : m_s(s) {}
    String(char c) : m_s(1, c) {}
    String(int v) : m_s(std::to_string(v)) {}
    String(unsigned v) : m_s(std::to_string(v)) {}
    String(long v) : m_s(std::to_string(v)) {}
    String(unsigned long v) : m_s(std::to_string(v)) {}
    String(double v) : m_s(std::to_string(v)) {}
    // Two-arg numeric ctors (value, base) — Arduino API used by State.cpp
    // (e.g. String(macByte, HEX)). No default on base so they don't collide
    // with the single-arg numeric ctors above.
    String(int v, uint8_t base) { char buf[32]; host_base_str(buf, sizeof(buf), v, base); m_s = buf; }
    String(unsigned v, uint8_t base) { char buf[32]; host_base_str(buf, sizeof(buf), (long)v, base); m_s = buf; }

    const char* c_str() const { return m_s.c_str(); }
    unsigned length() const { return (unsigned)m_s.size(); }
    bool equals(const char* s) const { return m_s == (s ? s : ""); }
    bool equals(const String& s) const { return m_s == s.m_s; }
    bool startsWith(const String& p) const { return m_s.rfind(p.m_s, 0) == 0; }
    bool startsWith(const char* p) const { return m_s.rfind(p ? p : "", 0) == 0; }
    bool endsWith(const String& s) const {
        return m_s.size() >= s.m_s.size() &&
               m_s.compare(m_s.size() - s.m_s.size(), s.m_s.size(), s.m_s) == 0;
    }
    int indexOf(const char* sub, int from = 0) const {
        size_t p = m_s.find(sub ? sub : "", from);
        return p == std::string::npos ? -1 : (int)p;
    }
    String substring(unsigned begin, unsigned end = (unsigned)-1) const {
        if (end == (unsigned)-1) end = m_s.size();
        if (begin > m_s.size()) begin = m_s.size();
        if (end > m_s.size()) end = m_s.size();
        return String(m_s.substr(begin, end - begin));
    }
    String toLowerCase() const {
        std::string l = m_s;
        for (auto& c : l) c = (char)tolower((unsigned char)c);
        return String(l);
    }
    void toUpperCase() {
        for (auto& c : m_s) c = (char)toupper((unsigned char)c);
    }
    void trim() {
        size_t b = m_s.find_first_not_of(" \t\r\n");
        if (b == std::string::npos) { m_s.clear(); return; }
        size_t e = m_s.find_last_not_of(" \t\r\n");
        m_s = m_s.substr(b, e - b + 1);
    }
    String concat(const char* s) { m_s += s ? s : ""; return *this; }
    String concat(const String& s) { m_s += s.m_s; return *this; }
    String concat(char c) { m_s += c; return *this; }
    String concat(int v) { m_s += std::to_string(v); return *this; }
    String concat(unsigned v) { m_s += std::to_string(v); return *this; }
    String concat(long v) { m_s += std::to_string(v); return *this; }
    String concat(double v) { m_s += std::to_string(v); return *this; }
    String& operator+=(const char* s) { m_s += s ? s : ""; return *this; }
    String& operator+=(const String& s) { m_s += s.m_s; return *this; }
    String& operator+=(char c) { m_s += c; return *this; }
    String operator+(const char* rhs) const { return String(*this) += rhs; }
    String operator+(const String& rhs) const { return String(*this) += rhs; }
    String operator+(char rhs) const { return String(*this) += rhs; }
    String operator+(int rhs) const { return String(*this) += rhs; }
    String operator+(double rhs) const { return String(*this) += rhs; }
    bool operator==(const char* s) const { return equals(s); }
    bool operator==(const String& s) const { return equals(s); }
    bool operator!=(const char* s) const { return !equals(s); }
    bool operator!=(const String& s) const { return !equals(s); }
    char operator[](unsigned i) const { return m_s[i]; }
    char& operator[](unsigned i) { return m_s[i]; }
    int parseInt() const { return atoi(m_s.c_str()); }
    float parseFloat() const { return (float)atof(m_s.c_str()); }
    operator const char*() const { return m_s.c_str(); }

private:
    std::string m_s;
};
// Free operator+ for `const char* + String` (e.g. "joyDir" + String(axis)) —
// the member operator+ above cannot handle a C-string on the left.
inline String operator+(const char* lhs, const String& rhs) {
    String r(lhs ? lhs : "");
    r += rhs;
    return r;
}

// ── Print / Stream ──────────────────────────────────────────────────────────
class Print {
public:
    virtual ~Print() {}
    virtual size_t write(uint8_t c) { return 1; }
    virtual size_t write(const uint8_t* buffer, size_t size) {
        size_t n = 0;
        for (size_t i = 0; i < size; ++i) n += write(buffer[i]);
        return n;
    }
    size_t print(const char* s) { return s ? write((const uint8_t*)s, strlen(s)) : 0; }
    size_t print(const String& s) { return print(s.c_str()); }
    size_t print(char c) { return write((uint8_t)c); }
    size_t print(const char* s, char term) { size_t n = print(s); return n + write((uint8_t)term); }
    size_t print(const String& s, char term) { return print(s.c_str(), term); }
    size_t print(int v, uint8_t base = 10) { return print(long(v), base); }
    size_t print(long v, uint8_t base = 10) { char buf[32]; snprintf(buf, sizeof(buf), "%ld", v); (void)base; return print(buf); }
    size_t print(unsigned v, uint8_t base = 10) { char buf[32]; snprintf(buf, sizeof(buf), "%lu", (unsigned long)v); (void)base; return print(buf); }
    size_t print(long unsigned v, uint8_t base = 10) { char buf[32]; snprintf(buf, sizeof(buf), "%lu", v); (void)base; return print(buf); }
    size_t print(double v, uint8_t digits = 2) { char buf[32]; snprintf(buf, sizeof(buf), "%.*f", digits, v); return print(buf); }
    size_t printf(const char* fmt, ...) {
        char buf[256];
        va_list ap;
        va_start(ap, fmt);
        int n = vsnprintf(buf, sizeof(buf), fmt, ap);
        va_end(ap);
        if (n < 0) return 0;
        return print(n < (int)sizeof(buf) ? buf : buf);  // truncated tail dropped on host
    }
    size_t println() { return print("\r\n"); }
    size_t println(const char* s) { size_t n = print(s); return n + print("\r\n"); }
    size_t println(const String& s) { return println(s.c_str()); }
    size_t println(char c) { return print(c) + print("\r\n"); }
    size_t println(int v, uint8_t base = 10) { return print(v, base) + print("\r\n"); }
    size_t println(long v, uint8_t base = 10) { return print(v, base) + print("\r\n"); }
    size_t println(unsigned v, uint8_t base = 10) { return print(v, base) + print("\r\n"); }
    size_t println(double v, uint8_t digits = 2) { return print(v, digits) + print("\r\n"); }

protected:
    void printNumber(unsigned long v, uint8_t base);
    void printFloat(double v, uint8_t digits);
};

class Stream : public Print {
public:
    Stream() : m_timeout(1000) {}
    virtual ~Stream() {}
    // Non-pure so that HardwareSerial globals can be default-constructed on
    // the host; MockSerial overrides all four. (size_t to match the suite's
    // MockSerial override — production code only ever *calls* available().)
    virtual size_t available() { return 0; }
    virtual int read() { return -1; }
    virtual int peek() { return -1; }
    virtual void flush() {}

    int readBytes(char* buffer, size_t length, int timeout = 1000) {
        (void)timeout;
        size_t n = 0;
        while (n < length) {
            int c = read();
            if (c < 0) break;
            buffer[n++] = (char)c;
        }
        return (int)n;
    }
    String readString() {
        String r;
        int c;
        while ((c = read()) >= 0) r += (char)c;
        return r;
    }
    String readStringUntil(char term) {
        String r;
        int c;
        while ((c = read()) >= 0) {
            if (c == term) break;
            r += (char)c;
        }
        return r;
    }
    int parseBytes(char term, char* buffer, size_t len, int timeout = 1000) {
        (void)timeout;
        size_t n = 0;
        int c;
        while ((c = read()) >= 0 && n < len - 1) {
            if (c == term) break;
            buffer[n++] = (char)c;
        }
        buffer[n] = 0;
        return (int)n;
    }
    long parseNumber() {
        String r = readNumber();
        return atol(r.c_str());
    }
    float parseFloat() {
        String r = readFloat();
        return (float)atof(r.c_str());
    }
    String readNumber() {
        String r;
        int c;
        while ((c = peek()) >= 0 && (isdigit(c) || c == '-')) r += (char)read();
        return r;
    }
    String readFloat() {
        String r;
        int c;
        while ((c = peek()) >= 0 &&
               (isdigit(c) || c == '-' || c == '.' || c == 'e' || c == 'E' || c == '+'))
            r += (char)read();
        return r;
    }
    void setTimeout(unsigned long timeout) { m_timeout = timeout; }
    unsigned long getTimeout() const { return m_timeout; }

private:
    unsigned long m_timeout;
};

class HardwareSerial : public Stream {
public:
    HardwareSerial() {}
    void begin(unsigned long baud, uint32_t config = 0x0, int pinRx = -1, int pinTx = -1) {
        (void)baud; (void)config; (void)pinRx; (void)pinTx;
    }
    void end() {}
    using Stream::write;
};

extern HardwareSerial Serial;
extern HardwareSerial Serial1;
extern HardwareSerial Serial2;

// ── I2C (TwoWire) — used by peripheral headers where compiled ─────────────
class TwoWire {
public:
    void begin() {}
    void begin(uint8_t addr) { (void)addr; }
    void beginTransmission(uint8_t addr) { (void)addr; m_txLen = 0; }
    uint8_t endTransmission(bool stop = true) { (void)stop; return 0; }
    size_t write(uint8_t c) { (void)c; m_txLen++; return 1; }
    size_t write(const uint8_t* buf, size_t n) { m_txLen += n; return n; }
    uint8_t requestFrom(uint8_t addr, uint8_t n, uint8_t sendStop = 1) { (void)addr; (void)n; (void)sendStop; return 0; }
    int available() { return 0; }
    int read() { return -1; }
    int peek() { return -1; }
    void flush() {}
private:
    size_t m_txLen = 0;
};

extern TwoWire Wire;
extern TwoWire Wire1;
