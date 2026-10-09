/*
 * settings — 配置持久化，含 USB/BLE 通道选择与输入法模式
 *
 * TODO 尚未实现，当前只有 init 占位以保证工程结构完整。
 */
#ifndef SETTINGS_H
#define SETTINGS_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t settings_init(void);

#ifdef __cplusplus
}
#endif

#endif
