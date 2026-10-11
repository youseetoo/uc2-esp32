#pragma once
// uc2::Wire — Arduino-compatible I2C façade over the IDF I2C v2 drivers.
//   master: i2c_new_master_bus + i2c_master_bus_add_device (per-address cache)
//   slave:  i2c_new_slave_device + on_request/on_receive callbacks (M5 Dial)
// Surface = exactly what main/ uses (grep-verified, see refs/arduino-idf-mapping.md):
//   begin(sda,scl[,freq]) / begin(addr,sda,scl,freq), beginTransmission(addr),
//   write(u8) / write(buf,len), endTransmission([stop]), requestFrom(addr,qty[,stop]),
//   available(), read(), readBytes(buf,len[,timeout]), onReceive(fn), onRequest(fn)
#include <cstddef>
#include <cstdint>

#include "driver/i2c_master.h"
#include "driver/i2c_slave.h"

namespace uc2 {

class Wire_ {
public:
    // Arduino endTransmission() status codes.
    static constexpr uint8_t ERR_SUCCESS   = 0;
    static constexpr uint8_t ERR_TOO_LONG  = 1;  // tx buffer overflowed (not produced by this impl)
    static constexpr uint8_t ERR_ADDR_NACK = 2;
    static constexpr uint8_t ERR_DATA_NACK = 3;
    static constexpr uint8_t ERR_OTHER     = 4;
    static constexpr uint8_t ERR_TIMEOUT   = 5;

    static constexpr int BUF_SIZE        = 256;  // Arduino default
    static constexpr int XFER_TIMEOUT_MS = 100;
    static constexpr int DEV_CACHE_SIZE  = 8;

    // Master on I2C_NUM_0. (3 args → master; 4 args → slave, no ambiguity.)
    bool begin(int sda, int scl, int freq_hz = 100000);
    // Slave (dial build).
    bool begin(uint8_t slave_addr, int sda, int scl, int freq_hz = 100000);
    void end();

    // ---- master transaction (Arduino tx/rx buffer semantics) ----
    void beginTransmission(uint8_t addr);
    size_t write(uint8_t b);
    size_t write(const uint8_t* buf, size_t len);
    uint8_t endTransmission(bool stop = true);  // 0 on success
    int requestFrom(uint8_t addr, int qty, bool stop = true);

    // Write then read within ONE transaction (repeated START). v2 cannot hold
    // the bus across separate endTransmission(false)/requestFrom() calls, so
    // code that relied on repeated start (I2cBridge) uses this instead.
    // Returns the number of bytes read (rdLen on success), or -1 on bus error
    // (errCode gets the endTransmission-style code).
    int writeRead(uint8_t addr, const uint8_t* wr, int wrLen, uint8_t* rd, int rdLen,
                  uint8_t* errCode = nullptr);  // bytes actually read
    int available();
    int read();  // -1 when empty
    int readBytes(uint8_t* buf, int len, uint32_t timeout_ms = 0);

    // ---- slave callbacks (run in ISR context, like the Arduino core) ----
    using ReceiveHandler = void (*)(int bytes);
    using RequestHandler = void (*)();
    void onReceive(ReceiveHandler fn);
    void onRequest(RequestHandler fn);

private:
    i2c_master_dev_handle_t devFor(uint8_t addr);

    static bool onSlaveReceive(i2c_slave_dev_handle_t dev,
                               const i2c_slave_rx_done_event_data_t* evt, void* arg);
    static bool onSlaveRequest(i2c_slave_dev_handle_t dev,
                               const i2c_slave_request_event_data_t* evt, void* arg);

    bool isSlave_ = false;

    i2c_master_bus_handle_t bus_ = nullptr;
    i2c_slave_dev_handle_t slave_ = nullptr;
    uint32_t freq_ = 100000;

    // per-address device handle cache (master)
    uint8_t devAddrs_[DEV_CACHE_SIZE] = {0};
    i2c_master_dev_handle_t devs_[DEV_CACHE_SIZE] = {nullptr};

    // Arduino-style tx/rx ring (one Wire instance = one buffer pair)
    uint8_t txBuf_[BUF_SIZE] = {0};
    size_t txCount_ = 0;
    uint8_t rxBuf_[BUF_SIZE] = {0};
    size_t rxCount_ = 0;
    size_t rxPos_ = 0;
    uint8_t xferAddr_ = 0;

    ReceiveHandler recvFn_ = nullptr;
    RequestHandler reqFn_ = nullptr;
};

}  // namespace uc2

// Global, like the Arduino core (main/ uses bare `Wire`).
extern uc2::Wire_ Wire;
