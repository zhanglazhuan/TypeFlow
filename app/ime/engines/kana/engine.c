/*
 * kana 引擎 —— 日文（罗马字 → 假名 → 汉字）
 *
 * 契约见 ../../engine_api.h。只允许 C 标准库。
 *
 * 这个引擎有个有意思的性质：**没有词库也能用**。罗马字转假名是确定性的，
 * 不需要查词典；词库只在"假名转汉字"那一步才需要。所以词库缺失时它退化成
 * 一个纯假名输入法，仍然可用 —— 拼音引擎做不到这一点。
 */

#include "engine_api.h"
#include "romaji.h"

#include <stdlib.h>
#include <string.h>

#define INPUT_CAP    32
#define PREEDIT_CAP  128    /* 假名是多字节，比罗马字长 */
#define COMMIT_CAP   128

typedef struct {
    ime_env_t env;
    char input[INPUT_CAP + 1];
    int  input_len;
    char preedit[PREEDIT_CAP];
    char commit[COMMIT_CAP];
} kana_t;

static void rebuild(kana_t *k)
{
    if (k->input_len == 0) { k->preedit[0] = '\0'; return; }
    kana_romaji_to_hiragana(k->input, k->input_len, k->preedit, PREEDIT_CAP);
}

static void *ka_create(const ime_env_t *env)
{
    kana_t *k = calloc(1, sizeof(kana_t));
    if (!k) return NULL;
    k->env = *env;
    return k;
}

static void ka_destroy(void *st) { free(st); }

static bool ka_accepts(void *st, char c) { (void)st; return kana_is_valid_char(c); }

static bool ka_feed(void *st, char c)
{
    kana_t *k = st;
    if (k->input_len >= INPUT_CAP) return false;
    if (!kana_is_valid_char(c))    return false;
    k->input[k->input_len++] = c;
    k->input[k->input_len]   = '\0';
    rebuild(k);
    return true;
}

static bool ka_backspace(void *st)
{
    kana_t *k = st;
    if (k->input_len == 0) return false;
    k->input[--k->input_len] = '\0';
    rebuild(k);
    return true;
}

static void ka_reset(void *st)
{
    kana_t *k = st;
    k->input_len  = 0;
    k->input[0]   = '\0';
    k->preedit[0] = '\0';
}

static const char *ka_preedit(void *st) { return ((kana_t *)st)->preedit; }

static int ka_candidates(void *st, ime_candidate_t *out, int max)
{
    kana_t *k = st;
    (void)out; (void)max;
    if (k->env.dict.size == 0) return 0;    /* 无词库 = 纯假名模式，没有候选 */

    /* TODO 假名 → 汉字候选，查词库 */
    return 0;
}

static const char *ka_commit(void *st, int index, size_t *len)
{
    kana_t *k = st;

    if (index < 0) {
        /* 直接提交假名。这是无词库时的正常路径，不是退路 —— 纯假名输入
         * 本身就是可用的日文输入方式。 */
        if (k->preedit[0] == '\0') return NULL;
        size_t n = strlen(k->preedit);
        if (n > COMMIT_CAP - 1) n = COMMIT_CAP - 1;
        memcpy(k->commit, k->preedit, n);
        k->commit[n] = '\0';
        if (len) *len = n;
        ka_reset(k);
        return k->commit;
    }

    /* TODO 汉字候选提交 */
    return NULL;
}

static const ime_ucs_range_t s_ranges[] = {
    { 0x3000, 0x303F },     /* 日文标点 */
    { 0x3040, 0x309F },     /* 平假名 */
    { 0x30A0, 0x30FF },     /* 片假名 */
    { 0x4E00, 0x9FFF },     /* 汉字 */
    { 0xFF00, 0xFFEF },     /* 全角 */
};

static const ime_reqs_t s_reqs = {
    .ucs_ranges      = s_ranges,
    .ucs_range_count = sizeof(s_ranges) / sizeof(s_ranges[0]),
    .dict_bytes_max  = 2u * 1024 * 1024,
    .ram_bytes_max   = sizeof(kana_t),
};

static const ime_engine_t s_engine = {
    .id           = "kana",
    .display_name = "日本語",
    .lang_tag     = "ja",
    .reqs         = &s_reqs,

    .create       = ka_create,
    .destroy      = ka_destroy,
    .accepts_char = ka_accepts,
    .feed         = ka_feed,
    .backspace    = ka_backspace,
    .reset        = ka_reset,
    .preedit      = ka_preedit,
    .candidates   = ka_candidates,
    .commit       = ka_commit,
};

const ime_engine_t *ime_engine_get_kana(void) { return &s_engine; }
