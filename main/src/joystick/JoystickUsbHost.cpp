// JoystickUsbHost.cpp — USB Host + HID Host for DS4 on ESP32-S3.
//
// Lifted from the Espressif/touchgadget ESP32_USB_Host_HID-ARDUINO
// hid_host_joystick.ino reference, with the 64-byte generic-report path
// rewired to publish a Ds4State_t into a FreeRTOS queue instead of printf().
#include "JoystickUsbHost.h"

#ifdef JOYSTICK_USBHOST_PROVIDER

#include <Arduino.h>
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/queue.h"
#include "freertos/task.h"
#include "driver/gpio.h"
#include "usb/usb_host.h"

#include "hid_host.h"

static const char* TAG = "ds4_usb";

namespace {

QueueHandle_t s_hidEventQueue = nullptr; // raw HID Host events
QueueHandle_t s_ds4Queue      = nullptr; // decoded Ds4State, length 1, overwrite
volatile bool s_userShutdown  = false;

struct HidHostEvent {
    hid_host_device_handle_t handle;
    hid_host_driver_event_t  event;
    void*                    arg;
};

static inline JoystickUsbHost::Ds4State zeroState(bool valid)
{
    JoystickUsbHost::Ds4State s = {};
    s.dpad = 8; // released
    s.valid = valid;
    return s;
}

static void pushDecoded(const JoystickUsbHost::Ds4State& s)
{
    if (!s_ds4Queue) return;
    // Overwrite: consumer always sees freshest sample.
    xQueueOverwrite(s_ds4Queue, &s);
}

// DS4 64-byte boot-style report layout (matches the touchgadget reference).
typedef struct __attribute__((packed)) {
    uint8_t ReportID;
    uint8_t leftXAxis;
    uint8_t leftYAxis;
    uint8_t rightXAxis;
    uint8_t rightYAxis;
    uint8_t dPad:4;
    uint8_t button1:4;  // Triangle[7], Circle[6], Cross[5], Square[4]
    uint8_t button2;    // R3:7,L3:6,Options:5,Share:4,R2:3,L2:2,R1:1,L1:0
    uint8_t button3:2;  // tpad click[1], logo[0]
    uint8_t reportCnt:6;
    uint8_t L2Axis;
    uint8_t R2Axis;
    uint16_t timestamp;
    uint8_t batteryLvl;
    uint16_t gyroX, gyroY, gyroZ;
    int16_t  accelX, accelY, accelZ;
    uint8_t  filler[39];
} DS4Report_t;

static inline int16_t scale_u8_to_i16(uint8_t v)
{
    // Centre at 128 then expand to int16 range. Matches the kOffset/kCurve
    // dynamic range expected by MotorGamePad::handleAxis.
    return (int16_t)((int)v - 128) * 256;
}

static void decodeDs4(const uint8_t* data, int length)
{
    if (length < (int)sizeof(DS4Report_t)) return;
    const DS4Report_t* r = (const DS4Report_t*)data;

    JoystickUsbHost::Ds4State s;
    s.lx      = scale_u8_to_i16(r->leftXAxis);
    s.ly      = scale_u8_to_i16(r->leftYAxis);
    s.rx      = scale_u8_to_i16(r->rightXAxis);
    s.ry      = scale_u8_to_i16(r->rightYAxis);
    s.l2      = r->L2Axis;
    s.r2      = r->R2Axis;
    s.dpad    = r->dPad;
    s.button1 = r->button1;
    s.button2 = r->button2;
    s.button3 = r->button3;
    s.valid   = true;
    pushDecoded(s);
}

static void hid_host_generic_report_cb(const uint8_t* const data, const int length)
{
    if (length == 64) {
        decodeDs4(data, length);
    }
    // Other report sizes (Logitech / T16K) are intentionally ignored — the
    // bridge is DS4-specific.
}

static void hid_host_interface_cb(hid_host_device_handle_t handle,
                                  const hid_host_interface_event_t event,
                                  void* arg)
{
    uint8_t buf[64] = {0};
    size_t  bufLen  = 0;
    hid_host_dev_params_t params;
    ESP_ERROR_CHECK(hid_host_device_get_params(handle, &params));

    switch (event) {
    case HID_HOST_INTERFACE_EVENT_INPUT_REPORT:
        if (hid_host_device_get_raw_input_report_data(handle, buf, sizeof(buf), &bufLen) == ESP_OK) {
            // Boot kbd/mouse get ignored; only generic 64B reports decoded.
            if (params.sub_class != HID_SUBCLASS_BOOT_INTERFACE) {
                hid_host_generic_report_cb(buf, (int)bufLen);
            }
        }
        break;
    case HID_HOST_INTERFACE_EVENT_DISCONNECTED:
        ESP_LOGI(TAG, "HID Device DISCONNECTED");
        hid_host_device_close(handle);
        pushDecoded(zeroState(false));
        break;
    case HID_HOST_INTERFACE_EVENT_TRANSFER_ERROR:
        ESP_LOGW(TAG, "HID Device TRANSFER_ERROR");
        break;
    default:
        break;
    }
}

static void hid_host_device_handle_event(hid_host_device_handle_t handle,
                                         const hid_host_driver_event_t event,
                                         void* arg)
{
    hid_host_dev_params_t params;
    ESP_ERROR_CHECK(hid_host_device_get_params(handle, &params));
    const hid_host_device_config_t dev_config = {
        .callback     = hid_host_interface_cb,
        .callback_arg = NULL,
    };
    if (event == HID_HOST_DRIVER_EVENT_CONNECTED) {
        ESP_LOGI(TAG, "HID Device CONNECTED (sub=%d proto=%d)",
                 params.sub_class, params.proto);
        ESP_ERROR_CHECK(hid_host_device_open(handle, &dev_config));
        if (params.sub_class == HID_SUBCLASS_BOOT_INTERFACE) {
            hid_class_request_set_protocol(handle, HID_REPORT_PROTOCOL_BOOT);
        }
        ESP_ERROR_CHECK(hid_host_device_start(handle));
    }
}

static void hid_host_device_cb(hid_host_device_handle_t handle,
                               const hid_host_driver_event_t event,
                               void* arg)
{
    if (!s_hidEventQueue) return;
    const HidHostEvent e = { handle, event, arg };
    xQueueSend(s_hidEventQueue, &e, 0);
}

static void usb_lib_task(void* arg)
{
    const usb_host_config_t host_config = {
        .skip_phy_setup = false,
        .intr_flags     = ESP_INTR_FLAG_LEVEL1,
    };
    ESP_ERROR_CHECK(usb_host_install(&host_config));
    xTaskNotifyGive((TaskHandle_t)arg);

    while (!s_userShutdown) {
        uint32_t flags;
        usb_host_lib_handle_events(portMAX_DELAY, &flags);
        if (flags & USB_HOST_LIB_EVENT_FLAGS_NO_CLIENTS) {
            usb_host_device_free_all();
        }
    }
    vTaskDelay(pdMS_TO_TICKS(10));
    usb_host_uninstall();
    vTaskDelete(NULL);
}

static void hid_host_task(void* arg)
{
    s_hidEventQueue = xQueueCreate(10, sizeof(HidHostEvent));
    while (!s_userShutdown) {
        HidHostEvent e;
        if (xQueueReceive(s_hidEventQueue, &e, pdMS_TO_TICKS(50))) {
            hid_host_device_handle_event(e.handle, e.event, e.arg);
        }
    }
    vQueueDelete(s_hidEventQueue);
    s_hidEventQueue = nullptr;
    vTaskDelete(NULL);
}

} // namespace

