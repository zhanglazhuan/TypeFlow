#include "monitor.h"
#include "esp_log.h"

static const char *TAG = "monitor";

esp_err_t monitor_init(void)
{
    ESP_LOGI(TAG, "堆/栈/任务水位监控，定期打点便于发现泄漏 — 尚未实现");
    return ESP_OK;
}
