/*
 * t_hello — 最小验证：工具链能编、板子能烧、串口有输出、PSRAM 认到了。
 *
 * 拿到新板子第一个跑的就是它。如果这个都不过，别去查上层代码。
 */

#include <inttypes.h>
#include <stdio.h>
#include "esp_chip_info.h"
#include "esp_psram.h"
#include "esp_system.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

void app_main(void)
{
    esp_chip_info_t info;
    esp_chip_info(&info);

    printf("\n=== TypeFlow t_hello ===\n");
    printf("芯片   : %d 核, revision %d.%d\n",
           info.cores, info.full_revision / 100, info.full_revision % 100);
    printf("内部堆 : %" PRIu32 " 字节\n", esp_get_free_heap_size());

    /* N16R8 应该报出约 8MB。报 0 说明 sdkconfig 里 PSRAM 没开，
     * 或者 SPIRAM_MODE 选成了 QUAD（R8 是八线，见 sdkconfig.defaults）。 */
    size_t psram = esp_psram_get_size();
    printf("PSRAM  : %u 字节 (%u MB)%s\n",
           (unsigned)psram, (unsigned)(psram / (1024 * 1024)),
           psram == 0 ? "  <-- 异常！检查 SPIRAM_MODE_OCT" : "");

    while (1) {
        vTaskDelay(pdMS_TO_TICKS(5000));
        printf("存活, 剩余堆 %" PRIu32 "\n", esp_get_free_heap_size());
    }
}
