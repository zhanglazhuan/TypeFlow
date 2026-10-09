/*
 * TypeFlow 线协议 v1 —— 编解码实现
 * 固件与 reMarkable 接收器共用。只依赖 C 标准库。
 */

#include "typeflow_proto.h"
#include <string.h>

uint16_t tf_crc16(const uint8_t *data, size_t len)
{
    uint16_t crc = 0xFFFF;
    for (size_t i = 0; i < len; i++) {
        crc ^= (uint16_t)data[i] << 8;
        for (int b = 0; b < 8; b++) {
            crc = (crc & 0x8000) ? (uint16_t)((crc << 1) ^ 0x1021)
                                 : (uint16_t)(crc << 1);
        }
    }
    return crc;
}

size_t tf_encode(uint8_t type, const void *payload, uint16_t payload_len,
                 uint8_t *out, size_t out_cap)
{
    if (!out || payload_len > TF_MAX_PAYLOAD) return 0;
    size_t total = TF_HEADER_LEN + payload_len + TF_CRC_LEN;
    if (out_cap < total) return 0;
    if (payload_len > 0 && !payload) return 0;

    out[0] = (uint8_t)(TF_MAGIC & 0xFF);
    out[1] = (uint8_t)(TF_MAGIC >> 8);
    out[2] = TF_VERSION;
    out[3] = type;
    out[4] = (uint8_t)(payload_len & 0xFF);
    out[5] = (uint8_t)(payload_len >> 8);
    if (payload_len) memcpy(out + TF_HEADER_LEN, payload, payload_len);

    uint16_t crc = tf_crc16(out, TF_HEADER_LEN + payload_len);
    out[TF_HEADER_LEN + payload_len]     = (uint8_t)(crc & 0xFF);
    out[TF_HEADER_LEN + payload_len + 1] = (uint8_t)(crc >> 8);
    return total;
}

bool tf_decode(const uint8_t *buf, size_t len, tf_frame_t *out, size_t *consumed)
{
    if (!buf || !out || !consumed) return false;
    *consumed = 0;

    /* 找 magic。流式传输里可能残留垃圾字节，逐字节滑动直到对上，
     * 把丢弃的字节数报给调用方，它才知道该从缓冲里挪掉多少。 */
    size_t start = 0;
    while (start + 2 <= len) {
        if (buf[start] == (uint8_t)(TF_MAGIC & 0xFF) &&
            buf[start + 1] == (uint8_t)(TF_MAGIC >> 8)) {
            break;
        }
        start++;
    }
    if (start + 2 > len) {
        /* 没找到 magic。保留最后 1 字节：它可能是被切断的 magic 首字节。 */
        *consumed = len > 0 ? len - 1 : 0;
        return false;
    }
    if (start > 0) {
        *consumed = start;
        return false;       /* 先让调用方丢掉垃圾，下次再解 */
    }

    if (len < TF_HEADER_LEN) return false;      /* 头还没收全 */

    if (buf[2] != TF_VERSION) {
        *consumed = 2;      /* 版本不认识，跳过 magic 继续找下一帧 */
        return false;
    }

    uint16_t plen = (uint16_t)(buf[4] | ((uint16_t)buf[5] << 8));
    if (plen > TF_MAX_PAYLOAD) {
        *consumed = 2;      /* 长度离谱，当成误同步 */
        return false;
    }

    size_t total = TF_HEADER_LEN + plen + TF_CRC_LEN;
    if (len < total) return false;               /* 载荷还没收全 */

    uint16_t want = tf_crc16(buf, TF_HEADER_LEN + plen);
    uint16_t got  = (uint16_t)(buf[TF_HEADER_LEN + plen] |
                               ((uint16_t)buf[TF_HEADER_LEN + plen + 1] << 8));
    if (want != got) {
        *consumed = 2;      /* 校验错，跳过 magic 重新同步 */
        return false;
    }

    out->type        = buf[3];
    out->payload_len = plen;
    out->payload     = plen ? (buf + TF_HEADER_LEN) : NULL;
    *consumed        = total;
    return true;
}
