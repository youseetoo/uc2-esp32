#include "ESP32-TWAI-CAN.hpp"


void TwaiCAN::setSpeed(TwaiSpeed twaiSpeed) {
    if(twaiSpeed < TWAI_SPEED_SIZE) speed = twaiSpeed;
}

uint32_t TwaiCAN::getSpeedNumeric() { 
    uint32_t actualSpeed = 500;
	switch(getSpeed()) {
        default: break;
        #if (SOC_TWAI_BRP_MAX > 256)
        case TWAI_SPEED_1KBPS   :   actualSpeed = 1   ; break;
        case TWAI_SPEED_5KBPS   :   actualSpeed = 5   ; break;
        case TWAI_SPEED_10KBPS  :   actualSpeed = 10  ; break;
        #endif
        #if (SOC_TWAI_BRP_MAX > 128) || (CONFIG_ESP32_REV_MIN_FULL >= 200)
        case TWAI_SPEED_12_5KBPS:   actualSpeed = 12  ; break;
        case TWAI_SPEED_16KBPS  :   actualSpeed = 16  ; break;
        case TWAI_SPEED_20KBPS  :   actualSpeed = 20  ; break;
        #endif
		case TWAI_SPEED_50KBPS  :   actualSpeed = 50  ; break;
		case TWAI_SPEED_100KBPS :   actualSpeed = 100 ; break;
		case TWAI_SPEED_125KBPS :   actualSpeed = 125 ; break;
		case TWAI_SPEED_250KBPS :   actualSpeed = 250 ; break;
		case TWAI_SPEED_500KBPS :   actualSpeed = 500 ; break;
		case TWAI_SPEED_800KBPS :   actualSpeed = 800 ; break;
		case TWAI_SPEED_1000KBPS:   actualSpeed = 1000; break;
	}
    return actualSpeed;
}

TwaiSpeed TwaiCAN::convertSpeed(uint16_t canSpeed) { 
    TwaiSpeed actualSpeed = getSpeed();
	switch(canSpeed) {
        default: break;
        #if (SOC_TWAI_BRP_MAX > 256)
        case 1:     actualSpeed = TWAI_SPEED_1KBPS;     break;
        case 5:     actualSpeed = TWAI_SPEED_5KBPS;     break;
        case 10:    actualSpeed = TWAI_SPEED_10KBPS;    break;
        #endif
        #if (SOC_TWAI_BRP_MAX > 128) || (CONFIG_ESP32_REV_MIN_FULL >= 200)
        case 13:    // Just to handle those who would round upwards..
        case 12:    actualSpeed = TWAI_SPEED_12_5KBPS;  break;
        case 16:    actualSpeed = TWAI_SPEED_16KBPS;    break;
        case 20:    actualSpeed = TWAI_SPEED_20KBPS;    break;
        #endif
		case 50:    actualSpeed = TWAI_SPEED_50KBPS;   break;
		case 100:   actualSpeed = TWAI_SPEED_100KBPS;   break;
		case 125:   actualSpeed = TWAI_SPEED_125KBPS;   break;
		case 250:   actualSpeed = TWAI_SPEED_250KBPS;   break;
		case 500:   actualSpeed = TWAI_SPEED_500KBPS;   break;
		case 800:   actualSpeed = TWAI_SPEED_800KBPS;   break;
		case 1000:  actualSpeed = TWAI_SPEED_1000KBPS;  break;
	}
    return actualSpeed;
}

void TwaiCAN::setTxQueueSize(uint16_t txQueue) { if(txQueue != 0xFFFF) txQueueSize = txQueue; }
void TwaiCAN::setRxQueueSize(uint16_t rxQueue) { if(rxQueue != 0xFFFF) rxQueueSize = rxQueue; }

bool TwaiCAN::getStatusInfo() {
    return ESP_OK == twai_get_status_info(&status);
}

uint32_t TwaiCAN::inTxQueue() {
    uint32_t ret = 0;
    if(getStatusInfo()) {
        ret = status.msgs_to_tx;
    }
    return ret;
};

uint32_t TwaiCAN::inRxQueue() {
    uint32_t ret = 0;
    if(getStatusInfo()) {
        ret = status.msgs_to_rx;
    }
    return ret;
};

uint32_t TwaiCAN::rxErrorCounter()
{
    uint32_t ret = 0;
    if(getStatusInfo()) {
        ret = status.rx_error_counter;
    }
    return ret;
};

