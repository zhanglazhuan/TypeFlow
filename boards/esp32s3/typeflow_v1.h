/*
 * TypeFlow v1 — Board Pin Map (ESP32-S3 N16R8)
 *
 * 唯一的引脚真相来源。任何 .c 文件里出现裸 GPIO 号都算 bug。
 *
 * ⚠ 引脚分配尚未与原理图核对 —— 标 TODO 的都是占位值。
 *   拿到原理图后：把 PDF 放进 design/，逐项核对本文件，然后删掉 TODO 标记。
 */

#ifndef TYPEFLOW_V1_H
#define TYPEFLOW_V1_H

#include "hal/gpio_types.h"

/* ========================================================================
 * ESP32-S3 N16R8 不可用引脚 —— 别往这些脚上分配任何功能
 * ========================================================================
 *   GPIO26~32   片内 SPI Flash
 *   GPIO33~37   八线(Octal) PSRAM ★ R8 特有：QUAD 版本不占这些脚，R8 占
 *   GPIO0/45/46 strapping，上电电平决定启动模式
 *   GPIO43/44   UART0 控制台 (见 sdkconfig.defaults)
 *   GPIO19/20   USB D-/D+，做 HID 时固定占用
 *
 * 可自由使用：GPIO1~18, 21, 38~42, 47, 48  —— 约 26 个。
 * 全尺寸键盘矩阵单靠这些脚不够，所以走 AW9523 扩展（见下）。
 * ======================================================================== */

/* ========================================================================
 * 显示：76x284 SPI 彩屏，ST7789 控制器
 * ========================================================================
 * 面板是 ST7789 全尺寸(240x320)显存里的一个窗口，所以必须设 gap 偏移，
 * 否则画面会整体错位。偏移量要点屏时实测，见 system/controller/display_control.c。
 * ======================================================================== */
#define BSP_LCD_SPI_HOST        SPI2_HOST
#define BSP_LCD_PIN_SCLK        GPIO_NUM_12   /* TODO 待核对原理图 */
#define BSP_LCD_PIN_MOSI        GPIO_NUM_11   /* TODO */
#define BSP_LCD_PIN_CS          GPIO_NUM_10   /* TODO */
#define BSP_LCD_PIN_DC          GPIO_NUM_9    /* TODO */
#define BSP_LCD_PIN_RST         GPIO_NUM_8    /* TODO */
#define BSP_LCD_PIN_BL          GPIO_NUM_7    /* TODO 背光，PWM 调亮度 */

#define BSP_LCD_H_RES           76
#define BSP_LCD_V_RES           284
#define BSP_LCD_PIXEL_CLOCK_HZ  (40 * 1000 * 1000)
#define BSP_LCD_BL_ON_LEVEL     1

/* 一整帧 RGB565 只有 76*284*2 = 42 KB，双缓冲 84 KB，内部 SRAM 完全放得下。
 * 所以候选条每次全量重绘即可，不需要脏区域追踪 —— 别把墨水屏那套局刷策略搬来。 */
#define BSP_LCD_FB_SIZE_BYTES   (BSP_LCD_H_RES * BSP_LCD_V_RES * 2)

/* ========================================================================
 * I2C：挂 AW9523 IO 扩展芯片
 * ======================================================================== */
#define BSP_I2C_PORT            0
#define BSP_I2C_PIN_SDA         GPIO_NUM_4    /* TODO */
#define BSP_I2C_PIN_SCL         GPIO_NUM_5    /* TODO */
#define BSP_I2C_FREQ_HZ         400000

/* ========================================================================
 * 键盘矩阵：经 AW9523 扩展
 * ========================================================================
 * 一片 AW9523 提供 16 个 IO（P0.0~P0.7 / P1.0~P1.7），可做 8x8 = 64 键。
 * AW9523 有 4 个可选地址 (0x58~0x5B)，不够就加第二片：
 *   #1 P0 = 8 行(输出驱动)， #1 P1 = 前 8 列(输入)
 *   #2 P0 = 后 8 列(输入)  → 8x16 = 128 键
 *
 * INT 脚是性能关键：AW9523 支持引脚电平变化中断，空闲时靠 INT 唤醒扫描，
 * 不要用定时器一直轮询 I2C。整轮扫描 = 8 次写行 + N 次读列，400 kHz 下约 1~2 ms，
 * 所以扫描频率上限约 100~200 Hz（打字速度够用），别想着做 1 kHz 轮询。
 * ======================================================================== */
#define BSP_AW9523_ADDR_0       0x58
#define BSP_AW9523_PIN_RST      GPIO_NUM_3    /* TODO 低有效；无此脚填 GPIO_NUM_NC */
#define BSP_AW9523_PIN_INT      GPIO_NUM_6    /* TODO 低有效，按键变化中断 */

/* 第二片 AW9523：硬件上焊了就打开，128 键矩阵需要它 */
/* #define BSP_AW9523_ADDR_1    0x59 */

#define BSP_KB_ROWS             8
#define BSP_KB_COLS             8             /* 加第二片后改 16 */

/* ========================================================================
 * 电池 / 电源
 * ======================================================================== */
#define BSP_BAT_ADC_PIN         GPIO_NUM_1    /* TODO ADC1_CH0，分压后测电压 */
#define BSP_BAT_ADC_MULTIPLIER  2.0f          /* TODO 按实际分压电阻改 */

/* ========================================================================
 * USB：必须支持双角色(DRP)
 * ========================================================================
 * GPIO19/20 由 USB-OTG 外设固定占用，无需在代码里配置，此处仅作记录。
 *
 * ⚠ 硬件设计要点：两个目标品牌需要的 USB 角色是**相反**的
 *
 *   BOOX        平板是 Android，能当 OTG host  -> TypeFlow 当 device，VBUS 取电
 *   reMarkable  平板切不到 host(2025.1 起被禁)，但它默认是 RNDIS gadget
 *               -> TypeFlow 当 host，必须**给平板供 VBUS**它才会枚举
 *
 * ESP32-S3 只有一个 USB-OTG 外设，两种角色不能并存，只能按目标品牌切。
 * 所以原理图上 USB-C 口要能双向：
 *   1. VBUS 开关（负载开关 / OTG 升压），host 模式下对外输出 5V
 *   2. CC 上下拉可切换，或至少用 ID 引脚区分角色
 *   3. 限流要想清楚：平板有自己的电池，只需最小枚举电流，不要去给它充电 ——
 *      便携键盘的电池扛不住，详见 system/hidout/hidout_usbhost.c 文件头
 *
 * 备选方案是 Y 线从外部取 5V，那样 TypeFlow 不必供电，但用户要多带根线。
 * **这条要在原理图定稿前决定。**
 * ======================================================================== */
#define BSP_USB_PIN_DM          GPIO_NUM_19
#define BSP_USB_PIN_DP          GPIO_NUM_20

/* host 模式下控制 VBUS 输出的引脚 */
#define BSP_USB_PIN_VBUS_EN     GPIO_NUM_2    /* TODO 待原理图确认 */

#endif /* TYPEFLOW_V1_H */
