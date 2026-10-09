/*
 * kbmatrix — 键盘矩阵扫描（经 AW9523 IO 扩展）
 *
 * 职责边界：只负责「哪个 (row, col) 按下/松开」，不认识键值、不认识输入法。
 * 矩阵坐标到语义按键的映射在 system/input。
 *
 * 扫描原理：逐行驱动为低电平，读回列端口，为低的列即该行上被按下的键。
 *
 * 两个必须在硬件上确认的点：
 *   1. 列线是否有上拉电阻。没有的话读回来是浮空值，表现为随机按键。
 *   2. 每个按键是否串了二极管。没有二极管时，同时按下构成矩形三个角的键会
 *      产生「鬼键」(ghosting)：第四个角会被误报为按下。本模块做矩形检测并
 *      丢弃可疑组合（见 kbmatrix.c），但那只能防误报，无法恢复真实按键。
 */

#ifndef KBMATRIX_H
#define KBMATRIX_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "hal/gpio_types.h"
#include "aw9523.h"

#ifdef __cplusplus
extern "C" {
#endif

#define KBMATRIX_MAX_ROWS   8   /* 一片 AW9523 的一个端口 = 8 行 */
#define KBMATRIX_MAX_COLS   16  /* 最多两个端口拼出 16 列 */

/** @brief 按键事件。row/col 是矩阵坐标，不是键值。 */
typedef struct {
    uint8_t row;
    uint8_t col;
    bool    pressed;    /* true = 按下, false = 松开 */
} kbmatrix_event_t;

typedef void (*kbmatrix_cb_t)(const kbmatrix_event_t *ev, void *ctx);

/** @brief 一组列脚的来源：某片 AW9523 的某个端口。 */
typedef struct {
    aw9523_handle_t dev;
    uint8_t         port;   /* AW9523_PORT_P0 / AW9523_PORT_P1 */
} kbmatrix_port_ref_t;

typedef struct {
    uint8_t rows;                       /* 实际行数，<= KBMATRIX_MAX_ROWS */
    uint8_t cols;                       /* 实际列数，<= KBMATRIX_MAX_COLS */

    kbmatrix_port_ref_t row_src;        /* 驱动行的端口（输出，推挽） */
    kbmatrix_port_ref_t col_src[2];     /* 读列的端口，最多两个拼接 */
    uint8_t             col_src_count;  /* 1 或 2 */

    gpio_num_t int_pin;                 /* AW9523 INT 脚，低有效；
                                         * 填 GPIO_NUM_NC 则退化为定时轮询 */
    uint8_t  debounce_ms;               /* 建议 5~10 */
    uint16_t scan_interval_ms;          /* 有键按下时的扫描周期。整轮扫描约
                                         * 1~2ms(400kHz I2C)，不要低于 5ms。 */
} kbmatrix_config_t;

/**
 * @brief 初始化矩阵并启动扫描任务。
 *
 * 配置行端口为输出、列端口为输入；int_pin 有效时开启 AW9523 引脚中断，
 * 使空闲状态下不产生任何 I2C 流量。
 */
esp_err_t kbmatrix_init(const kbmatrix_config_t *cfg, kbmatrix_cb_t cb, void *ctx);

/** @brief 停止扫描任务并释放资源。 */
esp_err_t kbmatrix_deinit(void);

/**
 * @brief 同步扫描一轮，把原始状态写入 state。调试用（debug/t_kbmatrix）。
 *
 * @param state 长度至少 rows 的数组，每项是该行的列位图（bit n = 第 n 列按下）
 */
esp_err_t kbmatrix_scan_once(uint16_t *state);

#ifdef __cplusplus
}
#endif

#endif /* KBMATRIX_H */
