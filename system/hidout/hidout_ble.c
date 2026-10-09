/*
 * hidout BLE 后端 —— NimBLE
 *
 * TODO 尚未实现。两种角色，取决于 text_mode：
 *   HID_UNICODE : 标准 HID over GATT（HID Service 0x1812），主机当普通蓝牙键盘认
 *   COMPANION   : 自定义 GATT 服务，直接把 UTF-8 文本写给主机侧配套 App
 *                 （Android 上唯一可靠的中文上屏路径，见 hidout.h 文件头）
 *
 * 两者可以并存：同时注册 HID Service 和自定义服务，按 text_mode 选用哪条通道发。
 *
 * NimBLE 已在核心 IDF 内，sdkconfig.defaults 里已开 CONFIG_BT_NIMBLE_ENABLED。
 * 参考起点：ESP-IDF 例程 examples/bluetooth/esp_hid_device
 */

#include "hidout_backend.h"
#include "esp_log.h"

static const char *TAG = "hidout_ble";

static esp_err_t ble_start(const hidout_config_t *cfg, hidout_rx_cb_t rx, void *rx_ctx)
{
    (void)cfg; (void)rx; (void)rx_ctx;
    ESP_LOGW(TAG, "BLE 后端尚未实现");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t ble_stop(void)      { return ESP_OK; }
static bool      ble_connected(void) { return false; }

static esp_err_t ble_send_report(uint8_t modifiers, const uint8_t keys[6])
{
    (void)modifiers; (void)keys;
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t ble_send_bytes(const uint8_t *data, size_t len)
{
    (void)data; (void)len;
    return ESP_ERR_NOT_SUPPORTED;
}

static const hidout_backend_t s_ble = {
    .name          = "ble",
    .link          = TARGET_LINK_BLE,
    .start         = ble_start,
    .stop          = ble_stop,
    .connected     = ble_connected,
    .send_report   = ble_send_report,
    .send_bytes    = ble_send_bytes,
};

const hidout_backend_t *hidout_backend_ble(void) { return &s_ble; }
