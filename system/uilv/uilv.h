/*
 * uilv — LVGL UI 层
 *
 * 目录规划（沿用 DeepStoa\system\uilv 的分法）：
 *   framework/  页面导航、生命周期
 *   widgets/    自制控件（候选条、状态指示）
 *   theme/      配色与样式
 *   utils/      字体加载、文本测量
 *   fonts/      由 tools/c_font.py 裁剪生成的中日文字库 .c
 *
 * ════════════════════════════════════════════════════════════════════════
 *  当前本组件不依赖 LVGL —— 这是刻意的，不是遗漏。
 * ════════════════════════════════════════════════════════════════════════
 * 接 LVGL 是独立的一步，要做的事：
 *   1. main/idf_component.yml 加 `lvgl/lvgl: "~9.2"`
 *   2. idf.py menuconfig 里设 LV_COLOR_DEPTH=16，并按 76x284 调 draw buffer
 *   3. 本 CMakeLists 的 REQUIRES 加 lvgl，并改用 file(GLOB_RECURSE) 收各子目录
 *      源码（写法照抄 DeepStoa\system\uilv\CMakeLists.txt）
 *   4. ⚠ GLOB 只在 CMake configure 阶段求值：新增 .c 后必须 idf.py reconfigure，
 *      否则表现为链接期 undefined reference。DeepStoa 那份注释里专门记了这一条。
 *
 * 先不引入的原因：LVGL 要配 lv_conf、拉托管依赖，是个独立里程碑；
 * 而在它就位之前，输入法内核、键盘扫描、hidout 都能各自推进和验证。
 */

#ifndef UILV_H
#define UILV_H

#include "esp_err.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 初始化 LVGL、绑定 display_control 的面板、起 tick 任务。 */
esp_err_t uilv_init(void);

#ifdef __cplusplus
}
#endif

#endif /* UILV_H */
