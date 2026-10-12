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
#include "ConfigController.h"
#include "DialController.h"
#include "DigitalInController.h"
#include "DigitalOutController.h"
#include "FanController.h"
#include "I2cBridge.h"
#include "InterruptController.h"
#include "MessageController.h"
#include "QidRegistry.h"
#include "State.h"
#include "Tmp102Controller.h"
#include "NVSConfig.h"
#include "i2c_slave_dial.h"
#include "uc2_hal/uc2_esp.h"
#include "uc2_hal/uc2_log.h"
#include "uc2_hal/uc2_prefs.h"
#include "uc2_hal/uc2_serial.h"
#include "uc2_hal/uc2_string.h"
#include "uc2_hal/uc2_wire.h"
#include "uc2_hal/uc2_time.h"
#include "nvs_flash.h"

static const char *TAG = "t13scratch";

// Scratch link stub: the real SerialProcess.cpp (867-line hub) is not part of
// scratch_t12 yet. DigitalInController's E-stop path references
// SerialProcess::serialize(cJSON*) at link time; this weak stub keeps the
// link green and frees the doc like the real one does.
namespace SerialProcess
{
__attribute__((weak)) void serialize(cJSON *doc)
{
    char *s = doc ? cJSON_PrintUnformatted(doc) : nullptr;
    std::printf("estop event (scratch stub): %s\n", s ? s : "(null)");
    if (s) cJSON_free(s);
    if (doc) cJSON_Delete(doc);
}
// QidRegistry sends {"qid":N,"state":"..."} through this; stub echoes to
// console (the real hub serializes over UART once it is converted).
__attribute__((weak)) void safeSendJsonString(char *jsonString)
{
    std::printf("qid notify (scratch stub): %s\n", jsonString ? jsonString : "(null)");
}
}  // namespace SerialProcess

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

    // I2cBridge — converted to uc2::Wire (T1.3 action 2)
    I2cBridge::setup();
    I2cBridge::loop();
    std::printf("T1.3 scratch: I2cBridge setup/loop ok\n");

    // Tmp102 — Wire + tsens driver (T1.3 action 2)
    Tmp102Controller::setup();
    Tmp102Controller::loop();
    std::printf("T1.3 scratch: Tmp102 pcb=%.1f air=%.1f esp=%.1f\n",
                Tmp102Controller::getPcbC(), Tmp102Controller::getAirC(),
                Tmp102Controller::getEspC());

    // DigitalInController — uc2 gpio/prefs/time façade (T1.3 action 2).
    // DIGITAL_IN_* and pinEmergencyExit are -1 in this board combo, so
    // setup()/checkEmergencyStop() take their no-pin paths (no GPIO touched).
    DigitalInController::setup();
    DigitalInController::loop();
    cJSON *droot = cJSON_Parse("{\"qid\":11,\"digitalinid\":1}");
    cJSON *dg = DigitalInController::get(droot);
    {
        char *ds = dg ? cJSON_PrintUnformatted(dg) : nullptr;
        std::printf("T1.3 scratch: DigitalInController get=%s\n", ds ? ds : "(null)");
        if (ds) cJSON_free(ds);
        if (dg) cJSON_Delete(dg);
    }
    cJSON_Delete(droot);
    DigitalInController::checkEmergencyStop(); // pin -1 → early return
    std::printf("T1.3 scratch: DigitalInController estop active=%d pol=%d\n",
                (int)DigitalInController::isEmergencyActive(),
                (int)DigitalInController::getEmergencyPolarity());

    // DigitalOutController — uc2 gpio/time façade (T1.3 action 2).
    // DIGITAL_OUT_* are -1 in this board combo: pinMode/digitalWrite no-op
    // via the façade's pin guard (-1 → 255 >= SOC_GPIO_PIN_COUNT).
    DigitalOutController::setup();
    DigitalOutController::loop();
    cJSON *oroot = cJSON_Parse("{\"qid\":12,\"digitaloutid\":1,\"digitaloutval\":1,"
                               "\"digitalout1IsTrigger\":1,\"digitalout1TriggerDelayOn\":10,"
                               "\"digitalout1TriggerDelayOff\":10}");
    const int oq = DigitalOutController::act(oroot);
    DigitalOutController::loop(); // runs the (no-pin) trigger table once
    cJSON *og = DigitalOutController::get(oroot);
    {
        char *os = og ? cJSON_PrintUnformatted(og) : nullptr;
        std::printf("T1.3 scratch: DigitalOutController act qid=%d get=%s\n", oq, os ? os : "(null)");
        if (os) cJSON_free(os);
        if (og) cJSON_Delete(og);
    }
    cJSON_Delete(oroot);
    std::printf("T1.3 scratch: DigitalOutController setup/loop ok (pins -1)\n");

    // FanController — Wire + Preferences + tacho GPIO ISR (T1.3 action 2)
    FanController::setup();
    FanController::loop();
    cJSON *froot = cJSON_Parse("{\"qid\":9,\"fan\":{\"kick\":1}}");
    const int fq = FanController::act(froot);
    cJSON *fg = FanController::get(froot);
    std::printf("T1.3 scratch: FanController act qid=%d wiper=0x%02X rpm=%.1f\n",
                fq, (unsigned)FanController::getWiper(), FanController::getRpm());
    if (fg)
    {
        char *fs = cJSON_PrintUnformatted(fg);
        std::printf("T1.3 scratch: FanController get=%s\n", fs ? fs : "(null)");
        if (fs) cJSON_free(fs);
        cJSON_Delete(fg);
    }
    else
    {
        std::printf("T1.3 scratch: FanController get=NULL\n");
    }
    cJSON_Delete(froot);

    // i2c_slave_dial — uc2::Wire slave (T1.3 action 2). Board combo here has
    // I2C pins -1 / I2C_CONTROLLER_TYPE=mDISABLED: setup() must fail the slave
    // begin cleanly and register callbacks; loop-level callbacks are no-ops.
    i2c_slave_dial::setup();
    const DialData dd = DialController::getDialValues();
    DialController::setDialValues(DialData{11, 22, 33, 44, 55});
    const DialData dd2 = DialController::getDialValues();
    std::printf("T1.3 scratch: i2c_slave_dial setup ok (dial A=%ld X=%ld Y=%ld Z=%ld I=%ld after set/get)\n",
                (long)dd.pos_a, (long)dd2.pos_x, (long)dd2.pos_y, (long)dd2.pos_z, (long)dd2.intensity);

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

    // uc2::Wire façade (master, I2C_NUM_0). No device on the bus in the
    // scratch build → endTransmission must NACK (non-zero), requestFrom 0 bytes.
    if (!Wire.begin(45, 44, 100000))
    {
        std::printf("T1.3 scratch: Wire.begin FAILED\n");
    }
    Wire.beginTransmission(0x68);
    Wire.write(0x10);
    uint8_t we = Wire.endTransmission();
    std::printf("T1.3 scratch: Wire endTransmission=0x%02x (non-zero expected: no device)\n", we);
    int rq = Wire.requestFrom(0x68, 2);
    std::printf("T1.3 scratch: Wire requestFrom=%d available=%d (0 expected)\n", rq, Wire.available());

    // InterruptController — raw-IDF GPIO ISR + queue task (T1.3 action 2).
    // Pin 38 is spare on this combo; a CHANGE listener just records the pin.
    static volatile uint8_t isrPin = 0;
    InterruptController::addInterruptListner(38,
                                             [](uint8_t pin) { isrPin = pin; },
                                             GPIO_INTR_ANYEDGE);
    delay(50);
    std::printf("T1.3 scratch: InterruptController attached pin38 (isrPin=%u)\n",
                (unsigned)isrPin);
    InterruptController::removeInterruptListner(38);

    // ConfigController — uc2::Preferences PSX getters (T1.3 action 2)
    Config::setup();
    char *psxMac = Config::getPsxMac();
    std::printf("T1.3 scratch: Config setup ok, mac=%s type=%d\n",
                psxMac ? psxMac : "(null)", Config::getPsxControllerType());
    delete[] psxMac;

    // MessageController — uc2 prefs/string/serial/log (T1.3 action 2).
    // HOME_MOTOR/MOTOR_CONTROLLER are off in this combo, so the homing/motor
    // paths (incl. DeviceRouter::handleHomeAct) compile out — no link deps.
    MessageController::setup();
    MessageController::loop();
    cJSON *mroot = cJSON_Parse("{\"qid\":13,\"message\":{\"key\":1,\"value\":2}}");
    const int mq = MessageController::act(mroot);
    MessageController::startHomingWithStoredParams(1); // HOME_MOTOR off → log_w path
    cJSON *mg = MessageController::get(mroot);
    {
        char *ms = mg ? cJSON_PrintUnformatted(mg) : nullptr;
        std::printf("T1.3 scratch: MessageController act qid=%d get=%s\n", mq, ms ? ms : "(null)");
        if (ms) cJSON_free(ms);
        if (mg) cJSON_Delete(mg);
    }
    cJSON_Delete(mroot);

    // QidRegistry — FreeRTOS mutex + millis/log (T1.3 action 2).
    // FocusMotor paths compile out (MOTOR_CONTROLLER off); SerialProcess::
    // safeSendJsonString resolves to the weak stub above.
    QidRegistry::setup();
    const bool qreg = QidRegistry::registerQid(15, 2);
    const int qstate = (int)QidRegistry::getQidState(15);
    QidRegistry::reportActionDone(15); // 1/2 — no notification yet
    QidRegistry::reportActionDone(15); // 2/2 — weak stub prints "done"
    const bool qpause = QidRegistry::pauseQid(16); // qid 16 not busy → false
    cJSON *qroot = cJSON_Parse("{\"qid\":16}");
    const int qh = QidRegistry::handleQidStateQuery(qroot); // weak stub prints state
    std::printf("T1.3 scratch: QidRegistry reg=%d state=%d pause16=%d handleQ=%d\n",
                (int)qreg, qstate, (int)qpause, qh);
    cJSON_Delete(qroot);

    // State — uc2 gpio/prefs/log/time/ESP façade (T1.3 action 2).
    // I2C_MASTER undefined in this combo → i2c_master::reboot()/startOTA()
    // compile out; WiFi/OTA path is ARDUINO-only → startOTA takes log_w.
    State::setup();
    State::printInfo();
    State::setBusy(true);
    cJSON *sroot = cJSON_Parse("{\"qid\":14,\"isBusy\":1}");
    const int sq = State::act(sroot);
    cJSON *sg = State::get(sroot);
    cJSON *smods = State::getModules();
    {
        char *sgs = sg ? cJSON_PrintUnformatted(sg) : nullptr;
        char *sms = smods ? cJSON_PrintUnformatted(smods) : nullptr;
        std::printf("T1.3 scratch: State act qid=%d busy=%d power=%d get=%s modules=%s\n",
                    sq, (int)State::getBusy(), (int)State::getBusPower(),
                    sgs ? sgs : "(null)", sms ? sms : "(null)");
        if (sgs) cJSON_free(sgs);
        if (sms) cJSON_free(sms);
        if (sg) cJSON_Delete(sg);
        if (smods) cJSON_Delete(smods);
    }
    cJSON_Delete(sroot);
    State::startOTA();  // raw-IDF path → log_w, no WiFi
    State::stopOTA();
    std::printf("T1.3 scratch: State setup/act/get/getModules/OTA ok (no WiFi in raw IDF)\n");

    std::printf("T1.3 scratch: components + uc2_hal + BuzzerController + NVSConfig + Serial + Wire OK (no Arduino, no shim)\n");
}

} // extern "C"
