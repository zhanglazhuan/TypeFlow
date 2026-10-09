/*
 * hidout 后端内部接口 —— 只给 hidout.c 和各后端实现用，不对外暴露。
 *
 * 后端分两类，一个后端可以只实现一类：
 *   HID 类      实现 send_report，发标准 8 字节键盘报告（USB HID / BLE HID）
 *   接收器类    实现 send_bytes，收发 TypeFlow 协议字节流（USB-host / BLE GATT）
 *
 * 协议组帧统一在 hidout.c 里用 tf_encode 做，后端只负责搬字节 ——
 * 否则每加一个传输就要重写一遍组帧，还容易和 PROTOCOL.md 走偏。
 */
#ifndef HIDOUT_BACKEND_H
#define HIDOUT_BACKEND_H

#include "hidout.h"

/** @brief 后端收到字节后回调给 hidout.c 做帧解析。 */
typedef void (*hidout_rx_cb_t)(const uint8_t *data, size_t len, void *ctx);

typedef struct {
    const char   *name;
    target_link_t link;

    esp_err_t (*start)(const hidout_config_t *cfg, hidout_rx_cb_t rx, void *rx_ctx);
    esp_err_t (*stop)(void);
    bool      (*connected)(void);

    /* HID 类后端实现：modifiers + 最多 6 个同时按下的键码 */
    esp_err_t (*send_report)(uint8_t modifiers, const uint8_t keys[6]);

    /* 接收器类后端实现：发送已组好帧的字节 */
    esp_err_t (*send_bytes)(const uint8_t *data, size_t len);
} hidout_backend_t;

const hidout_backend_t *hidout_backend_usb(void);
const hidout_backend_t *hidout_backend_ble(void);
const hidout_backend_t *hidout_backend_usbhost(void);
const hidout_backend_t *hidout_backend_typefolio(void);

#endif /* HIDOUT_BACKEND_H */
