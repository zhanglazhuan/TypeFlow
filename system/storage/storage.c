#include "storage.h"

#include <inttypes.h>
#include <string.h>
#include "esp_log.h"
#include "esp_partition.h"
#include "nvs_flash.h"

static const char *TAG = "storage";

/* dict 分区用自定义 data subtype 0x40，和 partitions.csv 对应 */
#define DICT_PARTITION_SUBTYPE  0x40
#define DICT_PARTITION_LABEL    "dict"

static const esp_partition_t       *s_dict_part = NULL;
static const void                  *s_dict_ptr  = NULL;
static esp_partition_mmap_handle_t  s_dict_mmap;

/* ── 词库映射 ─────────────────────────────────────────────────────────── */

static esp_err_t dict_init(void)
{
    s_dict_part = esp_partition_find_first(ESP_PARTITION_TYPE_DATA,
                                           DICT_PARTITION_SUBTYPE,
                                           DICT_PARTITION_LABEL);
    if (!s_dict_part) {
        ESP_LOGW(TAG, "找不到 dict 分区，输入法将没有候选词");
        return ESP_ERR_NOT_FOUND;
    }

    esp_err_t ret = esp_partition_mmap(s_dict_part, 0, s_dict_part->size,
                                       ESP_PARTITION_MMAP_DATA,
                                       &s_dict_ptr, &s_dict_mmap);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "映射 dict 分区(%" PRIu32 " 字节)失败: %s。"
                      "改成分段映射即可，只需改 storage_dict_map。",
                 s_dict_part->size, esp_err_to_name(ret));
        s_dict_ptr = NULL;
        return ret;
    }

    ESP_LOGI(TAG, "dict 已映射 %" PRIu32 " KB @ %p",
             s_dict_part->size / 1024, s_dict_ptr);
    return ESP_OK;
}

const uint8_t *storage_dict_map(uint32_t offset, uint32_t len)
{
    if (!s_dict_ptr || !s_dict_part) return NULL;
    /* 分开判断避免 offset + len 溢出回绕 */
    if (offset > s_dict_part->size) return NULL;
    if (len > s_dict_part->size - offset) return NULL;
    return (const uint8_t *)s_dict_ptr + offset;
}

uint32_t storage_dict_size(void)
{
    return s_dict_ptr && s_dict_part ? s_dict_part->size : 0;
}

/* ── 用户数据 ─────────────────────────────────────────────────────────── */

int storage_user_load(const char *key, void *buf, int buf_len)
{
    (void)key; (void)buf; (void)buf_len;
    return -1;  /* TODO 挂 littlefs 后从 userdict 分区读 */
}

int storage_user_save(const char *key, const void *buf, int len)
{
    (void)key; (void)buf; (void)len;
    return -1;  /* TODO */
}

/* ── 初始化 ───────────────────────────────────────────────────────────── */

esp_err_t storage_init(void)
{
    esp_err_t ret = nvs_flash_init();
    if (ret == ESP_ERR_NVS_NO_FREE_PAGES || ret == ESP_ERR_NVS_NEW_VERSION_FOUND) {
        ESP_LOGW(TAG, "NVS 需要擦除重建");
        ESP_ERROR_CHECK(nvs_flash_erase());
        ret = nvs_flash_init();
    }
    if (ret != ESP_OK) return ret;

    /* dict 缺失不是致命错误：没有词库也应该能开机、能用 ASCII 模式打字，
     * 这样才有机会通过 OTA 或工具把词库刷进去。 */
    dict_init();

    /* TODO 挂载 userdict (littlefs)。需要托管依赖 joltwallet/littlefs，
     * 或改用 IDF 自带的 spiffs（但 spiffs 在小分区上写放大更严重）。 */
    return ESP_OK;
}
