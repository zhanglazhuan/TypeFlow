/*
 * log_system — 日志初始化与落盘
 *
 * 移植自 DeepStoa\system\logs。职责：统一各模块 TAG 的日志等级、
 * 把最近若干条日志留在 RAM 环形缓冲里（崩溃后能捞出来），
 * 必要时写入 userdict 分区或通过 OTA 通道上传。
 */
#ifndef LOG_SYSTEM_H
#define LOG_SYSTEM_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

esp_err_t log_system_init(void);

/** @brief 把环形缓冲里的日志拷出来。用于崩溃诊断或串口 dump。 */
int log_system_dump(char *buf, int buf_len);

#ifdef __cplusplus
}
#endif

#endif /* LOG_SYSTEM_H */
