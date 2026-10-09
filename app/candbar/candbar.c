#include "candbar.h"
#include "ime.h"
#include "storage.h"
#include "hidout.h"

#include <inttypes.h>
#include <string.h>
#include "esp_log.h"

static const char *TAG = "candbar";

static ime_t *s_ime = NULL;

/* 中英切换：输入法「关」的时候按键原样透传给主机。
 * 刻意不做成一个 "ascii" 引擎 —— 英文直通不需要 preedit、不需要候选，
 * 它是输入法的缺席而不是输入法的一种。造个假引擎只会让 core 多处特判。 */
static bool s_ime_enabled = true;

/* ── 把 storage 的裸字节接口包成引擎要的 env ──────────────────────────────
 * 这层包装放在 app 层而不是 system/storage 里，是为了让依赖方向只有
 * app -> system 一个朝向。让 system/storage 直接返回 ime 的结构体会更省事，
 * 但那样系统层就反过来 #include 了应用层的头文件。
 *
 * get_env 按引擎 id 回调：多语言共享同一个 6MB dict 分区，将来容器格式定了
 * 之后，这里要按 id 从 TOC 里查出属于该引擎的那一段，而不是把整块给它。
 * ---------------------------------------------------------------------- */

static const uint8_t *dict_map_cb(void *ctx, uint32_t offset, uint32_t len)
{
    (void)ctx;
    return storage_dict_map(offset, len);
}

static int user_load_cb(void *ctx, const char *key, void *buf, int buf_len)
{
    (void)ctx;
    return storage_user_load(key, buf, buf_len);
}

static int user_save_cb(void *ctx, const char *key, const void *buf, int len)
{
    (void)ctx;
    return storage_user_save(key, buf, len);
}

static void get_env_cb(void *ctx, const char *engine_id, ime_env_t *out)
{
    (void)ctx;
    memset(out, 0, sizeof(*out));

    out->store.load = user_load_cb;
    out->store.save = user_save_cb;

    /* TODO 词库容器：现在整块 dict 分区直接给每个引擎，等 tools/dict_build.py
     * 定下 TOC 格式后改成按 engine_id 查出对应那一段。多语言共存必须做这步，
     * 否则两门语言的词库会互相覆盖。 */
    uint32_t size = storage_dict_size();
    if (size > 0) {
        out->dict.map  = dict_map_cb;
        out->dict.size = size;
    }
    ESP_LOGD(TAG, "引擎 %s 的 env：词库 %" PRIu32 " 字节", engine_id, size);
}

/* ── 初始化 ───────────────────────────────────────────────────────────── */

esp_err_t candbar_init(void)
{
    const ime_env_provider_t provider = {
        .get_env = get_env_cb,
        .ctx     = NULL,
    };

    s_ime = ime_create(&provider);
    if (!s_ime) {
        ESP_LOGE(TAG, "创建输入法内核失败（一个引擎都没编进来？）");
        return ESP_ERR_NO_MEM;
    }

    /* TODO 从 settings 读用户上次选的引擎 id。现在用注册表里的第一个。 */
    const ime_engine_t *e = ime_current_engine(s_ime);
    ESP_LOGI(TAG, "候选条就绪，共 %d 个输入法，当前：%s (%s)",
             ime_engine_count(), e->display_name, e->id);

    uint32_t dict_size = storage_dict_size();
    if (dict_size == 0) {
        ESP_LOGW(TAG, "词库分区为空 —— 需要候选词的输入法不会出候选");
    }
    return ESP_OK;
}

/* ── 提交 ─────────────────────────────────────────────────────────────── */

static void commit(int index)
{
    size_t len = 0;
    const char *text = ime_commit(s_ime, index, &len);
    if (!text || len == 0) return;

    esp_err_t ret = hidout_send_text(text, len);
    if (ret != ESP_OK) {
        ESP_LOGW(TAG, "发送失败: %s（见 hidout.h 文件头关于文本提交的说明）",
                 esp_err_to_name(ret));
    }
    /* TODO 通知 UI 重绘 */
}

