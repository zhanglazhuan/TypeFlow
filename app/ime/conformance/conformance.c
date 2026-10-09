/*
 * conformance — 通用一致性测试实现
 */

#include "conformance.h"

#include <stdio.h>
#include <string.h>

#define MAX_CANDS   32

static int s_pass, s_fail, s_verbose;

static void check(const char *name, int ok)
{
    if (ok) {
        s_pass++;
        if (s_verbose) printf("    PASS  %s\n", name);
    } else {
        s_fail++;
        printf("    FAIL  %s\n", name);
    }
}

/* ── UTF-8 合法性 ─────────────────────────────────────────────────────────
 * 这一项很值得测：引擎输出非法 UTF-8 的话，下游 hidout 的解码器会拒绝，
 * 屏幕上也会显示成乱码，而症状离原因很远，很难查。
 * ---------------------------------------------------------------------- */
static int valid_utf8(const char *s, size_t len)
{
    const unsigned char *p = (const unsigned char *)s;
    size_t i = 0;
    while (i < len) {
        unsigned char c = p[i];
        int n;
        if      (c < 0x80)          n = 1;
        else if ((c & 0xE0) == 0xC0) n = 2;
        else if ((c & 0xF0) == 0xE0) n = 3;
        else if ((c & 0xF8) == 0xF0) n = 4;
        else return 0;                      /* 非法首字节 */

        if (i + (size_t)n > len) return 0;   /* 被截断 */
        for (int k = 1; k < n; k++) {
            if ((p[i + k] & 0xC0) != 0x80) return 0;   /* 续字节不合法 */
        }
        i += (size_t)n;
    }
    return 1;
}

/* ── 元信息 ───────────────────────────────────────────────────────────── */

static void test_metadata(const ime_engine_t *e)
{
    check("有 id", e->id && e->id[0]);
    check("有 display_name", e->display_name && e->display_name[0]);
    check("有 lang_tag", e->lang_tag && e->lang_tag[0]);
    check("有 reqs", e->reqs != NULL);

    /* id 会被写进用户配置和词库容器的 TOC，所以限制字符集，
     * 免得出现空格、大写、中文这类以后难处理的 id */
    int id_ok = 1;
    for (const char *p = e->id; p && *p; p++) {
        if (!((*p >= 'a' && *p <= 'z') || (*p >= '0' && *p <= '9') || *p == '_')) {
            id_ok = 0;
            break;
        }
    }
    check("id 只含小写字母/数字/下划线", id_ok);

    check("必需的函数指针都不为空",
          e->create && e->destroy && e->accepts_char && e->feed &&
          e->backspace && e->reset && e->preedit && e->candidates && e->commit);

    if (e->reqs) {
        check("声明了 ucs_ranges（否则字形会缺，屏幕显示方框）",
              e->reqs->ucs_range_count > 0 && e->reqs->ucs_ranges != NULL);
        int ranges_ok = 1;
        for (int i = 0; i < e->reqs->ucs_range_count; i++) {
            if (e->reqs->ucs_ranges[i].first > e->reqs->ucs_ranges[i].last) {
                ranges_ok = 0;
                break;
            }
        }
        check("ucs_ranges 的 first <= last", ranges_ok);
    }
}

/* ── 行为 ─────────────────────────────────────────────────────────────── */

/* 找一个该引擎接受的字符，用于后续测试。找不到说明 accepts_char 全拒绝。 */
static int find_accepted_char(const ime_engine_t *e, void *st, char *out)
{
    for (char c = 0x20; c > 0 && c < 0x7F; c++) {
        if (e->accepts_char(st, c)) { *out = c; return 1; }
    }
    return 0;
}

