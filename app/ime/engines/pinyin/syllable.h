/*
 * syllable — 拼音音节切分
 *
 * 把连续字母串切成音节 id 序列，例如 "nihao" -> ["ni","hao"]。
 * 难点在歧义："xian" 可以是 "xian" 也可以是 "xi'an"，"nvhai" 的 v=ü。
 * 策略：最大匹配 + 回溯，多种切法都保留，交给 dict 按词频决定。
 * 只允许标准库。切分逻辑独立成文件，便于在 PC 上单独测试。
 */
#ifndef PINYIN_SYLLABLE_H
#define PINYIN_SYLLABLE_H

#include <stdint.h>
#include <stdbool.h>

#define PINYIN_MAX_SYLLABLES    16
#define PINYIN_INVALID_ID       0xFFFF

/** @brief 该字符是否可能是拼音编码的一部分（a-z 和分隔号）。 */
bool pinyin_is_valid_char(char c);

/**
 * @brief 把字母串切成音节 id 序列。
 * @return 音节个数；0 表示切不动
 */
int pinyin_segment(const char *input, int len,
                   uint16_t *out_ids, int max_ids);

/** @brief 音节 id 转回可显示的拼音串，用于 composition 显示。 */
const char *pinyin_id_to_str(uint16_t id);

#endif /* PINYIN_SYLLABLE_H */
