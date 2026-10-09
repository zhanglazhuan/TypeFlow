/*
 * TypeFlow — app_main
 *
 * 这里只做初始化编排，不放业务逻辑。初始化顺序是有依赖的，别随意调换：
 *   logs    -> 最先，后面所有模块的日志才有地方去
 *   events  -> 事件总线，模块间解耦靠它
 *   storage -> 必须在 candbar 之前：ime 创建时要拿词库来源
 *   display -> 必须在 uilv 之前：uilv 要绑定面板 handle
 *   hidout  -> 必须在 candbar 之前：选词后要有地方发
 *   input   -> 最后，键盘一通就会开始上报事件，此时下游必须都已就绪
 */

#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

#include "log_system.h"
#include "app_event.h"
#include "monitor.h"
#include "storage.h"
#include "display_control.h"
#include "uilv.h"
#include "target.h"
#include "hidout.h"
#include "input.h"
#include "candbar.h"

static const char *TAG = "main";

/* input 的回调：直接转给候选条 */
static void on_key(const input_event_t *ev, void *ctx)
{
    (void)ctx;
    candbar_handle_key(ev);
}

void app_main(void)
{
    ESP_LOGI(TAG, "TypeFlow 启动");

    ESP_ERROR_CHECK(log_system_init());
    ESP_ERROR_CHECK(app_event_init());
    ESP_ERROR_CHECK(monitor_init());
    ESP_ERROR_CHECK(storage_init());

    ESP_ERROR_CHECK(display_control_init());
    ESP_ERROR_CHECK(uilv_init());

    /* TODO 目标品牌应当从 settings 读，这里先写死便于 bring-up。
     *
     * 一期两个目标品牌都需要设备端接收器（见 receiver/README.md 品牌矩阵）：
     *   TARGET_BRAND_REMARKABLE → USB(TypeFlow 当 host) + XOVI 扩展（rM1/rM2 无蓝牙）
     *   TARGET_BRAND_BOOX       → BLE + Android 输入法 commitText()
     * 默认先用 GENERIC_PC：ASCII 零配置可用，调键盘和输入法内核时干扰最小。 */
    static const uint8_t k_token[16] = { 0 };   /* TODO 配对时生成并存 NVS */
    hidout_config_t hid = {
        .brand         = TARGET_BRAND_GENERIC_PC,
        .link_override = TARGET_LINK_NONE,      /* 用 profile 的首选通道 */
        .device_name   = "TypeFlow",
        .token         = k_token,
    };
    /* 后端尚未实现，会返回 NOT_SUPPORTED。不做 ESP_ERROR_CHECK，否则整机起不来，
     * 键盘扫描和输入法内核就没法单独调试了。 */
    hidout_init(&hid);

    ESP_ERROR_CHECK(candbar_init());
    ESP_ERROR_CHECK(input_init(on_key, NULL));

    ESP_LOGI(TAG, "初始化完成");
}
