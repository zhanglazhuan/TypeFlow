/*
 * AW9523B — I2C GPIO Expander Driver Implementation
 *
 * 寄存器语义以 AW9523B 数据手册和 Linux 主线驱动 drivers/gpio/gpio-aw9523.c 为准。
 */

#include "aw9523.h"

#include <inttypes.h>
#include <stdlib.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"

static const char *TAG = "aw9523";

/* ── 寄存器映射 ───────────────────────────────────────────────────────── */
#define REG_INPUT_P0    0x00
#define REG_OUTPUT_P0   0x02
#define REG_DIR_P0      0x04    /* 1 = 输入, 0 = 输出 */
#define REG_INT_P0      0x06    /* 0 = 使能中断, 1 = 屏蔽 (反逻辑) */
#define REG_ID          0x10
#define REG_GCR         0x11    /* bit4: 1 = P0 推挽, 0 = P0 开漏 */
#define REG_LEDMODE_P0  0x12    /* 0 = LED 恒流, 1 = GPIO (复位默认 0xFF) */
#define REG_LEDMODE_P1  0x13
#define REG_SOFT_RESET  0x7F

#define GCR_P0_PUSH_PULL    (1 << 4)

#define I2C_TIMEOUT_MS  100

struct aw9523_dev_t {
    i2c_master_dev_handle_t dev;
    uint8_t out_cache[2];   /* 输出寄存器影子，避免 set_pin 每次都先读 */
    uint8_t dir_cache[2];
};

/* ── 寄存器读写 ───────────────────────────────────────────────────────── */

static esp_err_t read_reg(aw9523_handle_t h, uint8_t reg, uint8_t *val)
{
    return i2c_master_transmit_receive(h->dev, &reg, 1, val, 1, I2C_TIMEOUT_MS);
}

static esp_err_t write_reg(aw9523_handle_t h, uint8_t reg, uint8_t val)
{
    uint8_t buf[2] = { reg, val };
    return i2c_master_transmit(h->dev, buf, 2, I2C_TIMEOUT_MS);
}

/* ── 硬复位 ───────────────────────────────────────────────────────────── */

static void hw_reset(gpio_num_t rst_pin)
{
    gpio_config_t cfg = {
        .pin_bit_mask = (1ULL << rst_pin),
        .mode         = GPIO_MODE_OUTPUT,
        .pull_up_en   = GPIO_PULLUP_DISABLE,
        .pull_down_en = GPIO_PULLDOWN_DISABLE,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    gpio_config(&cfg);

    gpio_set_level(rst_pin, 1);
    vTaskDelay(pdMS_TO_TICKS(5));
    gpio_set_level(rst_pin, 0);     /* 低有效，拉低复位 */
    vTaskDelay(pdMS_TO_TICKS(10));
    gpio_set_level(rst_pin, 1);     /* 释放 */
    vTaskDelay(pdMS_TO_TICKS(10));  /* 等芯片启动 */
}

/* ── 生命周期 ─────────────────────────────────────────────────────────── */

esp_err_t aw9523_new(i2c_master_bus_handle_t bus, const aw9523_config_t *cfg,
                     aw9523_handle_t *out)
{
    if (!bus || !cfg || !out) return ESP_ERR_INVALID_ARG;

    if (cfg->rst_pin != GPIO_NUM_NC) {
        hw_reset(cfg->rst_pin);
    }

    aw9523_handle_t h = calloc(1, sizeof(struct aw9523_dev_t));
    if (!h) return ESP_ERR_NO_MEM;

    i2c_device_config_t dev_cfg = {
        .dev_addr_length = I2C_ADDR_BIT_LEN_7,
        .device_address  = cfg->i2c_addr,
        .scl_speed_hz    = cfg->scl_speed_hz ? cfg->scl_speed_hz : 400000,
    };
    esp_err_t ret = i2c_master_bus_add_device(bus, &dev_cfg, &h->dev);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "加入 I2C 总线失败 addr=0x%02X: %s",
                 cfg->i2c_addr, esp_err_to_name(ret));
        free(h);
        return ret;
    }

    /* 校验 chip ID —— 读不到通常是没上电、还在复位、或 SDA/SCL 接反 */
    uint8_t id = 0;
    ret = read_reg(h, REG_ID, &id);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "读 chip ID 失败 addr=0x%02X: %s。"
                      "检查供电、RST 是否释放、SDA/SCL 是否接反、有无上拉电阻。",
                 cfg->i2c_addr, esp_err_to_name(ret));
        goto fail;
    }
    if (id != AW9523_CHIP_ID) {
        ESP_LOGE(TAG, "chip ID 是 0x%02X，期望 0x%02X —— 地址 0x%02X 上可能是别的器件",
                 id, AW9523_CHIP_ID, cfg->i2c_addr);
        ret = ESP_ERR_NOT_FOUND;
        goto fail;
    }

    /* ── 安全默认值 ──
     * 顺序有讲究：先关中断、再全设为输入，最后才配置输出级形态。
     * 反过来做会在配置窗口内短暂驱动未知负载。 */
    ESP_ERROR_CHECK_WITHOUT_ABORT(write_reg(h, REG_INT_P0 + 0, 0xFF)); /* 全屏蔽 */
    ESP_ERROR_CHECK_WITHOUT_ABORT(write_reg(h, REG_INT_P0 + 1, 0xFF));
    ESP_ERROR_CHECK_WITHOUT_ABORT(write_reg(h, REG_DIR_P0 + 0, 0xFF)); /* 全输入 */
    ESP_ERROR_CHECK_WITHOUT_ABORT(write_reg(h, REG_DIR_P0 + 1, 0xFF));
    h->dir_cache[0] = h->dir_cache[1] = 0xFF;
    h->out_cache[0] = h->out_cache[1] = 0x00;

    /* 0xFF = GPIO 模式，0x00 = LED 恒流吸收模式。极性容易记反：
     * DeepStoa\drivers\aw9523\aw9523.c 这里写的是 0x00 并注释成 "GPIO mode"，
     * 实际把 16 个脚全切进了 LED 模式。键盘矩阵下这会导致列脚读不出正确电平。
     * 依据：Linux 主线 gpio-aw9523.h — "LED mode switch. 0: LED, 1: GPIO"，
     * 且该寄存器复位默认值就是 0xFF。 */
    ESP_ERROR_CHECK_WITHOUT_ABORT(write_reg(h, REG_LEDMODE_P0, 0xFF));
    ESP_ERROR_CHECK_WITHOUT_ABORT(write_reg(h, REG_LEDMODE_P1, 0xFF));

    /* P0 默认推挽（键盘驱动行需要推挽，开漏拉不起高电平） */
    ESP_ERROR_CHECK_WITHOUT_ABORT(write_reg(h, REG_GCR, GCR_P0_PUSH_PULL));

    ESP_LOGI(TAG, "就绪 addr=0x%02X id=0x%02X speed=%" PRIu32 "Hz",
             cfg->i2c_addr, id, dev_cfg.scl_speed_hz);
    *out = h;
    return ESP_OK;

