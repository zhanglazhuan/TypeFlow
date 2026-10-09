/*
 * _template — 复制这个目录开始写你的输入法引擎
 *
 * ════════════════════════════════════════════════════════════════════════
 *  上手步骤：
 *    1. cp -r engines/_template engines/<你的id>
 *    2. 把本文件里所有 "template" 改成你的 id（函数名 ime_engine_get_<目录名>
 *       必须和目录名一致，注册表靠这个约定找到你）
 *    3. idf.py reconfigure     ← 新增目录后必须做，否则链接期找不到你的引擎
 *    4. 在 PC 上跑一致性测试：cd tools/ime-test && make && ./ime-test <你的id>
 *
 *  完整说明见 docs/adding-a-language.md，契约见 ../../engine_api.h。
 * ════════════════════════════════════════════════════════════════════════
 *
 * 这个示例做的是希腊字母转写：a→α, b→β, th→θ。挑它当模板是因为：
 *   - 它**不需要词库**，展示了最短的那条实现路径
 *   - 输出是多字节 UTF-8，会碰到真实的字节长度处理
 *   - 有一个二合字母(th)，展示"下一个键可能改变上一个键的结果"这种状态
 *
 * 如果你的语言需要词库（像中文拼音那样），参考 ../pinyin/engine.c。
 */

#include "engine_api.h"

#include <stdlib.h>
#include <string.h>

#define INPUT_CAP    32
#define PREEDIT_CAP  128    /* 多字节输出，要比输入宽裕 */

typedef struct {
    ime_env_t env;
    char input[INPUT_CAP + 1];
    int  input_len;
    char preedit[PREEDIT_CAP];
} tmpl_t;

/* ── 改这里：你的转换规则 ─────────────────────────────────────────────── */

/* 二合字母优先匹配，所以放在前面 */
static const struct { const char *in; const char *out; } RULES[] = {
    { "th", "θ" }, { "ph", "φ" }, { "ch", "χ" }, { "ps", "ψ" },
    { "a", "α" }, { "b", "β" }, { "g", "γ" }, { "d", "δ" }, { "e", "ε" },
    { "z", "ζ" }, { "i", "ι" }, { "k", "κ" }, { "l", "λ" }, { "m", "μ" },
    { "n", "ν" }, { "o", "ο" }, { "p", "π" }, { "r", "ρ" }, { "s", "σ" },
    { "t", "τ" }, { "u", "υ" }, { "f", "φ" }, { "x", "ξ" }, { "w", "ω" },
};
#define N_RULES ((int)(sizeof(RULES) / sizeof(RULES[0])))

/* ── 把输入串整体重转成 preedit ─────────────────────────────────────────
 * 每次 feed/backspace 都整体重算，而不是增量改。对短编码串来说这样最省心，
 * 也不会出现增量更新特有的状态不一致 bug。
 * -------------------------------------------------------------------- */
static void rebuild(tmpl_t *t)
{
    size_t pos = 0;
    int i = 0;

    while (i < t->input_len) {
        int matched = 0;
        for (int r = 0; r < N_RULES; r++) {
            int n = (int)strlen(RULES[r].in);
            if (i + n > t->input_len) continue;
            if (memcmp(t->input + i, RULES[r].in, (size_t)n) != 0) continue;

            size_t olen = strlen(RULES[r].out);
            if (pos + olen >= PREEDIT_CAP) { matched = 1; break; }
            memcpy(t->preedit + pos, RULES[r].out, olen);
            pos += olen;
            i   += n;
            matched = 1;
            break;
        }
        if (!matched) {
            /* 没有规则匹配就原样保留：用户必须看见自己按了什么，
             * 静默吞掉按键是最让人困惑的行为。 */
            if (pos + 1 >= PREEDIT_CAP) break;
            t->preedit[pos++] = t->input[i++];
        }
    }
    t->preedit[pos] = '\0';
}

/* ── vtable ───────────────────────────────────────────────────────────── */

