/*
 * kbmatrix — 键盘矩阵扫描实现
 */

#include "kbmatrix.h"

#include <stdlib.h>
#include <string.h>
#include "driver/gpio.h"
#include "esp_log.h"
#include "freertos/FreeRTOS.h"
#include "freertos/task.h"
#include "freertos/semphr.h"

static const char *TAG = "kbmatrix";

#define SCAN_TASK_STACK     3072
#define SCAN_TASK_PRIO      5
#define IDLE_POLL_MS        50      /* 无 INT 脚时的退化轮询周期 */

typedef struct {
    kbmatrix_config_t cfg;
    kbmatrix_cb_t     cb;
    void             *ctx;

    uint16_t stable[KBMATRIX_MAX_ROWS];     /* 已确认的稳定状态 */
    uint16_t pending[KBMATRIX_MAX_ROWS];    /* 读到但还没过消抖的状态 */
    uint8_t  settle[KBMATRIX_MAX_ROWS];     /* 每行连续保持 pending 的轮数 */

    TaskHandle_t      task;
    SemaphoreHandle_t wake;                 /* INT 中断 -> 扫描任务 */
    volatile bool     running;
} kbm_t;

static kbm_t *g_kbm = NULL;

/* ── INT 中断：只负责唤醒，I2C 不能在 ISR 里做 ───────────────────────── */

static void IRAM_ATTR int_isr(void *arg)
{
    kbm_t *k = (kbm_t *)arg;
    xSemaphoreGiveFromISR(k->wake, NULL);
}

/* ── 读一轮原始矩阵 ───────────────────────────────────────────────────── */

static esp_err_t read_cols(kbm_t *k, uint16_t *out)
{
    uint16_t v = 0;
    for (uint8_t i = 0; i < k->cfg.col_src_count; i++) {
        uint8_t byte = 0;
        esp_err_t ret = aw9523_read_port(k->cfg.col_src[i].dev,
                                         k->cfg.col_src[i].port, &byte);
        if (ret != ESP_OK) return ret;
        v |= ((uint16_t)byte) << (8 * i);
    }
    /* 列线常态被上拉为高，按下接地为低：取反后 1 = 按下 */
    *out = (uint16_t)~v;
    return ESP_OK;
}

static esp_err_t scan_raw(kbm_t *k, uint16_t *state)
{
    const uint16_t col_mask = (k->cfg.cols >= 16)
                            ? 0xFFFF
                            : (uint16_t)((1u << k->cfg.cols) - 1u);

    for (uint8_t r = 0; r < k->cfg.rows; r++) {
        /* 只把当前行拉低，其余行保持高。行端口是推挽输出。 */
        uint8_t row_val = (uint8_t)~(1u << r);
        esp_err_t ret = aw9523_write_port(k->cfg.row_src.dev,
                                          k->cfg.row_src.port, row_val);
        if (ret != ESP_OK) return ret;

        /* 行线建立时间：一次 I2C 写本身就要数十微秒，通常够用。
         * 若列线上拉偏大(>10k)且并联电容明显，这里要补 ets_delay_us()。 */

        uint16_t cols = 0;
        ret = read_cols(k, &cols);
        if (ret != ESP_OK) return ret;

        state[r] = cols & col_mask;
    }

    /* 收尾把所有行置高：否则空闲时被按住的键会一直把列线拉低，屏蔽掉 INT */
    return aw9523_write_port(k->cfg.row_src.dev, k->cfg.row_src.port, 0xFF);
}

/* ── 鬼键检测 ─────────────────────────────────────────────────────────────
 * 无二极管矩阵下，若 (r1,c1) (r1,c2) (r2,c1) 三键同时按下，硬件会让 (r2,c2)
 * 也表现为按下，且无法从电气上区分真假。策略：检测到这种矩形就整轮丢弃，
 * 宁可漏报也不误输出 —— 输入法里凭空多一个字比丢一次按键更难被用户察觉。
 * ------------------------------------------------------------------------ */
static bool has_ghost(const kbm_t *k, const uint16_t *state)
{
    for (uint8_t a = 0; a < k->cfg.rows; a++) {
        if (__builtin_popcount(state[a]) < 2) continue;
        for (uint8_t b = (uint8_t)(a + 1); b < k->cfg.rows; b++) {
            /* 两行共享至少两列即构成矩形 */
            if (__builtin_popcount((uint16_t)(state[a] & state[b])) >= 2) {
                return true;
            }
        }
    }
    return false;
}

/* ── 消抖并派发事件 ───────────────────────────────────────────────────── */

static void debounce_and_emit(kbm_t *k, const uint16_t *raw)
{
    const uint8_t need = (k->cfg.debounce_ms && k->cfg.scan_interval_ms)
                       ? (uint8_t)((k->cfg.debounce_ms / k->cfg.scan_interval_ms) + 1)
                       : 2;

    for (uint8_t r = 0; r < k->cfg.rows; r++) {
        if (raw[r] != k->pending[r]) {
            k->pending[r] = raw[r];
            k->settle[r]  = 1;
            continue;
        }
        if (k->settle[r] < need) {
            k->settle[r]++;
            continue;
        }
        /* 该行状态已稳定，与 stable 求差集后派发 */
        uint16_t diff = (uint16_t)(k->stable[r] ^ raw[r]);
        if (!diff) continue;

        for (uint8_t c = 0; c < k->cfg.cols; c++) {
            if (!(diff & (1u << c))) continue;
            kbmatrix_event_t ev = {
                .row     = r,
                .col     = c,
                .pressed = (raw[r] & (1u << c)) != 0,
            };
            if (k->cb) k->cb(&ev, k->ctx);
        }
        k->stable[r] = raw[r];
    }
}

