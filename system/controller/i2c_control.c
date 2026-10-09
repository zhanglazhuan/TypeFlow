#include "i2c_control.h"
#include "typeflow_v1.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"

static const char *TAG = "i2c_ctl";

static i2c_master_bus_handle_t s_bus = NULL;

esp_err_t i2c_control_init(void)
{
    if (s_bus) return ESP_OK;

    i2c_master_bus_config_t cfg = {
        .i2c_port   = BSP_I2C_PORT,
        .sda_io_num = BSP_I2C_PIN_SDA,
        .scl_io_num = BSP_I2C_PIN_SCL,
        .clk_source = I2C_CLK_SRC_DEFAULT,
        .glitch_ignore_cnt = 7,
        .flags = {
            /* 内部上拉只有几十 kΩ，400kHz 下偏弱。板上必须有 2.2k~4.7k 外部
             * 上拉；这里开内部上拉只是为了没焊电阻时也能勉强通信便于排查。 */
            .enable_internal_pullup = 1,
        },
    };

    esp_err_t ret = i2c_new_master_bus(&cfg, &s_bus);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "建总线失败: %s", esp_err_to_name(ret));
        return ret;
    }
    ESP_LOGI(TAG, "总线就绪 port=%d SDA=%d SCL=%d",
             BSP_I2C_PORT, BSP_I2C_PIN_SDA, BSP_I2C_PIN_SCL);
    return ESP_OK;
}

i2c_master_bus_handle_t i2c_control_get_bus(void)
{
    return s_bus;
}

void i2c_control_scan(void)
{
    if (!s_bus) { ESP_LOGW(TAG, "总线未初始化"); return; }

    int found = 0;
    for (int addr = 1; addr < 127; addr++) {
        if (i2c_master_probe(s_bus, addr, pdMS_TO_TICKS(20)) == ESP_OK) {
            ESP_LOGI(TAG, "  发现器件 0x%02X", addr);
            found++;
        }
    }
    if (found == 0) {
        ESP_LOGE(TAG, "总线上没有任何器件。依次检查："
                      "SDA/SCL 是否接反、上拉电阻、AW9523 供电、RST 是否还压着低电平");
    }
}
