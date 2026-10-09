/*
 * ime_core — 与语言无关的内核：引擎切换、候选分页、提交缓冲
 *
 * 只允许 C 标准库。
 */

#include "ime.h"
#include "ime_registry.h"

#include <stdlib.h>
#include <string.h>

#define MAX_CANDIDATES      64      /* 一次从引擎取的上限 */
#define COMMIT_CAP          256
#define DEFAULT_PAGE_SIZE   9

struct ime_ctx {
    ime_env_provider_t provider;
    bool               has_provider;

    const ime_engine_t *engine;
    void               *state;      /* 当前引擎的实例 */

    ime_candidate_t cands[MAX_CANDIDATES];
    int             cand_count;
    int             page;
    int             page_size;

    char commit_buf[COMMIT_CAP];
};

/* ── 注册表查询 ───────────────────────────────────────────────────────── */

int ime_engine_count(void)
{
    int n = 0;
    while (ime_registry[n]) n++;
    return n;
}

const ime_engine_t *ime_engine_at(int index)
{
    if (index < 0) return NULL;
    for (int i = 0; ime_registry[i]; i++) {
        if (i == index) return ime_registry[i]();
    }
    return NULL;
}

const ime_engine_t *ime_engine_find(const char *id)
{
    if (!id) return NULL;
    for (int i = 0; ime_registry[i]; i++) {
        const ime_engine_t *e = ime_registry[i]();
        if (e && e->id && strcmp(e->id, id) == 0) return e;
    }
    return NULL;
}

/* ── 引擎实例化 ───────────────────────────────────────────────────────── */

static void drop_engine(ime_t *im)
{
    if (im->engine && im->state && im->engine->destroy) {
        im->engine->destroy(im->state);
    }
    im->state      = NULL;
    im->engine     = NULL;
    im->cand_count = 0;
    im->page       = 0;
}

static bool spin_up(ime_t *im, const ime_engine_t *e)
{
    /* 没有 provider 时给一个全零 env：dict.size == 0、store.load == NULL。
     * 契约要求引擎必须能接受这种情况（见 engine_api.h），所以不需要特判。 */
    ime_env_t env;
    memset(&env, 0, sizeof(env));
    if (im->has_provider && im->provider.get_env) {
        im->provider.get_env(im->provider.ctx, e->id, &env);
    }

    void *st = e->create ? e->create(&env) : NULL;
    if (!st) return false;

    im->engine     = e;
    im->state      = st;
    im->cand_count = 0;
    im->page       = 0;
    return true;
}

/* ── 生命周期 ─────────────────────────────────────────────────────────── */

ime_t *ime_create(const ime_env_provider_t *provider)
{
    if (ime_engine_count() == 0) return NULL;   /* 一个引擎都没编进来 */

    ime_t *im = calloc(1, sizeof(ime_t));
    if (!im) return NULL;

    im->page_size = DEFAULT_PAGE_SIZE;
    if (provider) {
        im->provider     = *provider;
        im->has_provider = true;
    }

    /* 默认选第一个注册的引擎。谁是第一个取决于目录扫描顺序，
     * 所以宿主应该在创建后显式 ime_select_engine() 成用户配置里那个。 */
    for (int i = 0; ime_registry[i]; i++) {
        const ime_engine_t *e = ime_registry[i]();
        if (e && spin_up(im, e)) return im;
    }

    free(im);   /* 所有引擎都创建失败 */
    return NULL;
}

void ime_destroy(ime_t *im)
{
    if (!im) return;
    drop_engine(im);
    free(im);
}

bool ime_select_engine(ime_t *im, const char *id)
{
    if (!im) return false;
    const ime_engine_t *e = ime_engine_find(id);
    if (!e) return false;
    if (im->engine == e) return true;

    const ime_engine_t *prev = im->engine;
    void               *prev_state = im->state;

    im->engine = NULL;
    im->state  = NULL;
    if (spin_up(im, e)) {
        if (prev && prev_state && prev->destroy) prev->destroy(prev_state);
        return true;
    }

    /* 新引擎起不来就退回原来那个，不要让内核变成没有引擎的状态 */
    im->engine = prev;
    im->state  = prev_state;
    return false;
}

