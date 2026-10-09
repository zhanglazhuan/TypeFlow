/*
 * target — 目标设备品牌归类
 *
 * 为什么需要这一层：一期两个目标品牌的系统完全不同，传输通道和文本提交机制
 * 都不通用，而且各有硬约束（reMarkable 1/2 根本没有蓝牙）。把这些差异集中成
 * 一张 profile 表，调用方只问「当前目标支持什么」，不散落 if (品牌 == ...)。
 *
 * 各品牌的调研结论和取舍依据见 receiver/README.md 的品牌矩阵。
 */

#ifndef TARGET_H
#define TARGET_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ── 传输通道 ─────────────────────────────────────────────────────────────
 * 只有 USB 和 BLE 两类物理通道（键盘不该联 Wi-Fi）。但 USB 有两种角色，
 * 区别很关键，因为它直接影响硬件设计：
 *
 *   USB_HID   TypeFlow 是 USB *device*，平板是 host。发标准 HID 键码。
 *             BOOX 走这条（Android OTG 能当 host）。
 *   USB_HOST  TypeFlow 是 USB *host*，平板保持它默认的 device 角色。
 *             reMarkable 必须走这条 —— 它的 USB host 模式 2025.1 起被禁，
 *             但它自己作为 device 一直是个 RNDIS 网卡，所以我们去当 host
 *             连它，在上面跑 TCP + TypeFlow 协议。
 *
 * ⚠ ESP32-S3 只有一个 USB-OTG 外设，两种角色不能同时存在，只能按目标品牌切。
 *   硬件上 USB-C 口必须支持双角色（VBUS 可供可取），见 boards/ 说明。
 * ---------------------------------------------------------------------- */
typedef enum {
    TARGET_LINK_NONE = 0,
    TARGET_LINK_USB_HID,    /* TypeFlow=device，发 HID 键码（发不了 CJK） */
    TARGET_LINK_USB_HOST,   /* TypeFlow=host，跑 TypeFlow 协议（可发 CJK） */
    TARGET_LINK_BLE,        /* BLE，可跑 HID 或自定义 GATT */
    TARGET_LINK_TYPEFOLIO,  /* reMarkable pogo 针 + UART，模拟 Type Folio */
} target_link_t;

/** @brief 非 ASCII 文本怎么提交。 */
typedef enum {
    TARGET_COMMIT_NONE = 0,     /* 发不了非 ASCII */
    TARGET_COMMIT_RECEIVER,     /* 走 TypeFlow 协议交给设备端接收器 */
    TARGET_COMMIT_HID_UNICODE,  /* 发 OS 相关 Unicode 注入序列（仅 PC） */
} target_commit_t;

typedef enum {
    TARGET_BRAND_GENERIC_PC = 0,
    TARGET_BRAND_REMARKABLE,        /* rM1 / rM2 */
    TARGET_BRAND_REMARKABLE_PP,     /* Paper Pro / Pro Move / Pure */
    TARGET_BRAND_BOOX,
    TARGET_BRAND_COUNT,
} target_brand_t;

#define TARGET_MAX_LINKS 3

typedef struct {
    target_brand_t   brand;
    const char      *name;

    /* 可用传输，按推荐优先级排列；TARGET_LINK_NONE 结束 */
    target_link_t    links[TARGET_MAX_LINKS];

    target_commit_t  commit;

    /* 要不要在候选条上提示用户装接收器。没接收器就只能打英文。 */
    bool             needs_receiver;

    /* 系统自身有没有 CJK 输入法。没有的话 TypeFlow 是补空缺而不是做替代，
     * 这决定了产品话术，也决定接收器缺失时该给什么提示。 */
    bool             os_has_cjk_ime;

    /* 给用户看的一句话说明，接收器连不上时显示在候选条 */
    const char      *hint;
} target_profile_t;

/** @brief 取某品牌的 profile。 */
const target_profile_t *target_get(target_brand_t brand);

/** @brief 取当前选定的 profile。默认 GENERIC_PC。 */
const target_profile_t *target_current(void);

/** @brief 切换当前目标品牌。会从 NVS 读写持久化（由 settings 负责）。 */
esp_err_t target_set(target_brand_t brand);

/** @brief 该品牌是否支持某传输。用来拦住「rM2 上开蓝牙」这类不可能的组合。 */
bool target_supports_link(const target_profile_t *p, target_link_t link);

/** @brief 首选传输（links[0]）。 */
target_link_t target_preferred_link(const target_profile_t *p);

#ifdef __cplusplus
}
#endif

#endif /* TARGET_H */
