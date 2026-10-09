/*
 * hidout — 把最终文字送到主机
 *
 * ════════════════════════════════════════════════════════════════════════
 *  标准 HID 键盘发的是「键码」不是「字符」。所以 send_key() 发字母数字没问题，
 *  但 send_text("你好") 在 HID 协议层面没有通用做法。
 * ════════════════════════════════════════════════════════════════════════
 *
 * 一期两个目标品牌都得靠设备端接收器，机制完全不同：
 *
 *   reMarkable 1/2  没有蓝牙硬件；平板自己也切不到 USB host 模式(2025.1 起)。
 *                   → 反过来让 TypeFlow 当 USB host，连平板默认的 RNDIS gadget，
 *                     在 usb0 上跑 TCP + TypeFlow 协议 + XOVI 扩展提交文本
 *                   → 系统本身没有任何 CJK 输入法，TypeFlow 是补空缺
 *   BOOX (Android)  → BLE + InputMethodService.commitText()
 *   通用 PC         → HID Unicode 注入序列，各 OS 不同且都要用户配合
 *
 * 哪个品牌支持哪些通道由 system/target 的 profile 决定，不在这里判断。
 * 品牌矩阵与调研依据见 receiver/README.md；线协议见 protocol/PROTOCOL.md。
 */

#ifndef HIDOUT_H
#define HIDOUT_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "esp_err.h"
#include "target.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 连接状态变化回调。断连时候选条应提示用户。 */
typedef void (*hidout_link_cb_t)(bool connected, void *ctx);

typedef struct {
    /* 目标品牌。传输通道和提交机制都从它的 profile 推出来，
     * 不在这里重复指定 —— 避免出现「rM2 + BLE」这种不可能的组合。 */
    target_brand_t brand;

    /* 覆盖首选通道。TARGET_LINK_NONE 表示用 profile 的 links[0]。
     * 会校验该品牌是否真的支持，不支持则返回 ESP_ERR_NOT_SUPPORTED。 */
    target_link_t  link_override;

    const char    *device_name;     /* BLE 广播名 / USB product string */

    /* 接收器协议的接入 token，见 protocol/PROTOCOL.md。
     * 长度 TF_TOKEN_LEN。commit 策略不是 RECEIVER 时忽略。 */
    const uint8_t *token;
} hidout_config_t;

esp_err_t hidout_init(const hidout_config_t *cfg);
esp_err_t hidout_deinit(void);

void hidout_set_link_cb(hidout_link_cb_t cb, void *ctx);

/** @brief 底层链路是否已连接（USB 已枚举 / BLE 已连 / TCP 已建立）。 */
bool hidout_is_connected(void);

/**
 * @brief 接收器是否已握手并声明支持 COMMIT_TEXT。
 *
 * 和 hidout_is_connected() 是两件事：USB 插上了但接收器没装/没跑时，
 * 链路是通的而文本提交能力没有。候选条要据此决定是否退回 ASCII 直通并提示用户。
 */
bool hidout_can_commit_text(void);

/**
 * @brief 提交一段 UTF-8 文本。输入法选好词后调这个。
 *
 * 纯 ASCII 走键码或直接透传；非 ASCII 按当前 profile 的 commit 策略走。
 * @return ESP_ERR_NOT_SUPPORTED 表示当前目标发不了非 ASCII（接收器缺失等）
 */
esp_err_t hidout_send_text(const char *utf8, size_t len);

/**
 * @brief 发一次原始按键（退格、方向键、回车、Tab 等直接透传的键）。
 * @param modifiers HID 修饰键位图 (LCTRL=0x01 LSHIFT=0x02 LALT=0x04 LGUI=0x08)
 * @param keycode   HID Usage Page 0x07 的键码
 */
esp_err_t hidout_send_key(uint8_t modifiers, uint8_t keycode);

/** @brief 运行时切换传输通道。会断开当前连接，并校验品牌是否支持。 */
esp_err_t hidout_switch_link(target_link_t link);

#ifdef __cplusplus
}
#endif

#endif /* HIDOUT_H */
