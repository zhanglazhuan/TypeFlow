/*
 * TypeFlow 线协议 v1 —— 帧定义与编解码
 *
 * ════════════════════════════════════════════════════════════════════════
 *  这个文件由固件和 reMarkable 接收器共用（BOOX 侧是 Kotlin，另有一份移植）。
 *  规范见同目录 PROTOCOL.md；改动必须同步到两边，否则线上就不兼容了。
 * ════════════════════════════════════════════════════════════════════════
 *
 * 只允许 C 标准库 —— 和 app/ime 一样的纪律，这样同一份代码能编进固件、
 * 编进 armv7 守护进程、也能在 PC 上写单测。
 */

#ifndef TYPEFLOW_PROTO_H
#define TYPEFLOW_PROTO_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* 线上第 0 字节 'T'(0x54)、第 1 字节 'F'(0x46)。作为小端 uint16 就是 0x4654 ——
 * 别写成 0x5446，那样打包出来线上是 "FT"，抓包会反着读。 */
#define TF_MAGIC            0x4654u
#define TF_VERSION          1

#define TF_HEADER_LEN       6
#define TF_CRC_LEN          2
#define TF_MAX_PAYLOAD      1024
#define TF_MAX_FRAME        (TF_HEADER_LEN + TF_MAX_PAYLOAD + TF_CRC_LEN)

#define TF_TOKEN_LEN        16

/* ── 帧类型 ───────────────────────────────────────────────────────────── */
typedef enum {
    TF_TYPE_HELLO       = 0x01,
    TF_TYPE_HELLO_ACK   = 0x02,
    TF_TYPE_COMMIT_TEXT = 0x10,
    TF_TYPE_COMMIT_ACK  = 0x11,
    TF_TYPE_KEY         = 0x20,
    TF_TYPE_PING        = 0x30,
    TF_TYPE_PONG        = 0x31,
} tf_type_t;

/* ── 品牌（HELLO_ACK 里接收器自报）───────────────────────────────────── */
typedef enum {
    TF_BRAND_UNKNOWN    = 0,
    TF_BRAND_REMARKABLE = 1,
    TF_BRAND_BOOX       = 2,
    TF_BRAND_GENERIC_PC = 3,
} tf_brand_t;

/* ── 能力位图 ─────────────────────────────────────────────────────────── */
#define TF_CAP_COMMIT_TEXT  (1u << 0)
#define TF_CAP_KEY_PASSTHRU (1u << 1)
#define TF_CAP_FOCUS_REPORT (1u << 2)
#define TF_CAP_BAR_STATE    (1u << 3)   /* v2 预留 */

/* ── COMMIT_ACK 结果码 ────────────────────────────────────────────────── */
typedef enum {
    TF_RESULT_OK          = 0,
    TF_RESULT_NO_FOCUS    = 1,  /* 当前没有聚焦的输入框，字没送进去 */
    TF_RESULT_TOO_LONG    = 2,
    TF_RESULT_INTERNAL    = 3,
} tf_result_t;

/* ── 解析出的帧 ───────────────────────────────────────────────────────── */
typedef struct {
    uint8_t         type;
    uint16_t        payload_len;
    const uint8_t  *payload;    /* 指向调用方的缓冲，不拷贝 */
} tf_frame_t;

/* ── CRC-16/CCITT-FALSE（poly 0x1021, init 0xFFFF）───────────────────── */
uint16_t tf_crc16(const uint8_t *data, size_t len);

/**
 * @brief 编码一帧到 out。
 * @return 写入字节数；0 表示 out_cap 不够或 payload_len 超限
 */
size_t tf_encode(uint8_t type, const void *payload, uint16_t payload_len,
                 uint8_t *out, size_t out_cap);

/**
 * @brief 从字节流里尝试解出一帧。
 *
 * TCP 和 USB CDC 都不保证帧边界，所以调用方要维护累积缓冲，反复调这个函数。
 *
 * @param buf      累积缓冲
 * @param len      缓冲中有效字节数
 * @param out      解析结果，payload 指向 buf 内部
 * @param consumed 本次消耗的字节数（即使返回 false 也可能 >0，表示丢弃了垃圾字节）
 * @return true 解出一帧；false 表示数据还不够或需要继续丢弃
 */
bool tf_decode(const uint8_t *buf, size_t len, tf_frame_t *out, size_t *consumed);

#ifdef __cplusplus
}
#endif

#endif /* TYPEFLOW_PROTO_H */
