/*
 * ota — 固件升级，双 OTA 分区切换
 *
 * TODO 尚未实现，当前只有 init 占位以保证工程结构完整。
 */
#ifndef OTA_H
#define OTA_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t ota_init(void);

#ifdef __cplusplus
}
#endif

#endif
