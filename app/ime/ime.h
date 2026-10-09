/*
 * ime — 输入法内核对外 API
 *
 * 给 app/candbar 用。想加一门语言的话你要读的不是这个文件，而是
 * engine_api.h（外加 docs/adding-a-language.md）。
 *
 * 这一层与语言无关：它负责引擎注册表、引擎切换、候选分页、提交缓冲。
 * 具体某门语言怎么把按键变成字，在 engines/<id>/ 里。
 *
 * 纪律：本组件只允许包含 C 标准库。理由见 engine_api.h 文件头。
 */

#ifndef IME_H
#define IME_H

#include "engine_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ime_ctx ime_t;

/* ── 环境提供者 ───────────────────────────────────────────────────────────
 * core 自己不知道词库在哪、用户数据存哪 —— 由宿主注入。切换引擎时 core 会
 * 用目标引擎的 id 回调这个函数，宿主负责从词库容器里找出对应那一段。
 *
 * 宿主侧实现见 app/candbar/candbar.c。
 * ---------------------------------------------------------------------- */
typedef struct {
    /** @brief 填出 out。本引擎没有词库就把 out->dict.size 置 0。 */
    void (*get_env)(void *ctx, const char *engine_id, ime_env_t *out);
    void *ctx;
} ime_env_provider_t;

/* ── 生命周期 ─────────────────────────────────────────────────────────── */

/**
 * @brief 创建内核。会选中第一个已注册的引擎作为默认。
 * @param provider 可为 NULL（那样所有引擎都拿不到词库，只有不需要词库的能用）
 */
ime_t *ime_create(const ime_env_provider_t *provider);
void   ime_destroy(ime_t *ime);

/* ── 引擎 ─────────────────────────────────────────────────────────────── */

/** @brief 已编入固件的引擎个数。 */
int ime_engine_count(void);

/** @brief 按序号取引擎元信息，用于在设置界面里列出可选输入法。 */
const ime_engine_t *ime_engine_at(int index);

/** @brief 按 id 查引擎，没有返回 NULL。 */
const ime_engine_t *ime_engine_find(const char *id);

/** @brief 切换当前引擎。会清掉正在输入的内容。返回 false = 没这个 id。 */
bool ime_select_engine(ime_t *ime, const char *id);

/** @brief 当前引擎。内核创建成功后不会是 NULL。 */
const ime_engine_t *ime_current_engine(const ime_t *ime);

/* ── 输入 ─────────────────────────────────────────────────────────────── */

/** @brief 当前引擎认这个字符吗？不认的话调用方应把它透传给主机。 */
bool ime_accepts_char(const ime_t *ime, char c);

/** @brief 追加一个编码字符。 */
bool ime_feed_char(ime_t *ime, char c);

/** @brief 退一格。返回 false = 已空，调用方应把退格透传给主机。 */
bool ime_backspace(ime_t *ime);

/** @brief 清空输入与候选。 */
void ime_clear(ime_t *ime);

/** @brief 是否正在输入（preedit 非空）。 */
bool ime_is_composing(const ime_t *ime);

/** @brief 当前 preedit，UTF-8，常驻有效。 */
const char *ime_get_composition(const ime_t *ime);

/* ── 候选与分页 ───────────────────────────────────────────────────────── */

/** @brief 设置每页候选数。长条屏一行放得下几个就设几个。 */
void ime_set_page_size(ime_t *ime, int n);

/** @brief 取当前页候选，返回个数。0 表示当前引擎不产生候选或没有匹配。 */
int ime_get_candidates(ime_t *ime, ime_candidate_t *out, int max);

/** @brief 当前页共有几个候选（便于 UI 画序号）。 */
int ime_candidate_total(const ime_t *ime);

bool ime_page_next(ime_t *ime);
bool ime_page_prev(ime_t *ime);

/* ── 提交 ─────────────────────────────────────────────────────────────── */

/**
 * @brief 提交当前页第 index 个候选。
 *
 * index < 0 表示"直接提交 preedit" —— 不产生候选的输入法（谚文、Telex）
 * 走这条路。
 *
 * @param len_out 输出 UTF-8 字节数
 * @return 提交的文本，指针在下次调用本内核前有效；NULL 表示没东西可提交
 */
const char *ime_commit(ime_t *ime, int index, size_t *len_out);

#ifdef __cplusplus
}
#endif

#endif /* IME_H */
