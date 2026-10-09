/*
 * sleep — 低功耗：无按键时进 light sleep，靠 AW9523 INT 唤醒
 *
 * TODO 尚未实现，当前只有 init 占位以保证工程结构完整。
 */
#ifndef SLEEP_H
#define SLEEP_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t sleep_init(void);

#ifdef __cplusplus
}
#endif

#endif
