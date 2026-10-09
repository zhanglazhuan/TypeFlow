#include "ota.h"
#include "esp_log.h"

static const char *TAG = "ota";

esp_err_t ota_init(void)
{
    ESP_LOGI(TAG, "固件升级，双 OTA 分区切换 — 尚未实现");
    return ESP_OK;
}
