#include "sleep.h"
#include "esp_log.h"

static const char *TAG = "sleep";

esp_err_t sleep_init(void)
{
    ESP_LOGI(TAG, "低功耗：无按键时进 light sleep，靠 AW9523 INT 唤醒 — 尚未实现");
    return ESP_OK;
}