uint32_t TwaiCAN::txErrorCounter()
{
    uint32_t ret = 0;
    if(getStatusInfo()) {
        ret = status.tx_error_counter;
    }
    return ret;
};

uint32_t TwaiCAN::rxMissedCounter()
{
    uint32_t ret = 0;
    if(getStatusInfo()) {
        ret = status.rx_missed_count;
    }
    return ret;
};

uint32_t TwaiCAN::txFailedCounter()
{
    uint32_t ret = 0;
    if(getStatusInfo()) {
        ret = status.tx_failed_count;
    }
    return ret;
};

uint32_t TwaiCAN::busErrCounter()
{
    uint32_t ret = 0;
    if(getStatusInfo()) {
        ret = status.bus_error_count;
    }
    return ret;
};

uint32_t TwaiCAN::canState()
{
    uint32_t ret = 0;
    if(getStatusInfo()) {
        ret = (uint32_t)status.state;
    }
    return ret;
};


bool TwaiCAN::setPins(int8_t txPin, int8_t rxPin) {
    bool ret = !init;

    if(txPin >= 0) tx = txPin;
    else ret = false;
    if(rxPin >= 0) rx = rxPin;
    else ret = false;

    return ret;
}

bool TwaiCAN::recover(void) {
    uint32_t ret = 0;
    if(!getStatusInfo()) {
        log_i("CAN bus status read failed!");
        return false;
    }
    switch(status.state)
    {
      case TWAI_STATE_BUS_OFF:
      {
        log_i("Bus was off, starting recovery");
        return twai_initiate_recovery();
      }
      case TWAI_STATE_RECOVERING:
      {
        // Already recovering, nothing to do
        return true;
      }
      case TWAI_STATE_STOPPED:
      {
        // Stopped, nothing to do
        return true;
      }
      default:
      {
        log_i("Wrong state for recovery!");
      }
    }

    return false;
}

bool TwaiCAN::restart(void) {
    uint32_t ret = 0;
    if(!getStatusInfo()) {
        log_i("CAN bus status read failed!");
        return false;
    }
    switch(status.state)
    {
      case TWAI_STATE_STOPPED:
      {
        // Stopped, restart
        return twai_start();
      }
      default:
      {
        log_i("Wrong state for restart!");
      }
    }

    return false;
}

bool TwaiCAN::begin(TwaiSpeed twaiSpeed, 
                    int8_t txPin, int8_t rxPin,
                    uint16_t txQueue, uint16_t rxQueue,
                    twai_filter_config_t*  fConfig,
                    twai_general_config_t* gConfig,
                    twai_timing_config_t*  tConfig)
                    {

    bool ret = false;
    if(end()) {
        init = true;
        setSpeed(twaiSpeed);
        setPins(txPin, rxPin);
        
        gpio_reset_pin((gpio_num_t)rx);
        gpio_reset_pin((gpio_num_t)tx);

        setTxQueueSize(txQueue);
        setRxQueueSize(rxQueue);

        twai_general_config_t g_config = {.mode = TWAI_MODE_NORMAL, .tx_io = (gpio_num_t) tx, .rx_io = (gpio_num_t) rx, \
                                                .clkout_io = TWAI_IO_UNUSED, .bus_off_io = TWAI_IO_UNUSED,      \
                                                .tx_queue_len = txQueueSize, .rx_queue_len = rxQueueSize,       \
                                                .alerts_enabled = TWAI_ALERT_NONE,  .clkout_divider = 0,        \
                                                .intr_flags = ESP_INTR_FLAG_LEVEL1};

        twai_timing_config_t t_config[TWAI_SPEED_SIZE] = {
            #if (SOC_TWAI_BRP_MAX > 256)
            TWAI_TIMING_CONFIG_1KBITS(),
            TWAI_TIMING_CONFIG_5KBITS(),
            TWAI_TIMING_CONFIG_10KBITS(),
            #endif
            #if (SOC_TWAI_BRP_MAX > 128) || (CONFIG_ESP32_REV_MIN_FULL >= 200)
            TWAI_TIMING_CONFIG_12_5KBITS(),
            TWAI_TIMING_CONFIG_16KBITS(),
            TWAI_TIMING_CONFIG_20KBITS(),
            #endif
            TWAI_TIMING_CONFIG_50KBITS(),
            TWAI_TIMING_CONFIG_100KBITS(),
            TWAI_TIMING_CONFIG_125KBITS(),
            TWAI_TIMING_CONFIG_250KBITS(),
            TWAI_TIMING_CONFIG_500KBITS(),
            TWAI_TIMING_CONFIG_800KBITS(),
            TWAI_TIMING_CONFIG_1MBITS()
        };

        twai_filter_config_t f_config = TWAI_FILTER_CONFIG_ACCEPT_ALL();

        if(!gConfig) gConfig = &g_config;
        if(!tConfig) tConfig = &t_config[speed];
        if(!fConfig) fConfig = &f_config;

            //Install TWAI driver
        if (twai_driver_install(gConfig, tConfig, fConfig) == ESP_OK) {
            log_i("Driver installed");
        } else {
            log_i("Failed to install driver");
        }

        //Start TWAI driver
        if (twai_start() == ESP_OK) {
            log_i("Driver started");
            ret = true;
        } else {
            log_i("Failed to start driver");
        }
        if(!ret) end();
    }
    return ret;
}


