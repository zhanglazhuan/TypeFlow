#include "settings.h"
#include "esp_log.h"

static const char *TAG = "settings";

esp_err_t settings_init(void)
{
    ESP_LOGI(TAG, "配置持久化，含 USB/BLE 通道选择与输入法模式 — 尚未实现");
    return ESP_OK;
}