const ime_engine_t *ime_current_engine(const ime_t *im)
{
    return im ? im->engine : NULL;
}

/* ── 输入 ─────────────────────────────────────────────────────────────── */

static void refresh_candidates(ime_t *im)
{
    im->cand_count = 0;
    im->page       = 0;

    if (!im->engine->candidates) return;

    int n = im->engine->candidates(im->state, im->cands, MAX_CANDIDATES);
    im->cand_count = (n > 0 && n <= MAX_CANDIDATES) ? n : 0;
}

bool ime_accepts_char(const ime_t *im, char c)
{
    if (!im || !im->engine->accepts_char) return false;
    return im->engine->accepts_char(im->state, c);
}

bool ime_feed_char(ime_t *im, char c)
{
    if (!im || !im->engine->feed) return false;
    if (!im->engine->feed(im->state, c)) return false;
    refresh_candidates(im);
    return true;
}

bool ime_backspace(ime_t *im)
{
    if (!im || !im->engine->backspace) return false;
    if (!im->engine->backspace(im->state)) return false;
    refresh_candidates(im);
    return true;
}

void ime_clear(ime_t *im)
{
    if (!im) return;
    if (im->engine->reset) im->engine->reset(im->state);
    im->cand_count = 0;
    im->page       = 0;
}

const char *ime_get_composition(const ime_t *im)
{
    if (!im || !im->engine->preedit) return "";
    const char *s = im->engine->preedit(im->state);
    return s ? s : "";
}

bool ime_is_composing(const ime_t *im)
{
    const char *s = ime_get_composition(im);
    return s[0] != '\0';
}

/* ── 候选分页 ─────────────────────────────────────────────────────────────
 * 分页放在 core 而不是让每个引擎自己实现：翻页逻辑和语言无关，贡献者
 * candidates() 一次给出全部候选就行，白拿分页。
 * ---------------------------------------------------------------------- */

void ime_set_page_size(ime_t *im, int n)
{
    if (!im || n <= 0) return;
    im->page_size = n;
    im->page      = 0;
}

int ime_candidate_total(const ime_t *im)
{
    return im ? im->cand_count : 0;
}

int ime_get_candidates(ime_t *im, ime_candidate_t *out, int max)
{
    if (!im || !out || max <= 0) return 0;

    int start = im->page * im->page_size;
    if (start >= im->cand_count) return 0;

    int n = im->cand_count - start;
    if (n > im->page_size) n = im->page_size;
    if (n > max)           n = max;

    memcpy(out, &im->cands[start], (size_t)n * sizeof(ime_candidate_t));
    return n;
}

bool ime_page_next(ime_t *im)
{
    if (!im) return false;
    if ((im->page + 1) * im->page_size >= im->cand_count) return false;
    im->page++;
    return true;
}

bool ime_page_prev(ime_t *im)
{
    if (!im || im->page == 0) return false;
    im->page--;
    return true;
}

/* ── 提交 ─────────────────────────────────────────────────────────────── */

const char *ime_commit(ime_t *im, int index, size_t *len_out)
{
    if (!im || !im->engine->commit) return NULL;

    int engine_index;
    if (index < 0) {
        engine_index = -1;                          /* 直接提交 preedit */
    } else {
        engine_index = im->page * im->page_size + index;
        if (engine_index >= im->cand_count) return NULL;
    }

    size_t len = 0;
    const char *text = im->engine->commit(im->state, engine_index, &len);
    if (!text || len == 0) return NULL;

    /* 拷进自己的缓冲：引擎返回的指针可能指向它内部缓冲，而下面 refresh
     * 会再次调用引擎、让那块内存失效，所以不能把它直接交给调用方。 */
    if (len > COMMIT_CAP - 1) len = COMMIT_CAP - 1;
    memcpy(im->commit_buf, text, len);
    im->commit_buf[len] = '\0';
    if (len_out) *len_out = len;

    /* 引擎自己更新了输入状态（可能清空、也可能留下未转换的尾巴），
     * 重新取一遍候选，部分转换才能接着出候选。 */
    refresh_candidates(im);

    return im->commit_buf;
}
