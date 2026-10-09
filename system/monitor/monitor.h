/*
 * monitor — 堆/栈/任务水位监控，定期打点便于发现泄漏
 *
 * TODO 尚未实现，当前只有 init 占位以保证工程结构完整。
 */
#ifndef MONITOR_H
#define MONITOR_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t monitor_init(void);

#ifdef __cplusplus
}
#endif

#endif
