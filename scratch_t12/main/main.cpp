// T1.3 scratch app: links all six moved components (canopen_node, uc2_od,
// cjson, usb_host, esp_usb_host_hid, dac_module) AND references one public
// symbol from each, so the linker must pull every component's objects in.
// Build target: esp32s3, IDF 6.1 (no Arduino, no shim).
#include <cstdio>
#include <cstdint>

#include "CANopen.h"           // canopen_node
#include "cJSON.h"             // cjson
#include "hid_host.h"          // esp_usb_host_hid
#include "usb/usb_host.h"      // usb_host (vendored IDF 5.5.1 driver)
#include "DAC_Module.h"        // dac_module (C++)

// T1.3 action 2: uc2_hal façades + first converted leaf module (no Arduino)
#include "BuzzerController.h"
#include "NVSConfig.h"
#include "uc2_hal/uc2_esp.h"
#include "uc2_hal/uc2_log.h"
#include "uc2_hal/uc2_prefs.h"
#include "uc2_hal/uc2_serial.h"
#include "uc2_hal/uc2_string.h"
#include "uc2_hal/uc2_time.h"
#include "nvs_flash.h"

static const char *TAG = "t13scratch";

extern "C" {

// Reference one symbol per C component to force link-in:
//   canopen_node  -> CO_new (CANopenNode core)
//   usb_host      -> usb_host_install (vendored HCD/USBH stack)
//   esp_usb_host_hid -> hid_host_uninstall
// uc2_od is headerless; its OD array is pulled in transitively via
// canopen_node's CO_driver reference to OD.
void app_main(void)
{
    std::printf("T1.3 scratch: canopen linked (CO_new=%p)\n", (void *)CO_new);
    std::printf("T1.3 scratch: usb_host linked (usb_host_install=%p)\n", (void *)usb_host_install);
    (void)hid_host_uninstall;   // symbol reference only

    cJSON *root = cJSON_CreateObject();
    cJSON_AddItemToObject(root, "ok", cJSON_CreateBool(1));
    char *s = cJSON_PrintUnformatted(root);
    std::printf("T1.3 scratch: cjson=%s\n", s ? s : "(null)");
    if (s) cJSON_free(s);
    cJSON_Delete(root);

    // dac_module: default-construct + ODR-use a member fn to force the object in
    DAC_Module dac;
    (void)&DAC_Module::Setup;
    std::printf("T1.3 scratch: dac_module linked OK\n");

    // uc2_hal: exercise every façade wrapper (compile + runtime smoke)
    const uint32_t t0 = millis();
    delay(2);
    uc2::String mac = uc2::String(static_cast<unsigned char>(0x05), HEX) + "-" + "lit";
    uc2::String num = "42";
    log_i("T1.3 scratch: uc2_hal ok (ms_delta=%lu str=%s len=%u toInt=%d)",
          (unsigned long)(millis() - t0), mac.c_str(),
          (unsigned)mac.length(), num.toInt());
    std::printf("T1.3 scratch: ESP façade heap=%lu cycles=%lu\n",
                (unsigned long)ESP.getFreeHeap(), (unsigned long)ESP.getCycleCount());

    // BuzzerController — first module converted to direct IDF (T1.3 action 2)
    BuzzerController::setup();
    BuzzerController::loop();
    cJSON *broot = cJSON_Parse("{\"qid\":7,\"buzzer\":{\"beeps\":2,\"freq\":1000,\"duration\":50,\"gap\":20}}");
    const int bq = BuzzerController::act(broot);
    std::printf("T1.3 scratch: BuzzerController act qid=%d\n", bq);
    cJSON_Delete(broot);

    // NVSConfig — config module converted to uc2::Preferences (T1.3 action 2)
    nvs_flash_init();
    NVSConfig::loadConfig();
    std::printf("T1.3 scratch: NVSConfig loaded (motor=%d laser=%d canRole=%d canNodeId=%u)\n",
                (int)runtimeConfig.motor, (int)runtimeConfig.laser,
                (int)runtimeConfig.canRole, (unsigned)runtimeConfig.canNodeId);

    // uc2::Preferences round-trip on a scratch namespace (proves put/get/clear)
    uc2::Preferences p;
    if (p.begin("t13scratch"))
    {
        uint8_t blob[4] = {1, 2, 3, 4};
        p.putUChar("n", 42);
        p.putString("s", "hello");
        p.putBytes("b", blob, 4);
        std::printf("T1.3 scratch: prefs n=%u s=%s b_len=%zu\n",
                    (unsigned)p.getUChar("n", 0), p.getString("s", "?").c_str(),
                    p.getBytesLength("b"));
        p.clear();
        std::printf("T1.3 scratch: prefs after clear n=%u b_len=%zu\n",
                    (unsigned)p.getUChar("n", 99), p.getBytesLength("b"));
        p.end();
    }
    else
    {
        std::printf("T1.3 scratch: prefs begin FAILED\n");
    }

    // uc2::Serial façade (UART0; S3 console is USB-Serial-JTAG → no clash)
    Serial.setRxBufferSize(8192);  // main/main.cpp sets sizes before begin
    Serial.setTxBufferSize(2048);
    Serial.begin(115200);
    Serial.setTimeout(10);
    Serial.println("T1.3 scratch: Serial facade up");
    Serial.println(1234);
    Serial.println(3.14159);
    Serial.printf("free_heap=%lu\n", (unsigned long)ESP.getFreeHeap());
    Serial.write('A');
    Serial.flush();
    (void)Serial.available();
    char rb[8] = {0};
    (void)Serial.readBytes(rb, sizeof(rb), 5);
    Serial1.begin(115200);  // UART2 (TMC), default pins
    Serial1.println("serial1 ok");
    Serial1.end();
    std::printf("T1.3 scratch: Serial/Serial1 ok\n");

    std::printf("T1.3 scratch: components + uc2_hal + BuzzerController + NVSConfig OK (no Arduino, no shim)\n");
}

} // extern "C"
