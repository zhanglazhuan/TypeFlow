/*
 * input — 矩阵坐标 -> 语义按键
 *
 * drivers/kbmatrix 只报 (row, col)。这一层负责：
 *   - 查键位表得到键值
 *   - 处理修饰键状态（Shift/Ctrl/Alt/Fn）
 *   - 识别输入法专用键（选词 1~9、翻页、中英切换、中日切换）
 *   - 长按/连击/重复（按住退格要连续删）
 *
 * 键位表和物理布局绑定，所以放在这里而不是 drivers 里 —— 换一版键盘 PCB
 * 只需要改这张表，扫描代码不用动。
 */

#ifndef INPUT_H
#define INPUT_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 语义按键类型。 */
typedef enum {
    INPUT_KEY_NONE = 0,
    INPUT_KEY_CHAR,         /* 可打印字符，见 input_event_t.ch */
    INPUT_KEY_BACKSPACE,
    INPUT_KEY_ENTER,
    INPUT_KEY_SPACE,        /* 输入法状态下 = 选第一个候选 */
    INPUT_KEY_ESC,          /* 取消当前编码串 */
    INPUT_KEY_TAB,
    INPUT_KEY_ARROW_LEFT,
    INPUT_KEY_ARROW_RIGHT,
    INPUT_KEY_ARROW_UP,
    INPUT_KEY_ARROW_DOWN,

    /* 输入法专用 */
    INPUT_KEY_CAND_SELECT,  /* 选第 n 个候选，n 在 .index */
    INPUT_KEY_PAGE_NEXT,
    INPUT_KEY_PAGE_PREV,
    INPUT_KEY_MODE_TOGGLE,  /* 中/英切换 */
    INPUT_KEY_LANG_TOGGLE,  /* 中/日切换 */
    INPUT_KEY_LINK_TOGGLE,  /* USB/BLE 通道切换 */
} input_key_t;

/** @brief 修饰键位图，与 HID 定义一致便于直接透传。 */
#define INPUT_MOD_CTRL   0x01
#define INPUT_MOD_SHIFT  0x02
#define INPUT_MOD_ALT    0x04
#define INPUT_MOD_GUI    0x08
#define INPUT_MOD_FN     0x80   /* Fn 是本机自用，不发给主机 */

typedef struct {
    input_key_t key;
    char        ch;         /* key == INPUT_KEY_CHAR 时有效 */
    uint8_t     index;      /* key == INPUT_KEY_CAND_SELECT 时的候选序号(0起) */
    uint8_t     modifiers;  /* 按下时的修饰键状态 */
    bool        pressed;    /* true = 按下/重复, false = 松开 */
} input_event_t;

typedef void (*input_cb_t)(const input_event_t *ev, void *ctx);

/** @brief 初始化键盘链路：建 AW9523、起 kbmatrix、装键位表。 */
esp_err_t input_init(input_cb_t cb, void *ctx);

#ifdef __cplusplus
}
#endif

#endif /* INPUT_H */
