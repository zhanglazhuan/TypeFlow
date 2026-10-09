/*
 * ime-test — 在 PC 上跑输入法引擎，不需要 ESP-IDF、不需要硬件
 *
 * 这是贡献者的主要工作流。能这样做的唯一原因是 app/ime 守住了
 * 「只依赖 C 标准库」那条纪律 —— 见 app/ime/engine_api.h 文件头。
 *
 * 用法：
 *   make
 *   ./ime-test                     列出所有引擎
 *   ./ime-test check               对所有引擎跑一致性测试
 *   ./ime-test check <id>          只测一个
 *   ./ime-test type <id> <keys>    喂一串按键，打印 preedit / 候选 / 提交结果
 *
 * 最后那个命令是写引擎时最常用的：
 *   ./ime-test type template thalassa
 *   ./ime-test type pinyin nihao
 */

#include "ime.h"
#include "conformance.h"

#include <stdio.h>
#include <string.h>

#define MAX_CANDS 32

static void list_engines(void)
{
    int n = ime_engine_count();
    printf("已编入 %d 个引擎：\n", n);
    for (int i = 0; i < n; i++) {
        const ime_engine_t *e = ime_engine_at(i);
        printf("  %-12s %-20s %-8s  词库上限 %u KB, RAM %u B\n",
               e->id, e->display_name, e->lang_tag,
               (unsigned)(e->reqs->dict_bytes_max / 1024),
               (unsigned)e->reqs->ram_bytes_max);
        printf("               字形区段:");
        for (int r = 0; r < e->reqs->ucs_range_count; r++) {
            printf(" U+%04X..U+%04X",
                   (unsigned)e->reqs->ucs_ranges[r].first,
                   (unsigned)e->reqs->ucs_ranges[r].last);
        }
        printf("\n");
    }
}

static int cmd_check(const char *only_id)
{
    int total_fail = 0, engines = 0;

    for (int i = 0; i < ime_engine_count(); i++) {
        const ime_engine_t *e = ime_engine_at(i);
        if (only_id && strcmp(e->id, only_id) != 0) continue;
        engines++;
        ime_conformance_result_t r = ime_conformance_run(e, 0);
        total_fail += r.failed;
        printf("\n");
    }

    if (engines == 0) {
        printf("没有匹配的引擎: %s\n", only_id ? only_id : "(全部)");
        return 1;
    }
    if (total_fail) {
        printf("一致性测试未通过：%d 项失败\n", total_fail);
        return 1;
    }
    printf("%d 个引擎全部通过一致性测试\n", engines);
    return 0;
}

static int cmd_type(const char *id, const char *keys)
{
    ime_t *im = ime_create(NULL);    /* NULL provider：无词库，测纯合字逻辑 */
    if (!im) {
        printf("创建内核失败\n");
        return 1;
    }
    if (!ime_select_engine(im, id)) {
        printf("没有这个引擎: %s\n", id);
        ime_destroy(im);
        return 1;
    }

    printf("引擎 %s，输入 \"%s\"\n", id, keys);
    for (const char *p = keys; *p; p++) {
        if (!ime_feed_char(im, *p)) {
            printf("  '%c' 被拒绝（不是本输入法的编码字符）\n", *p);
            continue;
        }
        printf("  '%c' -> preedit=\"%s\"", *p, ime_get_composition(im));

        ime_candidate_t c[MAX_CANDS];
        int n = ime_get_candidates(im, c, MAX_CANDS);
        if (n > 0) {
            printf("  候选:");
            for (int i = 0; i < n; i++) printf(" %d.%.*s", i + 1, c[i].len, c[i].text);
        }
        printf("\n");
    }

    size_t len = 0;
    int total = ime_candidate_total(im);
    const char *out = ime_commit(im, total > 0 ? 0 : -1, &len);
    printf("提交 -> \"%s\" (%u 字节)\n", out ? out : "(无)", (unsigned)len);

    ime_destroy(im);
    return 0;
}

int main(int argc, char **argv)
{
    if (argc < 2) {
        list_engines();
        printf("\n用 './ime-test check' 跑一致性测试，"
               "'./ime-test type <id> <keys>' 试打字\n");
        return 0;
    }

    if (strcmp(argv[1], "check") == 0) {
        return cmd_check(argc >= 3 ? argv[2] : NULL);
    }
    if (strcmp(argv[1], "type") == 0 && argc >= 4) {
        return cmd_type(argv[2], argv[3]);
    }

    printf("未知命令。直接运行不带参数可看用法。\n");
    return 1;
}
