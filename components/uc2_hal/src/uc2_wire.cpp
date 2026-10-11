// uc2::Wire — implementation over IDF I2C v2 (master bus + slave device).
#include "uc2_hal/uc2_wire.h"

#include <cstring>

#include "esp_err.h"
#include "esp_log.h"

static const char* TAG = "uc2_wire";

uc2::Wire_ Wire;  // global, like the Arduino core

namespace uc2 {

bool Wire_::begin(int sda, int scl, int freq_hz)
{
    if (bus_) {
        return true;  // already up
    }
    i2c_master_bus_config_t cfg = {};
    cfg.i2c_port = I2C_NUM_0;
    cfg.sda_io_num = static_cast<gpio_num_t>(sda);
    cfg.scl_io_num = static_cast<gpio_num_t>(scl);
    cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    cfg.glitch_ignore_cnt = 7;
    cfg.intr_priority = 0;
    cfg.trans_queue_depth = 0;  // blocking (synchronous) transactions
    cfg.flags.enable_internal_pullup = 1;
    freq_ = static_cast<uint32_t>(freq_hz);
    isSlave_ = false;
    esp_err_t err = i2c_new_master_bus(&cfg, &bus_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_master_bus failed: %s", esp_err_to_name(err));
        return false;
    }
    ESP_LOGI(TAG, "I2C master on sda=%d scl=%d @ %u Hz", sda, scl, (unsigned)freq_);
    return true;
}

bool Wire_::begin(uint8_t slave_addr, int sda, int scl, int freq_hz)
{
    if (slave_) {
        return true;  // already up
    }
    i2c_slave_config_t cfg = {};
    cfg.i2c_port = I2C_NUM_0;
    cfg.sda_io_num = static_cast<gpio_num_t>(sda);
    cfg.scl_io_num = static_cast<gpio_num_t>(scl);
    cfg.clk_source = I2C_CLK_SRC_DEFAULT;
    cfg.send_buf_depth = BUF_SIZE;
    cfg.receive_buf_depth = BUF_SIZE;
    cfg.slave_addr = slave_addr;
    cfg.addr_bit_len = I2C_ADDR_BIT_LEN_7;
    cfg.intr_priority = 0;
    cfg.flags.enable_internal_pullup = 1;
    isSlave_ = true;
    esp_err_t err = i2c_new_slave_device(&cfg, &slave_);
    if (err != ESP_OK) {
        ESP_LOGE(TAG, "i2c_new_slave_device failed: %s", esp_err_to_name(err));
        return false;
    }
    i2c_slave_event_callbacks_t cbs = {};
    cbs.on_receive = onSlaveReceive;
#if SOC_I2C_SLAVE_CAN_GET_STRETCH_CAUSE
    cbs.on_request = onSlaveRequest;
#endif
    i2c_slave_register_event_callbacks(slave_, &cbs, this);
    ESP_LOGI(TAG, "I2C slave addr=0x%02X on sda=%d scl=%d @ %u Hz", slave_addr, sda, scl,
          (unsigned)freq_hz);
    return true;
}

void Wire_::end()
{
    if (bus_) {
        i2c_del_master_bus(bus_);
        bus_ = nullptr;
        for (int i = 0; i < DEV_CACHE_SIZE; i++) {
            devAddrs_[i] = 0;
            devs_[i] = nullptr;
        }
    }
    if (slave_) {
        i2c_del_slave_device(slave_);
        slave_ = nullptr;
    }
    txCount_ = 0;
    rxCount_ = 0;
    rxPos_ = 0;
}

i2c_master_dev_handle_t Wire_::devFor(uint8_t addr)
{
    for (int i = 0; i < DEV_CACHE_SIZE; i++) {
        if (devAddrs_[i] == addr) {
            return devs_[i];
        }
    }
    if (!bus_) {
        return nullptr;
    }
    i2c_device_config_t dc = {};
    dc.dev_addr_length = I2C_ADDR_BIT_LEN_7;
    dc.device_address = addr;
    dc.scl_speed_hz = freq_;
    for (int i = 0; i < DEV_CACHE_SIZE; i++) {
        if (!devs_[i]) {
            if (i2c_master_bus_add_device(bus_, &dc, &devs_[i]) == ESP_OK) {
                devAddrs_[i] = addr;
                return devs_[i];
            }
            return nullptr;
        }
    }
    return nullptr;  // cache full
}

void Wire_::beginTransmission(uint8_t addr)
{
    xferAddr_ = addr;
    txCount_ = 0;
}

size_t Wire_::write(uint8_t b)
{
    if (txCount_ < BUF_SIZE) {
        txBuf_[txCount_++] = b;
    }
    return 1;  // Arduino: reports 1 even when the buffer is full
}

size_t Wire_::write(const uint8_t* buf, size_t len)
{
    size_t space = BUF_SIZE - txCount_;
    size_t n = len < space ? len : space;
    if (n > 0) {
        std::memcpy(txBuf_ + txCount_, buf, n);
        txCount_ += n;
    }
    return len;
}

uint8_t Wire_::endTransmission(bool /*stop*/)
{
    // The v2 driver always issues a STOP; stop=false (repeated start) is never
    // used in main/ (grep-verified), so it is not modelled.
    if (!bus_) {
        return ERR_OTHER;
    }
    if (txCount_ == 0) {
        return ERR_SUCCESS;  // address-only transaction, nothing to send
    }
    i2c_master_dev_handle_t d = devFor(xferAddr_);
    if (!d) {
        return ERR_OTHER;
    }
    esp_err_t err = i2c_master_transmit(d, txBuf_, txCount_, XFER_TIMEOUT_MS);
    txCount_ = 0;
    if (err == ESP_OK) {
        return ERR_SUCCESS;
    }
    if (err == ESP_ERR_TIMEOUT) {
        return ERR_TIMEOUT;
    }
    if (err == ESP_ERR_INVALID_RESPONSE) {
        // v2 does not distinguish addr NACK from data NACK
        return ERR_DATA_NACK;
    }
    return ERR_OTHER;
}

int Wire_::requestFrom(uint8_t addr, int qty, bool /*stop*/)
{
    if (!bus_ || qty <= 0) {
        return 0;
    }
    i2c_master_dev_handle_t d = devFor(addr);
    if (!d) {
        return 0;
    }
    int n = qty < BUF_SIZE ? qty : BUF_SIZE;
    uint8_t tmp[BUF_SIZE];
    if (i2c_master_receive(d, tmp, n, XFER_TIMEOUT_MS) != ESP_OK) {
        return 0;
    }
    std::memcpy(rxBuf_, tmp, n);
    rxCount_ = n;
    rxPos_ = 0;
    return n;
}

int Wire_::writeRead(uint8_t addr, const uint8_t* wr, int wrLen, uint8_t* rd, int rdLen,
                     uint8_t* errCode)
{
    if (errCode) {
        *errCode = 0;
    }
    if (!bus_) {
        if (errCode) {
            *errCode = ERR_OTHER;
        }
        return -1;
    }
    i2c_master_dev_handle_t d = devFor(addr);
    if (!d) {
        if (errCode) {
            *errCode = ERR_OTHER;
        }
        return -1;
    }
    esp_err_t err = i2c_master_transmit_receive(d, wr, wrLen, rd, rdLen, XFER_TIMEOUT_MS);
    if (err != ESP_OK) {
        if (errCode) {
            *errCode = (err == ESP_ERR_TIMEOUT) ? ERR_TIMEOUT
                                                : (err == ESP_ERR_INVALID_RESPONSE) ? ERR_DATA_NACK
                                                                                    : ERR_OTHER;
        }
        return -1;
    }
    return rdLen;  // v2 either reads exactly rdLen bytes or fails
}

int Wire_::available()
{
    return static_cast<int>(rxCount_ - rxPos_);
}

int Wire_::read()
{
    if (rxPos_ >= rxCount_) {
        return -1;
    }
    return static_cast<int8_t>(rxBuf_[rxPos_++]);
}

int Wire_::readBytes(uint8_t* buf, int len, uint32_t /*timeout_ms*/)
{
    // main/ always calls this with data already present (after requestFrom /
    // onReceive), so no waiting is implemented.
    int n = static_cast<int>(rxCount_ - rxPos_);
    if (n > len) {
        n = len;
    }
    if (n <= 0) {
        return 0;
    }
    std::memcpy(buf, rxBuf_ + rxPos_, n);
    rxPos_ += n;
    if (rxPos_ == rxCount_) {
        rxPos_ = 0;
        rxCount_ = 0;
    }
    return n;
}

void Wire_::onReceive(ReceiveHandler fn)
{
    recvFn_ = fn;
}

void Wire_::onRequest(RequestHandler fn)
{
    reqFn_ = fn;
}

bool Wire_::onSlaveReceive(i2c_slave_dev_handle_t, const i2c_slave_rx_done_event_data_t* evt,
                           void* arg)
{
    Wire_* w = static_cast<Wire_*>(arg);
    uint32_t n = evt->length;
    if (n > BUF_SIZE) {
        n = BUF_SIZE;
    }
    std::memcpy(w->rxBuf_, evt->buffer, n);
    w->rxCount_ = n;
    w->rxPos_ = 0;
    if (w->recvFn_) {
        w->recvFn_(static_cast<int>(n));  // Arduino: onReceive(numBytes)
    }
    return false;
}

bool Wire_::onSlaveRequest(i2c_slave_dev_handle_t dev, const i2c_slave_request_event_data_t*,
                           void* arg)
{
    Wire_* w = static_cast<Wire_*>(arg);
    w->txCount_ = 0;
    if (w->reqFn_) {
        w->reqFn_();  // fills the buffer via Wire.write()
    }
    uint32_t written = 0;
    if (w->txCount_ > 0) {
        i2c_slave_write(dev, w->txBuf_, w->txCount_, &written, 5);
    }
    return false;
}

}  // namespace uc2
