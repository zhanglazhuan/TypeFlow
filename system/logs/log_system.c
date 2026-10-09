#include "log_system.h"
#include "esp_log.h"

static const char *TAG = "logs";

esp_err_t log_system_init(void)
{
    /* TODO 环形缓冲 + esp_log_set_vprintf 挂钩，参考 DeepStoa\system\logs\log_capture.c */
    ESP_LOGI(TAG, "日志系统就绪（环形缓冲尚未实现）");
    return ESP_OK;
}

int log_system_dump(char *buf, int buf_len)
{
    (void)buf; (void)buf_len;
    return 0;   /* TODO */
}