fail:
    i2c_master_bus_rm_device(h->dev);
    free(h);
    return ret;
}

esp_err_t aw9523_del(aw9523_handle_t h)
{
    if (!h) return ESP_ERR_INVALID_ARG;
    esp_err_t ret = i2c_master_bus_rm_device(h->dev);
    free(h);
    return ret;
}

/* ── 基本操作 ─────────────────────────────────────────────────────────── */

esp_err_t aw9523_get_chip_id(aw9523_handle_t h, uint8_t *id)
{
    if (!h || !id) return ESP_ERR_INVALID_ARG;
    return read_reg(h, REG_ID, id);
}

esp_err_t aw9523_soft_reset(aw9523_handle_t h)
{
    if (!h) return ESP_ERR_INVALID_ARG;
    esp_err_t ret = write_reg(h, REG_SOFT_RESET, 0x00);
    if (ret == ESP_OK) {
        /* 寄存器回到上电默认：全输入、LED mode 全 GPIO、影子缓存要跟着重置 */
        h->dir_cache[0] = h->dir_cache[1] = 0xFF;
        h->out_cache[0] = h->out_cache[1] = 0x00;
    }
    return ret;
}

esp_err_t aw9523_set_port_dir(aw9523_handle_t h, uint8_t port, uint8_t dir_mask)
{
    if (!h || port > 1) return ESP_ERR_INVALID_ARG;
    esp_err_t ret = write_reg(h, REG_DIR_P0 + port, dir_mask);
    if (ret == ESP_OK) h->dir_cache[port] = dir_mask;
    return ret;
}

esp_err_t aw9523_write_port(aw9523_handle_t h, uint8_t port, uint8_t value)
{
    if (!h || port > 1) return ESP_ERR_INVALID_ARG;
    esp_err_t ret = write_reg(h, REG_OUTPUT_P0 + port, value);
    if (ret == ESP_OK) h->out_cache[port] = value;
    return ret;
}

esp_err_t aw9523_read_port(aw9523_handle_t h, uint8_t port, uint8_t *value)
{
    if (!h || port > 1 || !value) return ESP_ERR_INVALID_ARG;
    return read_reg(h, REG_INPUT_P0 + port, value);
}

esp_err_t aw9523_set_pin(aw9523_handle_t h, uint8_t pin, uint8_t level)
{
    if (!h) return ESP_ERR_INVALID_ARG;
    uint8_t port = AW9523_GET_PORT(pin);
    uint8_t bit  = AW9523_GET_BIT(pin);

    uint8_t next = level ? (uint8_t)(h->out_cache[port] |  (1 << bit))
                         : (uint8_t)(h->out_cache[port] & ~(1 << bit));
    return aw9523_write_port(h, port, next);
}

esp_err_t aw9523_get_pin(aw9523_handle_t h, uint8_t pin, uint8_t *level)
{
    if (!h || !level) return ESP_ERR_INVALID_ARG;
    uint8_t port = AW9523_GET_PORT(pin);
    uint8_t bit  = AW9523_GET_BIT(pin);

    uint8_t val = 0;
    esp_err_t ret = read_reg(h, REG_INPUT_P0 + port, &val);
    if (ret == ESP_OK) *level = (val >> bit) & 0x01;
    return ret;
}

esp_err_t aw9523_set_p0_push_pull(aw9523_handle_t h, bool push_pull)
{
    if (!h) return ESP_ERR_INVALID_ARG;
    uint8_t gcr = 0;
    esp_err_t ret = read_reg(h, REG_GCR, &gcr);
    if (ret != ESP_OK) return ret;

    gcr = push_pull ? (uint8_t)(gcr |  GCR_P0_PUSH_PULL)
                    : (uint8_t)(gcr & ~GCR_P0_PUSH_PULL);
    return write_reg(h, REG_GCR, gcr);
}

esp_err_t aw9523_set_port_int_enable(aw9523_handle_t h, uint8_t port,
                                     uint8_t enable_mask)
{
    if (!h || port > 1) return ESP_ERR_INVALID_ARG;
    /* 寄存器是反逻辑：0 使能、1 屏蔽。对外接口用正逻辑，这里取反。 */
    return write_reg(h, REG_INT_P0 + port, (uint8_t)~enable_mask);
}
