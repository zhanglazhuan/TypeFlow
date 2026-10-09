/*
 * display_control — 屏幕门面
 *
 * 上层只通过这里碰屏幕；ST7789 的 SPI 配置、gap 偏移、背光都封在实现里。
 * LCD 本体用 ESP-IDF 内置的 esp_lcd_new_panel_st7789()，所以 drivers/ 下
 * 没有 LCD 组件 —— 不重复实现官方已经维护好的东西。
 */

#ifndef DISPLAY_CONTROL_H
#define DISPLAY_CONTROL_H

#include <stdint.h>
#include "esp_err.h"
#include "esp_lcd_panel_io.h"
#include "esp_lcd_panel_ops.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 初始化 SPI、ST7789 面板、背光。成功后屏幕已清黑并点亮。 */
esp_err_t display_control_init(void);

/** @brief 取面板 handle，供 uilv 绑定 LVGL flush 回调。 */
esp_lcd_panel_handle_t display_control_get_panel(void);

/** @brief 背光开关。 */
esp_err_t display_control_set_backlight(bool on);

/** @brief 背光亮度 0~100。未接 PWM 时退化为开/关。 */
esp_err_t display_control_set_brightness(uint8_t percent);

#ifdef __cplusplus
}
#endif

#endif /* DISPLAY_CONTROL_H */
