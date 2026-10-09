/*
 * romaji — 日文罗马字 -> 假名
 *
 * "ka"->か, "kya"->きゃ, "nn"->ん, 促音 "tt"->っt。纯查表 + 少量规则。
 * 假名到汉字的转换走 dict（和拼音共用同一套词库索引机制）。
 * 只允许标准库。
 */
#ifndef KANA_ROMAJI_H
#define KANA_ROMAJI_H

#include <stdint.h>
#include <stdbool.h>

bool kana_is_valid_char(char c);

/**
 * @brief 罗马字转平假名。
 * @param out     UTF-8 输出缓冲
 * @param out_cap 缓冲容量
 * @return 写入字节数；未完成的尾部罗马字原样保留在输出末尾
 */
int kana_romaji_to_hiragana(const char *romaji, int len, char *out, int out_cap);

#endif /* KANA_ROMAJI_H */
