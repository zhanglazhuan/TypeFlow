/*
 * engine_api.h — 输入法引擎插件契约
 *
 * ════════════════════════════════════════════════════════════════════════
 *  想加一门语言？这是你唯一必须读完的文件。
 *  上手步骤见 docs/adding-a-language.md，最快的起点是复制 engines/_template/。
 * ════════════════════════════════════════════════════════════════════════
 *
 * 纪律：引擎只允许包含 C 标准库。不许出现 esp_ 开头的头文件，也不许包含
 * freertos 或 driver 目录下的任何头文件。
 *
 * 这条不是洁癖，是为了你：守住它，你就能用普通 gcc 编译和测试自己的引擎，
 * 不需要 ESP-IDF、不需要开发板、也不需要那块定制键盘 PCB
 * （见 tools/ime-test/）。平台相关的东西全部通过 ime_env_t 注入。
 *
 * ── 一个重要的设计前提 ──────────────────────────────────────────────────
 * 本接口**不假设**"编码 → 查词库 → 选候选"这条路径。
 *
 * 韩文谚文是 jamo 机械合字、越南语 Telex 是确定性变音符放置、泰文是直接映射
 * —— 这些输入法根本没有候选，输入即输出。所以：
 *
 *   - candidates() 返回 0 是完全正常的情况，不是错误
 *   - commit() 收到 index < 0 表示"把当前 preedit 直接提交"
 *   - env->dict.size == 0 表示没有词库，引擎必须能正常工作
 *
 * 如果你的语言不需要词库，上面三条让你可以写一个非常短的引擎。
 */

#ifndef IME_ENGINE_API_H
#define IME_ENGINE_API_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>