static bool any_pressed(const kbm_t *k)
{
    for (uint8_t r = 0; r < k->cfg.rows; r++) {
        if (k->stable[r] || k->pending[r]) return true;
    }
    return false;
}

/* ── 扫描任务 ─────────────────────────────────────────────────────────── */

static void scan_task(void *arg)
{
    kbm_t *k = (kbm_t *)arg;
    uint16_t raw[KBMATRIX_MAX_ROWS] = {0};

    while (k->running) {
        /* 空闲时阻塞等 INT；有键按下时按 scan_interval_ms 连续扫描。
         * 这样既能测到松开，又不会在没人打字时白烧 I2C 带宽。 */
        TickType_t wait = any_pressed(k)
                        ? pdMS_TO_TICKS(k->cfg.scan_interval_ms)
                        : (k->cfg.int_pin != GPIO_NUM_NC
                               ? portMAX_DELAY
                               : pdMS_TO_TICKS(IDLE_POLL_MS));
        xSemaphoreTake(k->wake, wait);

        if (scan_raw(k, raw) != ESP_OK) {
            ESP_LOGW(TAG, "扫描时 I2C 失败，跳过本轮");
            continue;
        }
        if (has_ghost(k, raw)) {
            ESP_LOGD(TAG, "检测到鬼键矩形，丢弃本轮");
            continue;
        }
        debounce_and_emit(k, raw);
    }

    vTaskDelete(NULL);
}

/* ── 对外接口 ─────────────────────────────────────────────────────────── */

esp_err_t kbmatrix_init(const kbmatrix_config_t *cfg, kbmatrix_cb_t cb, void *ctx)
{
    if (!cfg || g_kbm) return ESP_ERR_INVALID_STATE;
    if (cfg->rows == 0 || cfg->rows > KBMATRIX_MAX_ROWS)   return ESP_ERR_INVALID_ARG;
    if (cfg->cols == 0 || cfg->cols > KBMATRIX_MAX_COLS)   return ESP_ERR_INVALID_ARG;
    if (cfg->col_src_count == 0 || cfg->col_src_count > 2) return ESP_ERR_INVALID_ARG;

    kbm_t *k = calloc(1, sizeof(kbm_t));
    if (!k) return ESP_ERR_NO_MEM;
    k->cfg = *cfg;
    k->cb  = cb;
    k->ctx = ctx;
    if (k->cfg.scan_interval_ms == 0) k->cfg.scan_interval_ms = 5;

    k->wake = xSemaphoreCreateBinary();
    if (!k->wake) { free(k); return ESP_ERR_NO_MEM; }

    /* 行：输出（dir 位 0 = 输出），初始全高 */
    esp_err_t ret = aw9523_set_port_dir(cfg->row_src.dev, cfg->row_src.port, 0x00);
    if (ret != ESP_OK) goto fail;
    ret = aw9523_write_port(cfg->row_src.dev, cfg->row_src.port, 0xFF);
    if (ret != ESP_OK) goto fail;

    /* 列：输入（dir 位 1 = 输入），并开引脚变化中断 */
    for (uint8_t i = 0; i < cfg->col_src_count; i++) {
        ret = aw9523_set_port_dir(cfg->col_src[i].dev, cfg->col_src[i].port, 0xFF);
        if (ret != ESP_OK) goto fail;
        if (cfg->int_pin != GPIO_NUM_NC) {
            ret = aw9523_set_port_int_enable(cfg->col_src[i].dev,
                                             cfg->col_src[i].port, 0xFF);
            if (ret != ESP_OK) goto fail;
        }
    }

    g_kbm = k;
    k->running = true;

    if (cfg->int_pin != GPIO_NUM_NC) {
        gpio_config_t ic = {
            .pin_bit_mask = (1ULL << cfg->int_pin),
            .mode         = GPIO_MODE_INPUT,
            .pull_up_en   = GPIO_PULLUP_ENABLE,
            .pull_down_en = GPIO_PULLDOWN_DISABLE,
            .intr_type    = GPIO_INTR_NEGEDGE,   /* INT 低有效 */
        };
        gpio_config(&ic);
        /* 服务已装过会返回 ESP_ERR_INVALID_STATE，这里无害，忽略即可 */
        gpio_install_isr_service(0);
        gpio_isr_handler_add(cfg->int_pin, int_isr, k);
    }

    if (xTaskCreate(scan_task, "kbmatrix", SCAN_TASK_STACK, k,
                    SCAN_TASK_PRIO, &k->task) != pdPASS) {
        g_kbm = NULL;
        ret = ESP_ERR_NO_MEM;
        goto fail;
    }

    ESP_LOGI(TAG, "就绪 %ux%u, INT=%d, 消抖 %ums",
             cfg->rows, cfg->cols, (int)cfg->int_pin, cfg->debounce_ms);
    return ESP_OK;

fail:
    vSemaphoreDelete(k->wake);
    free(k);
    return ret;
}

esp_err_t kbmatrix_deinit(void)
{
    kbm_t *k = g_kbm;
    if (!k) return ESP_ERR_INVALID_STATE;

    k->running = false;
    if (k->cfg.int_pin != GPIO_NUM_NC) {
        gpio_isr_handler_remove(k->cfg.int_pin);
    }
    xSemaphoreGive(k->wake);             /* 让任务从阻塞中退出并自删 */
    vTaskDelay(pdMS_TO_TICKS(20));

    vSemaphoreDelete(k->wake);
    free(k);
    g_kbm = NULL;
    return ESP_OK;
}

esp_err_t kbmatrix_scan_once(uint16_t *state)
{
    if (!g_kbm || !state) return ESP_ERR_INVALID_ARG;
    return scan_raw(g_kbm, state);
}
