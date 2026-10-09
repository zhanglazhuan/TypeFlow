/*
 * app_event — 模块间事件总线
 *
 * 移植自 DeepStoa\system\events。用途：让 input -> candbar -> hidout 之间
 * 不需要互相 #include。键盘这条链路上事件量很小，直接用 esp_event 默认循环即可。
 */
#ifndef APP_EVENT_H
#define APP_EVENT_H

#include "esp_err.h"
#include "esp_event.h"

#ifdef __cplusplus
extern "C" {
#endif

ESP_EVENT_DECLARE_BASE(APP_EVENT);

typedef enum {
    APP_EVT_KEY,              /* 语义按键，data = input_event_t */
    APP_EVT_IME_UPDATED,      /* 编码串或候选变了，candbar 该重绘 */
    APP_EVT_TEXT_COMMITTED,   /* 选定了候选，data = UTF-8 文本 */
    APP_EVT_LINK_CHANGED,     /* USB/BLE 连接状态变化 */
    APP_EVT_BATTERY_LOW,
} app_event_id_t;

esp_err_t app_event_init(void);
esp_err_t app_event_post(app_event_id_t id, const void *data, size_t len);

#ifdef __cplusplus
}
#endif

#endif /* APP_EVENT_H */
