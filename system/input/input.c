#include "input.h"
#include "typeflow_v1.h"
#include "i2c_control.h"

#include <ctype.h>
#include <string.h>
#include "aw9523.h"
#include "kbmatrix.h"
#include "esp_log.h"

static const char *TAG = "input";

static input_cb_t  s_cb  = NULL;
static void       *s_ctx = NULL;
static uint8_t     s_mods = 0;

static aw9523_handle_t s_expander = NULL;

/* ── 键位表 ───────────────────────────────────────────────────────────────
 * [row][col] -> 键值。第一层是普通层，第二层是 Fn 层。
 *
 * ⚠ 全是占位内容：真实布局要等键盘 PCB 定稿。填表方法是先烧 debug/t_kbmatrix，
 *   逐个按键记下它报的 (row, col)，再照抄进来。
 * ---------------------------------------------------------------------- */
static const char s_keymap_base[BSP_KB_ROWS][BSP_KB_COLS] = {
    { 'q','w','e','r','t','y','u','i' },
    { 'o','p','a','s','d','f','g','h' },
    { 'j','k','l','z','x','c','v','b' },
    { 'n','m','1','2','3','4','5','6' },
    { '7','8','9','0','-','=','[',']' },
    { ';','\'','`',',','.','/','\\',0  },
    {  0,  0,  0,  0,  0,  0,  0,  0  },
    {  0,  0,  0,  0,  0,  0,  0,  0  },
};

/* 功能键位置，同样是占位值 */
#define IS_BACKSPACE(r, c)  ((r) == 6 && (c) == 0)
#define IS_ENTER(r, c)      ((r) == 6 && (c) == 1)
#define IS_SPACE(r, c)      ((r) == 6 && (c) == 2)
#define IS_SHIFT(r, c)      ((r) == 7 && (c) == 0)
#define IS_FN(r, c)         ((r) == 7 && (c) == 1)

/* ── kbmatrix 回调：翻译成语义事件 ────────────────────────────────────── */

static void on_matrix(const kbmatrix_event_t *m, void *ctx)
{
    (void)ctx;

    /* 修饰键只改状态，不往上报 */
    if (IS_SHIFT(m->row, m->col)) {
        if (m->pressed) s_mods |= INPUT_MOD_SHIFT; else s_mods &= (uint8_t)~INPUT_MOD_SHIFT;
        return;
    }
    if (IS_FN(m->row, m->col)) {
        if (m->pressed) s_mods |= INPUT_MOD_FN;    else s_mods &= (uint8_t)~INPUT_MOD_FN;
        return;
    }

    input_event_t ev = { .modifiers = s_mods, .pressed = m->pressed };

    if      (IS_BACKSPACE(m->row, m->col)) ev.key = INPUT_KEY_BACKSPACE;
    else if (IS_ENTER(m->row, m->col))     ev.key = INPUT_KEY_ENTER;
    else if (IS_SPACE(m->row, m->col))     ev.key = INPUT_KEY_SPACE;
    else {
        char c = s_keymap_base[m->row][m->col];
        if (c == 0) return;                 /* 空位，键盘上没有这个键 */

        /* 数字键在有编码串时应当是「选第 n 个候选」，但那是输入法的上下文判断，
         * 这一层不该知道。所以统一报 CHAR，由 app/candbar 决定怎么解释。 */
        ev.key = INPUT_KEY_CHAR;
        ev.ch  = (s_mods & INPUT_MOD_SHIFT) ? (char)toupper((unsigned char)c) : c;
    }

    if (s_cb) s_cb(&ev, s_ctx);
}

/* ── 初始化 ───────────────────────────────────────────────────────────── */

esp_err_t input_init(input_cb_t cb, void *ctx)
{
    s_cb  = cb;
    s_ctx = ctx;

    esp_err_t ret = i2c_control_init();
    if (ret != ESP_OK) return ret;

    aw9523_config_t exp_cfg = {
        .i2c_addr     = BSP_AW9523_ADDR_0,
        .rst_pin      = BSP_AW9523_PIN_RST,
        .scl_speed_hz = BSP_I2C_FREQ_HZ,
    };
    ret = aw9523_new(i2c_control_get_bus(), &exp_cfg, &s_expander);
    if (ret != ESP_OK) {
        ESP_LOGE(TAG, "AW9523 初始化失败，键盘不可用");
        i2c_control_scan();     /* 顺手扫一遍总线，日志里能看出是不是地址不对 */
        return ret;
    }

    /* P0 驱动行，P1 读列。加第二片时把它的 P0 接到 col_src[1] 并把 cols 改成 16。 */
    kbmatrix_config_t kb = {
        .rows = BSP_KB_ROWS,
        .cols = BSP_KB_COLS,
        .row_src       = { .dev = s_expander, .port = AW9523_PORT_P0 },
        .col_src[0]    = { .dev = s_expander, .port = AW9523_PORT_P1 },
        .col_src_count = 1,
        .int_pin          = BSP_AW9523_PIN_INT,
        .debounce_ms      = 8,
        .scan_interval_ms = 5,
    };
    return kbmatrix_init(&kb, on_matrix, NULL);
}