static void test_behavior(const ime_engine_t *e)
{
    /* 全零 env：没有词库、没有存储。契约要求引擎必须能接受这种情况。 */
    ime_env_t env;
    memset(&env, 0, sizeof(env));

    void *st = e->create(&env);
    check("无词库无存储时 create() 成功", st != NULL);
    if (!st) return;

    const char *pre = e->preedit(st);
    check("初始 preedit 不是 NULL", pre != NULL);
    check("初始 preedit 为空串", pre && pre[0] == '\0');

    ime_candidate_t cands[MAX_CANDS];
    int n = e->candidates(st, cands, MAX_CANDS);
    check("空输入时候选数为 0", n == 0);

    check("空状态下 backspace 返回 false", e->backspace(st) == false);

    char c;
    if (!find_accepted_char(e, st, &c)) {
        check("accepts_char 至少接受一个可打印 ASCII", 0);
        e->destroy(st);
        return;
    }

    /* accepts_char 与 feed 必须一致：说不接受就不能接受 */
    int consistent = 1;
    for (char t = 0x20; t > 0 && t < 0x7F; t++) {
        if (!e->accepts_char(st, t)) {
            if (e->feed(st, t)) { consistent = 0; break; }
        }
    }
    check("accepts_char 为 false 的字符 feed 也必须失败", consistent);
    e->reset(st);

    check("feed 一个被接受的字符返回 true", e->feed(st, c) == true);
    pre = e->preedit(st);
    check("feed 后 preedit 非空", pre && pre[0] != '\0');
    check("preedit 是合法 UTF-8", pre && valid_utf8(pre, strlen(pre)));

    check("feed 后 backspace 返回 true", e->backspace(st) == true);
    pre = e->preedit(st);
    check("backspace 回到空 preedit", pre && pre[0] == '\0');

    /* 灌到满：不能崩，也不能无限接受 */
    int fed = 0;
    for (int i = 0; i < 4096; i++) {
        if (!e->feed(st, c)) break;
        fed++;
    }
    check("输入有长度上限（不会无限接受）", fed < 4096);
    pre = e->preedit(st);
    check("满载时 preedit 仍是合法 UTF-8", pre && valid_utf8(pre, strlen(pre)));

    n = e->candidates(st, cands, MAX_CANDS);
    check("候选数在 [0, max] 内", n >= 0 && n <= MAX_CANDS);
    int cand_ok = 1;
    for (int i = 0; i < n; i++) {
        if (!cands[i].text || cands[i].len == 0 ||
            !valid_utf8(cands[i].text, cands[i].len)) {
            cand_ok = 0;
            break;
        }
    }
    check("每个候选都非空且是合法 UTF-8", cand_ok);

    e->reset(st);
    pre = e->preedit(st);
    check("reset 后 preedit 为空", pre && pre[0] == '\0');
    check("reset 后候选数为 0", e->candidates(st, cands, MAX_CANDS) == 0);

    /* commit：不管返回什么都不能崩，返回非 NULL 时 len 必须有效且 UTF-8 合法 */
    e->feed(st, c);
    size_t len = 12345;                 /* 故意填脏值，看引擎是否真的写了 */
    const char *out = e->commit(st, -1, &len);
    if (out) {
        check("commit(-1) 返回文本时 len 已被写入", len != 12345 && len > 0);
        check("commit 输出是合法 UTF-8", valid_utf8(out, len));
    } else {
        check("commit(-1) 返回 NULL 也是合法的（不强制支持直接提交）", 1);
    }

    e->reset(st);
    out = e->commit(st, -1, &len);
    check("空状态下 commit 返回 NULL", out == NULL);

    /* 越界索引不能崩 */
    e->feed(st, c);
    out = e->commit(st, 9999, &len);
    check("越界候选索引不崩溃", 1);
    (void)out;

    e->destroy(st);
}

ime_conformance_result_t ime_conformance_run(const ime_engine_t *e, int verbose)
{
    s_pass = s_fail = 0;
    s_verbose = verbose;

    printf("  引擎 %s (%s, %s)\n",
           e->id ? e->id : "(无 id)",
           e->display_name ? e->display_name : "?",
           e->lang_tag ? e->lang_tag : "?");

    test_metadata(e);
    if (e->create && e->destroy) test_behavior(e);

    ime_conformance_result_t r = { s_pass, s_fail };
    printf("  -> %d 通过, %d 失败\n", r.passed, r.failed);
    return r;
}
