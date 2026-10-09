/*
 * battery — 电池电压采样与电量估算（BLE 无线形态必需）
 *
 * TODO 尚未实现，当前只有 init 占位以保证工程结构完整。
 */
#ifndef BATTERY_H
#define BATTERY_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t battery_init(void);

#ifdef __cplusplus
}
#endif

#endif
