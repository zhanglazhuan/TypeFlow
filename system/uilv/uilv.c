#include "uilv.h"
#include "esp_log.h"

static const char *TAG = "uilv";

esp_err_t uilv_init(void)
{
    /* TODO 接 LVGL，步骤见 uilv.h 文件头 */
    ESP_LOGI(TAG, "UI 层占位（LVGL 尚未引入）");
    return ESP_OK;
}