static void *tmpl_create(const ime_env_t *env)
{
    tmpl_t *t = calloc(1, sizeof(tmpl_t));
    if (!t) return NULL;
    t->env = *env;      /* 存下来即可；本引擎不用词库，不碰 env.dict */
    return t;
}

static void tmpl_destroy(void *st) { free(st); }

static bool tmpl_accepts(void *st, char c)
{
    (void)st;
    return c >= 'a' && c <= 'z';
}

static bool tmpl_feed(void *st, char c)
{
    tmpl_t *t = st;
    if (t->input_len >= INPUT_CAP)   return false;
    if (!tmpl_accepts(st, c))        return false;
    t->input[t->input_len++] = c;
    t->input[t->input_len]   = '\0';
    rebuild(t);
    return true;
}

static bool tmpl_backspace(void *st)
{
    tmpl_t *t = st;
    if (t->input_len == 0) return false;
    t->input[--t->input_len] = '\0';
    rebuild(t);
    return true;
}

static void tmpl_reset(void *st)
{
    tmpl_t *t = st;
    t->input_len  = 0;
    t->input[0]   = '\0';
    t->preedit[0] = '\0';
}

static const char *tmpl_preedit(void *st) { return ((tmpl_t *)st)->preedit; }

/* 确定性转写没有歧义，所以不产生候选。返回 0 是完全合法的 —— 见 engine_api.h。 */
static int tmpl_candidates(void *st, ime_candidate_t *out, int max)
{
    (void)st; (void)out; (void)max;
    return 0;
}

static const char *tmpl_commit(void *st, int index, size_t *len)
{
    tmpl_t *t = st;

    /* 没有候选的引擎只会收到 index < 0。这里不特判 index >= 0，
     * 因为 core 只在 candidates() 返回过东西时才会传正数。 */
    (void)index;

    if (t->preedit[0] == '\0') return NULL;

    /* 注意：返回的指针要到下次调用本引擎前保持有效。这里直接返回 preedit，
     * 然后 reset 会把它清掉 —— 所以必须先算长度、先返回，不能先 reset。
     * 拼音引擎用了独立的 commit 缓冲，那样更稳妥，两种都行。 */
    static char buf[PREEDIT_CAP];
    size_t n = strlen(t->preedit);
    memcpy(buf, t->preedit, n + 1);
    if (len) *len = n;

    tmpl_reset(t);
    return buf;
}

/* ── 改这里：元信息与资源声明 ─────────────────────────────────────────── */

/* ★ 别漏了这个。字体是构建时按所有启用引擎的区段并集裁出来的，
 * 漏声明的结果是你的字在屏幕上全显示成方框。 */
static const ime_ucs_range_t s_ranges[] = {
    { 0x0370, 0x03FF },     /* 希腊字母 */
};

static const ime_reqs_t s_reqs = {
    .ucs_ranges      = s_ranges,
    .ucs_range_count = sizeof(s_ranges) / sizeof(s_ranges[0]),
    .dict_bytes_max  = 0,               /* 不需要词库 */
    .ram_bytes_max   = sizeof(tmpl_t),
};

static const ime_engine_t s_engine = {
    .id           = "template",         /* 稳定唯一，发布后别改 */
    .display_name = "Ελληνικά (示例)",
    .lang_tag     = "el",               /* BCP-47 */
    .reqs         = &s_reqs,

    .create       = tmpl_create,
    .destroy      = tmpl_destroy,
    .accepts_char = tmpl_accepts,
    .feed         = tmpl_feed,
    .backspace    = tmpl_backspace,
    .reset        = tmpl_reset,
    .preedit      = tmpl_preedit,
    .candidates   = tmpl_candidates,
    .commit       = tmpl_commit,
};

/* 函数名必须是 ime_engine_get_<目录名>，注册表靠这个约定找到你 */
const ime_engine_t *ime_engine_get__template(void) { return &s_engine; }
