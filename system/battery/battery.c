#include "battery.h"
#include "esp_log.h"

static const char *TAG = "battery";

esp_err_t battery_init(void)
{
    ESP_LOGI(TAG, "电池电压采样与电量估算（BLE 无线形态必需） — 尚未实现");
    return ESP_OK;
}