bool TwaiCAN::end() {
    bool ret = false;
    if(init) {
        //Stop the TWAI driver
        if (twai_stop() == ESP_OK) {
            log_i("Driver stopped\n");
            ret = true;
        } else {
            log_i("Failed to stop driver\n");
        }

        //Uninstall the TWAI driver
        if (twai_driver_uninstall() == ESP_OK) {
            log_i("Driver uninstalled\n");
            ret &= true;
        } else {
            log_i("Failed to uninstall driver\n");
            ret &= false;
        }
        init = !ret;
    } else ret = true;
    return ret;
}


// Put this into your TwaiCAN class .cpp (and add prototypes in .hpp)
// Needs: #include "driver/twai.h" and Arduino log macros or ESP_LOGx

struct TwaiHealthSnapshot {
  uint32_t ms = 0;

  // Status
  twai_state_t state = TWAI_STATE_STOPPED;
  uint32_t msgs_to_tx = 0;
  uint32_t msgs_to_rx = 0;
  uint32_t tx_error_counter = 0;
  uint32_t rx_error_counter = 0;
  uint32_t tx_failed_count = 0;
  uint32_t rx_missed_count = 0;
  uint32_t bus_error_count = 0;
  uint32_t arb_lost_count = 0;

  // Alerts (bitmask)
  uint32_t alerts = 0;

  // GPIO sampling (best-effort)
  int rx_level = -1;   // 0/1 if readable
  int tx_level = -1;
};

static const char* twaiStateStr(twai_state_t s) {
  switch (s) {
    case TWAI_STATE_STOPPED:   return "STOPPED";
    case TWAI_STATE_RUNNING:   return "RUNNING";
    case TWAI_STATE_BUS_OFF:   return "BUS_OFF";
    case TWAI_STATE_RECOVERING:return "RECOVERING";
    default:                   return "UNKNOWN";
  }
}

static void printAlertBits(uint32_t a) {
  // Alerts vary slightly by IDF version, but these are the common ones
  struct { uint32_t bit; const char* name; } m[] = {
    { TWAI_ALERT_TX_SUCCESS,        "TX_SUCCESS" },
    { TWAI_ALERT_TX_FAILED,         "TX_FAILED" },
    { TWAI_ALERT_RX_DATA,           "RX_DATA" },
    { TWAI_ALERT_RX_QUEUE_FULL,     "RX_QUEUE_FULL" },
    { TWAI_ALERT_RX_FIFO_OVERRUN,   "RX_FIFO_OVERRUN" },
    { TWAI_ALERT_TX_RETRIED,        "TX_RETRIED" },
    { TWAI_ALERT_ARB_LOST,          "ARB_LOST" },
    { TWAI_ALERT_BUS_ERROR,         "BUS_ERROR" },
    { TWAI_ALERT_ERR_PASS,          "ERR_PASSIVE" },
    { TWAI_ALERT_BUS_OFF,           "BUS_OFF" },
    { TWAI_ALERT_BUS_RECOVERED,     "BUS_RECOVERED" },
    { TWAI_ALERT_ABOVE_ERR_WARN,    "ABOVE_ERR_WARN" },
    { TWAI_ALERT_BELOW_ERR_WARN,    "BELOW_ERR_WARN" },
  };

  bool any = false;
  for (auto &e : m) {
    if (a & e.bit) {
      if (!any) { log_i("  alerts:"); any = true; }
      log_i("    - %s", e.name);
    }
  }
  if (!any) log_i("  alerts: (none)");
}

