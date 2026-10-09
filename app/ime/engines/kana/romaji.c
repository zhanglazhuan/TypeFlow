/*
 * romaji — 日文罗马字 -> 平假名
 *
 * TODO 尚未实现。要点：
 *   - 主表是罗马字到假名的直接映射（ka->か, shi->し, tsu->つ ...）
 *   - 拗音：ky/gy/sh/ch/ny/hy/my/ry + a/u/o -> 大假名 + 小ゃゅょ
 *   - 促音：同辅音重复（tta -> った）
 *   - 撥音：n 后面不是元音时 -> ん；nn 一定是 ん
 *   - 尾部未完成的罗马字要原样保留，用户才看得到自己打到哪了
 */

#include "romaji.h"
#include <string.h>

bool kana_is_valid_char(char c)
{
    return (c >= 'a' && c <= 'z') || c == '-';
}

int kana_romaji_to_hiragana(const char *romaji, int len, char *out, int out_cap)
{
    if (!romaji || !out || out_cap <= 0) return 0;

    /* TODO 真正的转换。现在原样回显，保证 UI 链路可以先跑通。 */
    int n = len < out_cap - 1 ? len : out_cap - 1;
    memcpy(out, romaji, (size_t)n);
    out[n] = '\0';
    return n;
}