#ifdef __cplusplus
extern "C" {
#endif

/* ── 候选项 ───────────────────────────────────────────────────────────── */
typedef struct {
    const char *text;   /* UTF-8。指向引擎内部缓冲，下次调用本引擎即失效 */
    uint16_t    len;    /* 字节数，不含结尾 NUL */
} ime_candidate_t;

/* ── 词库访问 ─────────────────────────────────────────────────────────────
 * 只能看到**属于本引擎**的那一段。多语言共享同一个 6MB flash 分区，
 * 容器格式和 TOC 由 tools/dict_build.py 负责，引擎不用关心。
 *
 * map() 的形状是「给 offset 要指针」而不是「返回整块基地址」：这样宿主端
 * 若改成按页分段映射，引擎代码一行都不用动。
 * ---------------------------------------------------------------------- */
typedef struct {
    const uint8_t *(*map)(void *ctx, uint32_t offset, uint32_t len);
    uint32_t       size;    /* 0 = 本引擎没有词库，必须能正常工作 */
    void          *ctx;
} ime_dict_t;

/* ── 用户数据（词频、自造词）─────────────────────────────────────────────
 * 可写存储在 MCU 上是 flash，写次数有限。别每次选词都 save()，
 * 在内存里累积、择机落盘。
 * ---------------------------------------------------------------------- */
typedef struct {
    int  (*load)(void *ctx, const char *key, void *buf, int buf_len);
    int  (*save)(void *ctx, const char *key, const void *buf, int len);
    void *ctx;              /* load == NULL 表示不提供，引擎应跳过自学习 */
} ime_store_t;

/** @brief create() 时注入的环境。 */
typedef struct {
    ime_dict_t  dict;
    ime_store_t store;
} ime_env_t;

/* ── 资源与字形需求声明 ───────────────────────────────────────────────────
 * ★ ucs_ranges 很容易被忽略，但漏了它你的字在屏幕上全是方框。
 *
 * 长条屏靠 flash 里的点阵字体渲染，字体是构建时按**所有启用引擎的区段并集**
 * 裁出来的（tools/font_build.py）。你不声明谚文/泰文/西里尔区段，
 * 字体里就没有这些字形。
 * ---------------------------------------------------------------------- */
typedef struct {
    uint32_t first;
    uint32_t last;      /* 闭区间 */
} ime_ucs_range_t;

typedef struct {
    const ime_ucs_range_t *ucs_ranges;
    uint8_t                ucs_range_count;
    uint32_t               dict_bytes_max;  /* 词库预算，0 = 不需要词库 */
    uint32_t               ram_bytes_max;   /* create() 后常驻 RAM 估计 */
} ime_reqs_t;

/* ════════════════════════════════════════════════════════════════════════
 * 引擎 vtable
 * ════════════════════════════════════════════════════════════════════════
 * 引擎**自己持有全部输入状态**（编码缓冲、合字进度等）。core 不替你存
 * 输入串 —— 因为各语言的状态差别太大：拼音是字母缓冲，谚文是初/中/终声槽位，
 * Telex 是带变音标记的音节。让你自己管反而简单。
 *
 * core 替你做的是：引擎注册与切换、候选**分页**、提交缓冲的生命周期。
 * 所以 candidates() 一次返回你能给出的全部候选，翻页不用你操心。
 * ------------------------------------------------------------------------ */
typedef struct ime_engine {
    /* id 必须稳定且全局唯一：它同时是词库容器里的键、以及用户配置里存的值。
     * 一旦发布就别改，改了用户的设置和词库都会对不上。用小写加下划线。 */
    const char *id;             /* "pinyin" / "hangul" / "telex" */
    const char *display_name;   /* 给人看的名字，可含非 ASCII："拼音" */
    const char *lang_tag;       /* BCP-47："zh-Hans" / "ko" / "vi" */

    const ime_reqs_t *reqs;     /* 不可为 NULL */

    /** @brief 创建实例。env 在实例生命周期内保持有效，可以存指针。
     *  @return NULL 表示创建失败（内存不足、词库格式不认识等） */
    void *(*create)(const ime_env_t *env);
    void  (*destroy)(void *st);

    /** @brief 这个字符属于本输入法的编码吗？不属于的话 core 会原样透传给主机。 */
    bool  (*accepts_char)(void *st, char c);

    /** @brief 追加一个编码字符。返回 false = 缓冲满或该字符此刻不合法。 */
    bool  (*feed)(void *st, char c);

    /** @brief 退一格。返回 false = 已经空了（core 据此把退格透传给主机）。 */
    bool  (*backspace)(void *st);

    /** @brief 清空所有输入状态。 */
    void  (*reset)(void *st);

    /** @brief 当前 preedit（用户在候选条上看到的编码串），UTF-8。
     *  空串表示"没有正在输入"。返回的指针要到下次调用本引擎前保持有效。 */
    const char *(*preedit)(void *st);

    /** @brief 取候选，按你认为最合适的顺序（通常按词频降序）。
     *  @return 写入个数。**返回 0 是合法的** —— 不需要候选的输入法就这么做。 */
    int   (*candidates)(void *st, ime_candidate_t *out, int max);

    /**
     * @brief 提交。
     *
     * @param index >= 0 选 candidates() 里的第 index 个（core 已把翻页换算好）；
     *              < 0  表示直接提交当前 preedit（无候选的输入法走这条）
     * @param len   输出 UTF-8 字节数
     * @return 提交的文本；NULL 表示没什么可提交。
     *
     * 提交后引擎要自己更新输入状态：整串转完就清空，只转掉前缀就保留剩余部分
     * （"woshi" 选了"我"之后应该剩 "shi" 继续出候选）。core 会重新调 preedit()
     * 来判断是否还在输入中，所以你不用告诉它消耗了多少。
     */
    const char *(*commit)(void *st, int index, size_t *len);
} ime_engine_t;

/* ── 注册 ─────────────────────────────────────────────────────────────────
 * **没有中心注册文件要改。** 构建时会扫 app/ime/engines/ 下的每个目录，
 * 按约定生成注册表（tools/gen_ime_registry.py）。你只要：
 *
 *   1. 建目录 app/ime/engines/<你的id>/
 *   2. 在里面实现下面这个函数，名字必须是 ime_engine_get_<目录名>
 *   3. 目录里的所有 .c 会被自动编进来
 *
 * 就这样 —— 不碰任何现有文件，所以不会和别人的 PR 冲突。
 *
 * ⚠ 新增目录后要跑一次 `idf.py reconfigure`：CMake 的目录扫描只在 configure
 *   阶段求值，不然表现为链接期找不到你的引擎。
 * ---------------------------------------------------------------------- */
typedef const ime_engine_t *(*ime_engine_getter_t)(void);

#ifdef __cplusplus
}
#endif

#endif /* IME_ENGINE_API_H */