static uint32_t nowMs() {
  return (uint32_t)(esp_timer_get_time() / 1000ULL);
}
/*
// Enable a broad set of alerts (call after begin(), once)
bool TwaiCAN::enableHealthAlerts() {
  uint32_t mask =
    TWAI_ALERT_TX_SUCCESS |
    TWAI_ALERT_TX_FAILED |
    TWAI_ALERT_RX_DATA |
    TWAI_ALERT_RX_QUEUE_FULL |
    TWAI_ALERT_RX_FIFO_OVERRUN |
    TWAI_ALERT_TX_RETRIED |
    TWAI_ALERT_ARB_LOST |
    TWAI_ALERT_BUS_ERROR |
    TWAI_ALERT_ERR_PASS |
    TWAI_ALERT_BUS_OFF |
    TWAI_ALERT_BUS_RECOVERED |
    TWAI_ALERT_ABOVE_ERR_WARN |
    TWAI_ALERT_BELOW_ERR_WARN;

  esp_err_t e = twai_reconfigure_alerts(mask, nullptr);
  if (e != ESP_OK) {
    log_i("twai_reconfigure_alerts failed: %d", (int)e);
    return false;
  }
  // Clear any stale alerts
  uint32_t dummy = 0;
  (void)twai_read_alerts(&dummy, 0);
  return true;
}

// Take one snapshot (status + current alerts + optional pin levels)
bool TwaiCAN::readHealthSnapshot(TwaiHealthSnapshot& out, uint32_t alert_wait_ms) {
  twai_status_info_t st;
  if (twai_get_status_info(&st) != ESP_OK) return false;

  out.ms = nowMs();
  out.state = st.state;
  out.msgs_to_tx = st.msgs_to_tx;
  out.msgs_to_rx = st.msgs_to_rx;
  out.tx_error_counter = st.tx_error_counter;
  out.rx_error_counter = st.rx_error_counter;
  out.tx_failed_count = st.tx_failed_count;
  out.rx_missed_count = st.rx_missed_count;
  out.bus_error_count = st.bus_error_count;
  out.arb_lost_count = st.arb_lost_count;

  uint32_t alerts = 0;
  (void)twai_read_alerts(&alerts, pdMS_TO_TICKS(alert_wait_ms));
  out.alerts = alerts;

  // Best-effort bus stuck hint: sample levels on RX/TX pins (not a true CAN differential read!)
  if (rx >= 0) out.rx_level = gpio_get_level((gpio_num_t)rx);
  if (tx >= 0) out.tx_level = gpio_get_level((gpio_num_t)tx);

  return true;
}


// Print snapshot + deltas + interpretation
void TwaiCAN::logBusHealth(uint32_t observe_ms, uint32_t alert_wait_ms) {
  static bool have_prev = false;
  static TwaiHealthSnapshot prev;

  TwaiHealthSnapshot cur;
  if (!readHealthSnapshot(cur, alert_wait_ms)) {
    log_i("CAN health: status read failed");
    return;
  }

  uint32_t dt = 0;
  if (have_prev) dt = (cur.ms >= prev.ms) ? (cur.ms - prev.ms) : 0;

  log_i("CAN health:");
  log_i("  state: %s", twaiStateStr(cur.state));
  log_i("  speed: %lu kbps", (unsigned long)getSpeedNumeric());
  log_i("  queues: tx=%lu rx=%lu (cfg tx=%u rx=%u)",
        (unsigned long)cur.msgs_to_tx, (unsigned long)cur.msgs_to_rx,
        (unsigned)txQueueSize, (unsigned)rxQueueSize);
  log_i("  TEC/REC: %lu / %lu",
        (unsigned long)cur.tx_error_counter, (unsigned long)cur.rx_error_counter);
  log_i("  counters: tx_failed=%lu rx_missed=%lu bus_err=%lu arb_lost=%lu",
        (unsigned long)cur.tx_failed_count,
        (unsigned long)cur.rx_missed_count,
        (unsigned long)cur.bus_error_count,
        (unsigned long)cur.arb_lost_count);

  if (cur.rx_level != -1 || cur.tx_level != -1) {
    log_i("  pin_levels (raw): RX=%d TX=%d", cur.rx_level, cur.tx_level);
  }

  printAlertBits(cur.alerts);

  if (have_prev && dt > 0) {
    auto d = [&](uint32_t a, uint32_t b){ return (a>=b)?(a-b):0; };

    uint32_t d_tx_failed = d(cur.tx_failed_count, prev.tx_failed_count);
    uint32_t d_rx_missed = d(cur.rx_missed_count, prev.rx_missed_count);
    uint32_t d_bus_err   = d(cur.bus_error_count, prev.bus_error_count);
    uint32_t d_arb_lost  = d(cur.arb_lost_count, prev.arb_lost_count);

    log_i("  delta over %lums: tx_failed=%lu rx_missed=%lu bus_err=%lu arb_lost=%lu",
          (unsigned long)dt,
          (unsigned long)d_tx_failed,
          (unsigned long)d_rx_missed,
          (unsigned long)d_bus_err,
          (unsigned long)d_arb_lost);

    // Fast heuristics for "bus health" interpretation
    if (cur.state == TWAI_STATE_BUS_OFF) {
      log_i("  hint: BUS_OFF -> call recover() and check wiring/termination/bitrate");
    } else if (cur.alerts & TWAI_ALERT_ERR_PASS) {
      log_i("  hint: error-passive -> heavy errors, likely bitrate mismatch, noise, or bad termination");
    } else if (cur.alerts & TWAI_ALERT_ABOVE_ERR_WARN) {
      log_i("  hint: above error warning -> errors accumulating");
    }

    if (d_bus_err > 0) {
      log_i("  hint: bus errors increasing -> check 120R ends, stub lengths, transceiver power/ground, bitrate");
    }
    if (d_arb_lost > 0) {
      log_i("  hint: arbitration lost -> bus busy or higher-priority talkers present (this is normal at high load)");
    }
    if (d_tx_failed > 0) {
      log_i("  hint: TX failures -> ACK missing (no other node, wrong bitrate), or bus stuck dominant, or transceiver issue");
    }
    if (d_rx_missed > 0 || (cur.alerts & (TWAI_ALERT_RX_QUEUE_FULL | TWAI_ALERT_RX_FIFO_OVERRUN))) {
      log_i("  hint: RX overrun/missed -> you are not draining RX fast enough (increase rx_queue, move RX to a task, avoid long critical sections)");
    }

    // "Bus blocked" style symptoms (best-effort)
    // Not definitive, but useful for field debugging:
    if (cur.state == TWAI_STATE_RUNNING &&
        cur.tx_error_counter > prev.tx_error_counter &&
        (cur.alerts & (TWAI_ALERT_TX_FAILED | TWAI_ALERT_BUS_ERROR)) &&
        cur.msgs_to_rx == prev.msgs_to_rx) {
      log_i("  hint: sending causes errors but no RX activity -> possible bitrate mismatch or no ACK partner");
    }

    if (cur.state == TWAI_STATE_RUNNING &&
        cur.msgs_to_tx > (txQueueSize / 2) &&
        d_tx_failed == 0 &&
        d_arb_lost > 0) {
      log_i("  hint: TX queue piling up while losing arbitration -> bus near saturation or a high-priority spammer");
    }
  }

  prev = cur;
  have_prev = true;

  // Optional short observation window: sample a few times to catch spikes
  if (observe_ms > 0) {
    uint32_t start = nowMs();
    uint32_t samples = 0;
    uint32_t sum_bus_err = 0, sum_tx_failed = 0, sum_arb_lost = 0, sum_rx_missed = 0;
    TwaiHealthSnapshot s0 = cur, s1;

    while ((nowMs() - start) < observe_ms) {
      vTaskDelay(pdMS_TO_TICKS(50));
      if (!readHealthSnapshot(s1, 0)) break;
      sum_bus_err   += (s1.bus_error_count   - s0.bus_error_count);
      sum_tx_failed += (s1.tx_failed_count   - s0.tx_failed_count);
      sum_arb_lost  += (s1.arb_lost_count    - s0.arb_lost_count);
      sum_rx_missed += (s1.rx_missed_count   - s0.rx_missed_count);
      s0 = s1;
      samples++;
    }

    if (samples > 0) {
      log_i("CAN observe %lums: bus_err=%lu tx_failed=%lu arb_lost=%lu rx_missed=%lu (sampled %lu times)",
            (unsigned long)observe_ms,
            (unsigned long)sum_bus_err,
            (unsigned long)sum_tx_failed,
            (unsigned long)sum_arb_lost,
            (unsigned long)sum_rx_missed,
            (unsigned long)samples);
    }
  }
}
*/

TwaiCAN ESP32Can;