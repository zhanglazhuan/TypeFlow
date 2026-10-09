#include "display_control.h"
#include "typeflow_v1.h"

#include "driver/gpio.h"
#include "driver/spi_master.h"
#include "esp_lcd_panel_st7789.h"
#include "esp_lcd_panel_vendor.h"
#include "esp_log.h"

static const char *TAG = "disp_ctl";

static esp_lcd_panel_handle_t    s_panel = NULL;
static esp_lcd_panel_io_handle_t s_io    = NULL;

/* ── ST7789 的 76x284 窗口偏移 ─────────────────────────────────────────────
 * ST7789 的显存是 240x320。76x284 面板只用到其中一个窗口，所以必须告诉面板
 * 驱动这个窗口从哪开始，否则显示内容会整体错位、边缘出现花条。
 *
 * ⚠ 下面两个值是按「窗口居中」算出来的推测值，必须点屏实测校准：
 *     X_GAP = (240 - 76)  / 2 = 82
 *     Y_GAP = (320 - 284) / 2 = 18
 *   实际取决于玻璃如何切以及 FPC 走线，厂家给的规格书里通常直接写了起始行列。
 *   校准方法：画一个正好铺满 BSP_LCD_H_RES x BSP_LCD_V_RES 的纯色矩形，
 *   调 gap 直到四边刚好贴合、没有多余色带。
 * ------------------------------------------------------------------------ */
#define LCD_X_GAP   82      /* TODO 实测校准 */
#define LCD_Y_GAP   18      /* TODO 实测校准 */

esp_err_t display_control_init(void)
{
    if (s_panel) return ESP_OK;

    /* ── 背光 ── */
    gpio_config_t bl = {
        .pin_bit_mask = (1ULL << BSP_LCD_PIN_BL),
        .mode         = GPIO_MODE_OUTPUT,
        .intr_type    = GPIO_INTR_DISABLE,
    };
    ESP_ERROR_CHECK(gpio_config(&bl));
    gpio_set_level(BSP_LCD_PIN_BL, !BSP_LCD_BL_ON_LEVEL);   /* 初始化期间先关，避免闪白 */

    /* ── SPI 总线 ──
     * max_transfer_sz 给到一整帧，这样 LVGL 可以一次 flush 全屏。
     * 整帧只有 42KB，不需要分块传输。 */
    spi_bus_config_t bus = {
        .sclk_io_num     = BSP_LCD_PIN_SCLK,
        .mosi_io_num     = BSP_LCD_PIN_MOSI,
        .miso_io_num     = -1,
        .quadwp_io_num   = -1,
        .quadhd_io_num   = -1,
        .max_transfer_sz = BSP_LCD_FB_SIZE_BYTES,
    };
    ESP_ERROR_CHECK(spi_bus_initialize(BSP_LCD_SPI_HOST, &bus, SPI_DMA_CH_AUTO));

    esp_lcd_panel_io_spi_config_t io_cfg = {
        .dc_gpio_num       = BSP_LCD_PIN_DC,
        .cs_gpio_num       = BSP_LCD_PIN_CS,
        .pclk_hz           = BSP_LCD_PIXEL_CLOCK_HZ,
        .lcd_cmd_bits      = 8,
        .lcd_param_bits    = 8,
        .spi_mode          = 0,
        .trans_queue_depth = 10,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_io_spi(
        (esp_lcd_spi_bus_handle_t)BSP_LCD_SPI_HOST, &io_cfg, &s_io));

    esp_lcd_panel_dev_config_t panel_cfg = {
        .reset_gpio_num = BSP_LCD_PIN_RST,
        .rgb_ele_order  = LCD_RGB_ELEMENT_ORDER_RGB,  /* TODO 颜色发蓝就换 BGR */
        .bits_per_pixel = 16,
    };
    ESP_ERROR_CHECK(esp_lcd_new_panel_st7789(s_io, &panel_cfg, &s_panel));

    ESP_ERROR_CHECK(esp_lcd_panel_reset(s_panel));
    ESP_ERROR_CHECK(esp_lcd_panel_init(s_panel));

    /* 窗口偏移，非标分辨率必需 */
    ESP_ERROR_CHECK(esp_lcd_panel_set_gap(s_panel, LCD_X_GAP, LCD_Y_GAP));

    /* TODO 以下三项按实际装配方向调：镜像/交换 XY/反色 */
    ESP_ERROR_CHECK(esp_lcd_panel_mirror(s_panel, false, false));
    ESP_ERROR_CHECK(esp_lcd_panel_swap_xy(s_panel, false));
    ESP_ERROR_CHECK(esp_lcd_panel_invert_color(s_panel, true)); /* ST7789 通常需要反色 */

    ESP_ERROR_CHECK(esp_lcd_panel_disp_on_off(s_panel, true));

    ESP_LOGI(TAG, "ST7789 就绪 %dx%d gap=(%d,%d) %dMHz",
             BSP_LCD_H_RES, BSP_LCD_V_RES, LCD_X_GAP, LCD_Y_GAP,
             BSP_LCD_PIXEL_CLOCK_HZ / 1000000);
    return ESP_OK;
}

esp_lcd_panel_handle_t display_control_get_panel(void)
{
    return s_panel;
}

esp_err_t display_control_set_backlight(bool on)
{
    return gpio_set_level(BSP_LCD_PIN_BL,
                          on ? BSP_LCD_BL_ON_LEVEL : !BSP_LCD_BL_ON_LEVEL);
}

esp_err_t display_control_set_brightness(uint8_t percent)
{
    /* TODO 接 LEDC PWM 做真正的调光。现在只做开/关。 */
    return display_control_set_backlight(percent > 0);
}