namespace JoystickUsbHost {

bool begin()
{
    static bool started = false;
    if (started) return true;

    s_ds4Queue = xQueueCreate(1, sizeof(Ds4State));
    if (!s_ds4Queue) {
        ESP_LOGE(TAG, "ds4 queue alloc failed");
        return false;
    }
    // Seed with a zeroed invalid state so the consumer's xQueuePeek never
    // returns garbage before the first report arrives.
    Ds4State seed = zeroState(false);
    xQueueOverwrite(s_ds4Queue, &seed);

    BaseType_t ok = xTaskCreatePinnedToCore(usb_lib_task, "ds4_usbcore", 4096,
                                            xTaskGetCurrentTaskHandle(), 2, NULL, 0);
    if (ok != pdTRUE) {
        ESP_LOGE(TAG, "usb_lib_task create failed");
        return false;
    }
    // Wait for usb_host_install to complete.
    ulTaskNotifyTake(false, pdMS_TO_TICKS(1000));

    const hid_host_driver_config_t hid_cfg = {
        .create_background_task = true,
        .task_priority          = 5,
        .stack_size             = 4096,
        .core_id                = 0,
        .callback               = hid_host_device_cb,
        .callback_arg           = NULL,
    };
    if (hid_host_install(&hid_cfg) != ESP_OK) {
        ESP_LOGE(TAG, "hid_host_install failed");
        return false;
    }

    s_userShutdown = false;
    ok = xTaskCreate(hid_host_task, "ds4_hid", 4096, NULL, 2, NULL);
    if (ok != pdTRUE) {
        ESP_LOGE(TAG, "hid_host_task create failed");
        return false;
    }
    started = true;
    ESP_LOGI(TAG, "DS4 USB host bridge started");
    return true;
}

QueueHandle_t getQueue() { return s_ds4Queue; }

} // namespace JoystickUsbHost

#endif // JOYSTICK_USBHOST_PROVIDER