/* 切到下一个输入法。引擎顺序由注册表决定（目录名排序）。 */
static void cycle_engine(void)
{
    int n = ime_engine_count();
    if (n <= 1) return;

    const ime_engine_t *cur = ime_current_engine(s_ime);
    int idx = 0;
    for (int i = 0; i < n; i++) {
        const ime_engine_t *e = ime_engine_at(i);
        if (e == cur) { idx = i; break; }
    }
    const ime_engine_t *next = ime_engine_at((idx + 1) % n);
    if (next && ime_select_engine(s_ime, next->id)) {
        ESP_LOGI(TAG, "切换输入法 -> %s", next->display_name);
    }
}

/* ── 按键处理 ─────────────────────────────────────────────────────────── */

void candbar_handle_key(const input_event_t *ev)
{
    if (!s_ime || !ev->pressed) return;

    /* 中英切换和输入法切换在任何状态下都要响应，所以先处理 */
    if (ev->key == INPUT_KEY_MODE_TOGGLE) {
        s_ime_enabled = !s_ime_enabled;
        if (!s_ime_enabled) ime_clear(s_ime);
        ESP_LOGI(TAG, "输入法 %s", s_ime_enabled ? "开" : "关（英文直通）");
        return;
    }
    if (ev->key == INPUT_KEY_LANG_TOGGLE) {
        ime_clear(s_ime);
        cycle_engine();
        return;
    }

    /* 输入法关闭时全部原样透传 */
    if (!s_ime_enabled) {
        if (ev->key == INPUT_KEY_CHAR)      hidout_send_text(&ev->ch, 1);
        else if (ev->key == INPUT_KEY_SPACE) hidout_send_text(" ", 1);
        else if (ev->key == INPUT_KEY_BACKSPACE) hidout_send_key(0, 0x2A);
        else if (ev->key == INPUT_KEY_ENTER)     hidout_send_key(0, 0x28);
        return;
    }

    const bool composing = ime_is_composing(s_ime);
    const int  total     = ime_candidate_total(s_ime);

    switch (ev->key) {
        case INPUT_KEY_CHAR:
            /* 有候选时数字键 = 选候选。这个上下文判断必须在这一层做 ——
             * system/input 不该认识输入法状态，ime core 不该认识键盘布局。 */
            if (composing && total > 0 && ev->ch >= '1' && ev->ch <= '9') {
                commit(ev->ch - '1');
            } else if (!ime_feed_char(s_ime, ev->ch)) {
                /* 当前引擎不吃这个字符（不是它的编码字母），直接透传 */
                hidout_send_text(&ev->ch, 1);
            }
            break;

        case INPUT_KEY_SPACE:
            if (!composing) {
                hidout_send_text(" ", 1);
            } else if (total > 0) {
                commit(0);          /* 有候选：空格选第一个 */
            } else {
                commit(-1);         /* 无候选的输入法（谚文/转写）：直接上屏 */
            }
            break;

        case INPUT_KEY_BACKSPACE:
            if (!ime_backspace(s_ime)) {
                hidout_send_key(0, 0x2A);   /* 编码串已空，退格给主机 */
            }
            break;

        case INPUT_KEY_ESC:
            if (composing) ime_clear(s_ime);
            break;

        case INPUT_KEY_PAGE_NEXT: ime_page_next(s_ime); break;
        case INPUT_KEY_PAGE_PREV: ime_page_prev(s_ime); break;

        case INPUT_KEY_ENTER:
            if (!composing) {
                hidout_send_key(0, 0x28);
            } else if (total == 0) {
                commit(-1);         /* 无候选引擎：回车确认当前 preedit */
            } else {
                ime_clear(s_ime);   /* 有候选却按回车 = 放弃转换 */
            }
            break;

        default:
            break;
    }

    /* TODO 通知 UI 重绘候选条 */
}
