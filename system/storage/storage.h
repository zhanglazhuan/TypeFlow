/*
 * storage — 持久化（全部在片内 flash，本项目没有 SD 卡）
 *
 * 硬件上没有 SD 卡插槽，也不打算加：词库 6 MB、用户词频 1 MB，16 MB 片内 flash
 * 完全够用，而且 mmap 出来当数组查比走文件系统快得多（见 app/ime/dict.h）。
 * 所以这里不会出现 sdmmc/sdspi 相关的任何东西 —— 参考项目 DeepStoa 有
 * sd_control，那是它要放音频文件，TypeFlow 不需要。
 *
 * 三块互不相干的存储，别混在一起：
 *   nvs      设置项（键值，小）           -> settings 用
 *   dict     词库镜像（只读，6MB）         -> mmap 成指针
 *   userdict 用户词频/自造词（可写，1MB）  -> littlefs
 *
 * ★ 这里刻意不出现任何 ime 类型。原先的 system/storage 直接返回
 *   ime_dict_source_t，那是分层倒置：系统层反过来依赖了应用层的头文件。
 *   现在本模块只提供裸的字节访问，由 app/candbar 把它包成 ime 要的回调结构 ——
 *   ime 的平台无关性照样成立（见 app/ime/ime.h 文件头），而依赖方向理顺了。
 */

#ifndef STORAGE_H
#define STORAGE_H

#include <stdint.h>
#include <stddef.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 初始化 NVS、映射 dict 分区、挂载 userdict。 */
esp_err_t storage_init(void);

/**
 * @brief 取词库中 offset 处的只读指针，保证至少 len 字节可读。
 *
 * 接口形状是「给 offset 要指针」而不是「返回整块基地址」：万一 6 MB 一次性
 * mmap 在 S3 上失败，改成按页分段映射只需动这个函数的实现，上层一行不用改。
 *
 * @return NULL 表示词库未就绪或越界
 */
const uint8_t *storage_dict_map(uint32_t offset, uint32_t len);

/** @brief 词库总字节数。0 表示 dict 分区不存在或未刷入。 */
uint32_t storage_dict_size(void);

/** @brief 从 userdict 读一项。返回实际字节数，<0 表示不存在或出错。 */
int storage_user_load(const char *key, void *buf, int buf_len);

/** @brief 写一项到 userdict。返回 0 成功。 */
int storage_user_save(const char *key, const void *buf, int len);

#ifdef __cplusplus
}
#endif

#endif /* STORAGE_H */
