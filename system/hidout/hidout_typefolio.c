/*
 * hidout Type Folio 后端 —— reMarkable pogo 针 + UART，模拟官方键盘配件
 *
 * 用途：ASCII 零安装保底。设备端不用装任何软件，插上就能打英文 ——
 * bring-up 阶段和「用户还没装接收器」时的退路。
 *
 * ⚠ 只能发键码，发不了 CJK。中文必须走 USB-host 通道 + 接收器。
 *
 * TODO 尚未实现。前人方案：
 *   jonasoberschweiber/remarkable-keyboard-adapter (RP2040)
 *   经 reMarkable 的磁吸配件口，UART 协议，需要先发命令完成握手。
 *   电气部分它引用了 Dudlushka 的项目。
 *
 * 实现前要确认的事：
 *   1. 握手报文的确切格式（抓上面那个项目的 UART 波形，或读它的源码）
 *   2. pogo 针的机械与电气接口 —— 这会影响 TypeFlow 的外壳设计，
 *      要在硬件定稿前决定到底做不做这条通道
 *   3. Type Folio 只有英/法/德/北欧布局，扫描码到字符的映射要照它的表
 */

#include "hidout_backend.h"
#include "esp_log.h"

static const char *TAG = "hidout_tf";

static esp_err_t tf_start(const hidout_config_t *cfg, hidout_rx_cb_t rx, void *rx_ctx)
{
    (void)cfg; (void)rx; (void)rx_ctx;
    ESP_LOGW(TAG, "Type Folio 后端尚未实现（需先确认握手协议与 pogo 针接口）");
    return ESP_ERR_NOT_SUPPORTED;
}

static esp_err_t tf_stop(void)      { return ESP_OK; }
static bool      tf_connected(void) { return false; }

static esp_err_t tf_send_report(uint8_t modifiers, const uint8_t keys[6])
{
    (void)modifiers; (void)keys;
    return ESP_ERR_NOT_SUPPORTED;
}

static const hidout_backend_t s_typefolio = {
    .name        = "typefolio",
    .link        = TARGET_LINK_TYPEFOLIO,
    .start       = tf_start,
    .stop        = tf_stop,
    .connected   = tf_connected,
    .send_report = tf_send_report,
    .send_bytes  = NULL,            /* 只能发键码 */
};

const hidout_backend_t *hidout_backend_typefolio(void) { return &s_typefolio; }
