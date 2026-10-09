/*
 * pinyin 引擎 —— 中文拼音
 *
 * 契约见 ../../engine_api.h。只允许 C 标准库。
 *
 * 这是一个「需要词库」的引擎：编码切成音节后查词库出候选。
 * 对比 engines/_template 里那个不需要词库的最小例子。
 */

#include "engine_api.h"
#include "syllable.h"

#include <stdlib.h>
#include <string.h>

#define INPUT_CAP       32
#define PREEDIT_CAP     96      /* 比输入长：音节间要插分隔号 */
#define COMMIT_CAP      128

typedef struct {
    ime_env_t env;

    char     input[INPUT_CAP + 1];
    int       input_len;

    uint16_t syllables[PINYIN_MAX_SYLLABLES];
    int      syllable_count;

    char     preedit[PREEDIT_CAP];
    char     commit[COMMIT_CAP];
} pinyin_t;

/* ── 内部 ─────────────────────────────────────────────────────────────── */

static void rebuild_preedit(pinyin_t *p)
{
    p->preedit[0] = '\0';
    if (p->input_len == 0) return;

    if (p->syllable_count == 0) {
        /* 切不动就原样显示 —— 用户得看见自己打了什么，
         * 显示空白会让人以为按键丢了。 */
        int n = p->input_len < PREEDIT_CAP - 1 ? p->input_len : PREEDIT_CAP - 1;
        memcpy(p->preedit, p->input, (size_t)n);
        p->preedit[n] = '\0';
        return;
    }

    /* 用分隔号连接切好的音节，让用户看清切分结果对不对 */
    size_t pos = 0;
    for (int i = 0; i < p->syllable_count; i++) {
        const char *s = pinyin_id_to_str(p->syllables[i]);
        if (!s) continue;
        size_t n = strlen(s);
        if (pos + n + 2 >= PREEDIT_CAP) break;
        if (i > 0) p->preedit[pos++] = '\'';
        memcpy(p->preedit + pos, s, n);
        pos += n;
    }
    p->preedit[pos] = '\0';
}

static void resegment(pinyin_t *p)
{
    p->syllable_count = 0;
    if (p->input_len > 0) {
        p->syllable_count = pinyin_segment(p->input, p->input_len,
                                           p->syllables, PINYIN_MAX_SYLLABLES);
    }
    rebuild_preedit(p);
}

/* ── vtable ───────────────────────────────────────────────────────────── */

static void *py_create(const ime_env_t *env)
{
    pinyin_t *p = calloc(1, sizeof(pinyin_t));
    if (!p) return NULL;
    p->env = *env;
    /* 没有词库也照样创建成功：用户至少能看到拼音切分，
     * 而且这样设备缺词库时不会整个输入法起不来。 */
    return p;
}

static void py_destroy(void *st) { free(st); }

static bool py_accepts(void *st, char c)
{
    (void)st;
    return pinyin_is_valid_char(c);
}

static bool py_feed(void *st, char c)
{
    pinyin_t *p = st;
    if (p->input_len >= INPUT_CAP)   return false;
    if (!pinyin_is_valid_char(c))    return false;

    p->input[p->input_len++] = c;
    p->input[p->input_len]   = '\0';
    resegment(p);
    return true;
}

static bool py_backspace(void *st)
{
    pinyin_t *p = st;
    if (p->input_len == 0) return false;
    p->input[--p->input_len] = '\0';
    resegment(p);
    return true;
}

static void py_reset(void *st)
{
    pinyin_t *p = st;
    p->input_len      = 0;
    p->input[0]       = '\0';
    p->preedit[0]     = '\0';
    p->syllable_count = 0;
}

static const char *py_preedit(void *st)
{
    return ((pinyin_t *)st)->preedit;
}

static int py_candidates(void *st, ime_candidate_t *out, int max)
{
    pinyin_t *p = st;
    (void)out; (void)max;

    if (p->env.dict.size == 0 || p->syllable_count == 0) return 0;

    /* TODO 查词库。需要先定二进制格式，见 tools/dict_build.py。
     * 现在返回 0 —— 契约允许，UI 会表现为"有 preedit 但没候选"。 */
    return 0;
}

static const char *py_commit(void *st, int index, size_t *len)
{
    pinyin_t *p = st;

    if (index < 0) {
        /* 没有候选可选时直接把编码串本身发出去（用户想打英文的退路）。
         * 拼音引擎下这种情况就是"词库缺失"或"打了个没有匹配的音"。 */
        if (p->input_len == 0) return NULL;
        int n = p->input_len < COMMIT_CAP - 1 ? p->input_len : COMMIT_CAP - 1;
        memcpy(p->commit, p->input, (size_t)n);
        p->commit[n] = '\0';
        if (len) *len = (size_t)n;
        py_reset(p);
        return p->commit;
    }

    /* TODO 候选提交：取第 index 个候选、更新词频、消耗掉已转换的音节
     * （"woshi" 选了"我"之后要留下 "shi"）。等 py_candidates 实现后再写。 */
    return NULL;
}

/* ── 元信息 ───────────────────────────────────────────────────────────── */

static const ime_ucs_range_t s_ranges[] = {
    { 0x3000, 0x303F },     /* CJK 标点 */
    { 0x4E00, 0x9FFF },     /* CJK 统一汉字基本区 */
    { 0xFF00, 0xFFEF },     /* 全角字符 */
};

static const ime_reqs_t s_reqs = {
    .ucs_ranges      = s_ranges,
    .ucs_range_count = sizeof(s_ranges) / sizeof(s_ranges[0]),
    .dict_bytes_max  = 4u * 1024 * 1024,
    .ram_bytes_max   = sizeof(pinyin_t),
};

static const ime_engine_t s_engine = {
    .id           = "pinyin",
    .display_name = "拼音",
    .lang_tag     = "zh-Hans",
    .reqs         = &s_reqs,

    .create       = py_create,
    .destroy      = py_destroy,
    .accepts_char = py_accepts,
    .feed         = py_feed,
    .backspace    = py_backspace,
    .reset        = py_reset,
    .preedit      = py_preedit,
    .candidates   = py_candidates,
    .commit       = py_commit,
};

const ime_engine_t *ime_engine_get_pinyin(void) { return &s_engine; }
