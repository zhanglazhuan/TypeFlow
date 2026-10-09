/*
 * syllable — 拼音音节切分
 *
 * 用「声母 + 韵母」分解来校验音节，而不是列一张 400+ 条的音节全表：
 * 表更小、好维护，代价是会多接受一些不存在的组合（如 "jua"）。这没问题 ——
 * 非法组合在 dict 里查不到词，自然被淘汰，不需要在这一层就拒绝。
 *
 * 只允许标准库。
 */

#include "syllable.h"
#include <string.h>

/* 零声母用空串表示（"an"、"wo" 这类） */
static const char *const INITIALS[] = {
    "", "b", "p", "m", "f", "d", "t", "n", "l", "g", "k", "h",
    "j", "q", "x", "zh", "ch", "sh", "r", "z", "c", "s", "y", "w",
};
#define N_INITIALS ((int)(sizeof(INITIALS) / sizeof(INITIALS[0])))

/* v 代替 ü，键盘上没有 ü 键 */
static const char *const FINALS[] = {
    "a", "o", "e", "ai", "ei", "ao", "ou", "an", "en", "ang", "eng", "ong", "er",
    "i", "ia", "ie", "iao", "iu", "ian", "in", "iang", "ing", "iong",
    "u", "ua", "uo", "uai", "ui", "uan", "un", "uang", "ueng",
    "v", "ve", "van", "vn",
};
#define N_FINALS ((int)(sizeof(FINALS) / sizeof(FINALS[0])))

/* id = 声母序号 * N_FINALS + 韵母序号 */
#define MAKE_ID(ini, fin)   ((uint16_t)((ini) * N_FINALS + (fin)))

bool pinyin_is_valid_char(char c)
{
    return (c >= 'a' && c <= 'z') || c == '\'';
}

/* 在 s 处尝试匹配一个音节，返回消耗的字节数，0 表示匹配不上。
 * 贪心取最长：先试 2 字母声母(zh/ch/sh)，韵母也按长度降序试。 */
static int match_syllable(const char *s, int len, uint16_t *id)
{
    int best_len = 0;
    uint16_t best_id = PINYIN_INVALID_ID;

    for (int i = 0; i < N_INITIALS; i++) {
        int ilen = (int)strlen(INITIALS[i]);
        if (ilen > len) continue;
        if (ilen > 0 && strncmp(s, INITIALS[i], (size_t)ilen) != 0) continue;

        for (int f = 0; f < N_FINALS; f++) {
            int flen = (int)strlen(FINALS[f]);
            if (ilen + flen > len) continue;
            if (strncmp(s + ilen, FINALS[f], (size_t)flen) != 0) continue;

            if (ilen + flen > best_len) {
                best_len = ilen + flen;
                best_id  = MAKE_ID(i, f);
            }
        }
    }

    if (best_len > 0) *id = best_id;
    return best_len;
}

int pinyin_segment(const char *input, int len, uint16_t *out_ids, int max_ids)
{
    if (!input || !out_ids || max_ids <= 0) return 0;

    int count = 0;
    int pos   = 0;

    while (pos < len && count < max_ids) {
        if (input[pos] == '\'') { pos++; continue; }   /* 用户手写的分隔号 */

        uint16_t id = PINYIN_INVALID_ID;
        int n = match_syllable(input + pos, len - pos, &id);
        if (n == 0) {
            /* 切不动：剩下的可能是还没打完的半个音节。已切出的部分照样有用，
             * 所以返回已有结果而不是判定整串失败。 */
            break;
        }
        out_ids[count++] = id;
        pos += n;
    }

    return count;
}

const char *pinyin_id_to_str(uint16_t id)
{
    static char buf[8];

    if (id == PINYIN_INVALID_ID) return NULL;
    int ini = id / N_FINALS;
    int fin = id % N_FINALS;
    if (ini >= N_INITIALS || fin >= N_FINALS) return NULL;

    /* 注意：返回的是静态缓冲，调用方必须立刻用掉。engine.c 里是边拼边 memcpy，
     * 没有同时持有两个返回值，安全。 */
    size_t n = 0;
    size_t li = strlen(INITIALS[ini]);
    memcpy(buf, INITIALS[ini], li); n += li;
    size_t lf = strlen(FINALS[fin]);
    if (n + lf < sizeof(buf)) { memcpy(buf + n, FINALS[fin], lf); n += lf; }
    buf[n] = '\0';
    return buf;
}
