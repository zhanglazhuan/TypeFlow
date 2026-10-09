/*
 * target — 品牌 profile 注册表
 *
 * 目前三个品牌的差异还只是静态数据，所以放在一个文件里。等出现真正的品牌专属
 * 逻辑时（比如 Paper Pro 要先 modprobe btnxpuart、BOOX 要协商 ATT MTU），
 * 再拆成 target_remarkable.c / target_boox.c。
 */

#include "target.h"
#include "esp_log.h"

static const char *TAG = "target";

/* ════════════════════════════════════════════════════════════════════════
 * reMarkable 1 / 2
 * ════════════════════════════════════════════════════════════════════════
 * 硬约束（调研见 receiver/README.md）：
 *   - 没有蓝牙硬件，BLE 完全不可能，别在 UI 上给用户这个选项
 *   - 2025.1 起 sysfs 的 USB role 开关被禁，平板切不到 host，所以它接不了 HID 键盘
 *   - 但它作为 USB *device* 一直是个 RNDIS 网卡（插 PC 就是 10.11.99.1）。
 *     所以反过来：TypeFlow 去当 host 连它，在 usb0 上跑 TCP + TypeFlow 协议。
 *   - 系统没有任何 CJK 输入法，Type Folio 只有英/法/德/北欧布局
 * -------------------------------------------------------------------- */
static const target_profile_t s_remarkable = {
    .brand  = TARGET_BRAND_REMARKABLE,
    .name   = "reMarkable 1/2",
    .links  = { TARGET_LINK_USB_HOST, TARGET_LINK_TYPEFOLIO, TARGET_LINK_NONE },
    .commit = TARGET_COMMIT_RECEIVER,
    .needs_receiver = true,
    .os_has_cjk_ime = false,
    .hint   = "需在设备上安装 TypeFlow 接收器 (XOVI 扩展)",
};

/* Paper Pro / Pro Move / Pure：多了蓝牙硬件，但驱动默认不加载
 * (modprobe btnxpuart)，且电池供电下会被省电策略断开。所以 BLE 排在 USB 之后。 */
static const target_profile_t s_remarkable_pp = {
    .brand  = TARGET_BRAND_REMARKABLE_PP,
    .name   = "reMarkable Paper Pro",
    .links  = { TARGET_LINK_USB_HOST, TARGET_LINK_BLE, TARGET_LINK_TYPEFOLIO },
    .commit = TARGET_COMMIT_RECEIVER,
    .needs_receiver = true,
    .os_has_cjk_ime = false,
    .hint   = "需安装接收器；蓝牙需先 modprobe btnxpuart",
};

/* ════════════════════════════════════════════════════════════════════════
 * BOOX（Android / Onyx）
 * ════════════════════════════════════════════════════════════════════════
 * Android 有官方支持的提交路径：InputMethodService.commitText()。
 * 系统自带中文输入法，所以 TypeFlow 的价值是「候选词不占墨水屏、不吃刷新」，
 * 而不是「让它能打中文」—— 这点和 reMarkable 完全不同。
 * -------------------------------------------------------------------- */
static const target_profile_t s_boox = {
    .brand  = TARGET_BRAND_BOOX,
    .name   = "BOOX (Android)",
    .links  = { TARGET_LINK_BLE, TARGET_LINK_USB_HID, TARGET_LINK_NONE },
    .commit = TARGET_COMMIT_RECEIVER,
    .needs_receiver = true,
    .os_has_cjk_ime = true,
    .hint   = "需安装 TypeFlow 输入法并在系统设置里启用",
};

/* ════════════════════════════════════════════════════════════════════════
 * 通用 PC —— 开发调试主力，不需要装任何东西
 * ════════════════════════════════════════════════════════════════════════
 * 走 HID Unicode 注入序列，各 OS 不同且都要用户配合（见 hidout.h）。
 * 但 ASCII 部分零配置可用，所以是 bring-up 阶段最方便的目标。
 * -------------------------------------------------------------------- */
static const target_profile_t s_generic_pc = {
    .brand  = TARGET_BRAND_GENERIC_PC,
    .name   = "通用 PC",
    .links  = { TARGET_LINK_USB_HID, TARGET_LINK_BLE, TARGET_LINK_NONE },
    .commit = TARGET_COMMIT_HID_UNICODE,
    .needs_receiver = false,
    .os_has_cjk_ime = true,
    .hint   = "中文需主机侧 Unicode 注入配置，见 hidout.h",
};

static const target_profile_t *const s_registry[TARGET_BRAND_COUNT] = {
    [TARGET_BRAND_GENERIC_PC]    = &s_generic_pc,
    [TARGET_BRAND_REMARKABLE]    = &s_remarkable,
    [TARGET_BRAND_REMARKABLE_PP] = &s_remarkable_pp,
    [TARGET_BRAND_BOOX]          = &s_boox,
};

static const target_profile_t *s_current = &s_generic_pc;

const target_profile_t *target_get(target_brand_t brand)
{
    if (brand < 0 || brand >= TARGET_BRAND_COUNT) return NULL;
    return s_registry[brand];
}

const target_profile_t *target_current(void)
{
    return s_current;
}

esp_err_t target_set(target_brand_t brand)
{
    const target_profile_t *p = target_get(brand);
    if (!p) return ESP_ERR_INVALID_ARG;

    s_current = p;
    ESP_LOGI(TAG, "目标切换为 %s（首选通道 %d，%s）",
             p->name, (int)target_preferred_link(p),
             p->needs_receiver ? "需要接收器" : "无需接收器");
    /* TODO 经 settings 持久化到 NVS */
    return ESP_OK;
}

bool target_supports_link(const target_profile_t *p, target_link_t link)
{
    if (!p || link == TARGET_LINK_NONE) return false;
    for (int i = 0; i < TARGET_MAX_LINKS; i++) {
        if (p->links[i] == TARGET_LINK_NONE) break;
        if (p->links[i] == link) return true;
    }
    return false;
}

target_link_t target_preferred_link(const target_profile_t *p)
{
    return p ? p->links[0] : TARGET_LINK_NONE;
}
