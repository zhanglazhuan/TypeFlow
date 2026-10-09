/*
 * hidout USB 后端 —— TinyUSB HID keyboard device
 *
 * TODO 尚未实现。实现前需要：
 *   1. 在 main/idf_component.yml 加依赖: espressif/esp_tinyusb
 *      （TinyUSB 不在核心 IDF 里，components/usb 是 USB Host 栈，不是这个）
 *   2. 确认 sdkconfig 里控制台没有占用 USB：本项目控制台走 UART0，
 *      见 sdkconfig.defaults 中 CONFIG_ESP_CONSOLE_UART_DEFAULT
 *   3. 写 HID report descriptor（标准 6KRO 键盘）
 *
 * 参考起点：ESP-IDF 例程 examples/peripherals/usb/device/tusb_hid
 */

#include "hidout_backend.h"
#include "esp_log.h"

static const char *TAG = "hidout_usb";

static esp_err_t usb_start(const hidout_config_t *cfg, hidout_rx_cb_t rx, void *rx_ctx)
{
    (void)cfg; (void)rx; (void)rx_ctx;
    ESP_LOGW(TAG, "USB 后端尚未实现（需先加托管依赖 espressif/esp_tinyusb）");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t usb_stop(void)            { return ESP_OK; }
static bool      usb_connected(void)       { return false; }

static esp_err_t usb_send_report(uint8_t modifiers, const uint8_t keys[6])
{
    (void)modifiers; (void)keys;
    return ESP_ERR_NOT_SUPPORTED;
}

static const hidout_backend_t s_usb = {
    .name          = "usb",
    .link          = TARGET_LINK_USB_HID,
    .start         = usb_start,
    .stop          = usb_stop,
    .connected     = usb_connected,
    .send_report   = usb_send_report,
    .send_bytes    = NULL,   /* COMPANION 策略下需要 vendor 接口，待定 */
};

const hidout_backend_t *hidout_backend_usb(void) { return &s_usb; }
