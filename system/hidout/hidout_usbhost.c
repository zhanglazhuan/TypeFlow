/*
 * hidout USB-host 后端 —— TypeFlow 当 USB host，在平板的 USB gadget 上跑协议
 *
 * ★ 这是 reMarkable 的主通道。角色是反的，值得说清楚为什么：
 *
 *   常规思路是「键盘当 USB device，平板当 host」。但 reMarkable 自 2025.1 起
 *   sysfs 的 USB role 开关被禁，切不到 host 模式，所以它接不了 HID 键盘。
 *
 *   而它作为 USB *device* 的能力一直在：插到 PC 上就是一个 RNDIS 网卡
 *   (10.11.99.1)。所以反过来 —— TypeFlow 去当 host 连它，在 usb0 上跑 TCP，
 *   承载 TypeFlow 协议（见 protocol/PROTOCOL.md）。
 *
 *   这样也顺手解决了 CJK：这条通道根本不是 HID，传的是 UTF-8 文本而不是键码，
 *   键码那道墙不存在。代价是平板上必须装接收器（XOVI 扩展）。
 *
 * TODO 尚未实现。要做的事：
 *   1. 加托管依赖 espressif/iot_usbh_rndis（Espressif 官方维护，支持 S3）。
 *      若平板暴露的是 CDC-ECM 而非 RNDIS，换 espressif/iot_usbh_ecm。
 *   2. 起 USB host 栈，枚举到平板后把它挂成 lwIP netif
 *   3. TCP 连 10.11.99.1:14500，按 PROTOCOL.md 发 HELLO、等 HELLO_ACK
 *   4. 保活 PING/PONG，拔插自动重连
 *
 * ⚠ 两个必须先确认的问题，见 receiver/remarkable/xovi-ext/SPIKE.md：
 *
 *   VBUS：USB host 按规范要给 device 供电。reMarkable 有自己的电池，但作为
 *   device 它需要看到 VBUS 才会枚举。也就是说 TypeFlow 的电池要去点亮平板的
 *   USB 口，甚至给它涓流充电 —— 对一个便携键盘是反过来的负担。硬件上要么加
 *   VBUS 开关只给最小电流，要么用 Y 线/带电 hub 从外部供 5V。
 *   **这条会影响原理图，硬件定稿前要定。**
 *
 *   是否有更简单的 CDC-ACM：Linux 多功能 gadget 通常是 RNDIS + ACM + 存储。
 *   若 reMarkable 也暴露了 ACM 串口，就能用 usb_host_cdc_acm 直接跑串口，
 *   省掉整个 lwIP/TCP 栈，简单得多。用 `lsusb -v` 确认一下再决定走哪条。
 */

#include "hidout_backend.h"
#include "esp_log.h"

static const char *TAG = "hidout_usbh";

static esp_err_t usbh_start(const hidout_config_t *cfg, hidout_rx_cb_t rx, void *rx_ctx)
{
    (void)cfg; (void)rx; (void)rx_ctx;
    ESP_LOGW(TAG, "USB-host 后端尚未实现（需先加托管依赖 espressif/iot_usbh_rndis）");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t usbh_stop(void)      { return ESP_OK; }
static bool      usbh_connected(void) { return false; }

static esp_err_t usbh_send_bytes(const uint8_t *data, size_t len)
{
    (void)data; (void)len;
    return ESP_ERR_NOT_SUPPORTED;
}

static const hidout_backend_t s_usbhost = {
    .name        = "usb-host",
    .link        = TARGET_LINK_USB_HOST,
    .start       = usbh_start,
    .stop        = usbh_stop,
    .connected   = usbh_connected,
    .send_report = NULL,            /* 不是 HID 类后端 */
    .send_bytes  = usbh_send_bytes,
};

const hidout_backend_t *hidout_backend_usbhost(void) { return &s_usbhost; }
