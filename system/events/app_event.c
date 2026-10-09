#include "app_event.h"
#include "esp_log.h"

ESP_EVENT_DEFINE_BASE(APP_EVENT);

static const char *TAG = "events";

esp_err_t app_event_init(void)
{
    esp_err_t ret = esp_event_loop_create_default();
    if (ret == ESP_ERR_INVALID_STATE) ret = ESP_OK;  /* 已创建，无害 */
    if (ret == ESP_OK) ESP_LOGI(TAG, "事件总线就绪");
    return ret;
}

esp_err_t app_event_post(app_event_id_t id, const void *data, size_t len)
{
    return esp_event_post(APP_EVENT, (int32_t)id, (void *)data, len, 0);
}
