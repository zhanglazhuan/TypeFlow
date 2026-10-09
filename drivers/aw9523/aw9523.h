/*
 * AW9523B — I2C GPIO Expander Driver
 *
 * 移植自 D:\Codes\DeepStoa\drivers\aw9523，但做了三处结构性改动：
 *   1. 单例 → handle 式。键盘矩阵可能需要两片（8x8=64 键不够时），
 *      AW9523 有 4 个可选地址 0x58~0x5B。
 *   2. 不再自己建 I2C 总线，改为接收调用方传入的 bus handle。
 *      总线是共享资源，归 system/controller 统一创建。
 *   3. 去掉板级耦合。原版 init 里硬编码了墨水屏的 CS/BUSY 引脚方向，
 *      这里 init 只做"能通信 + 安全默认值"，方向配置交给调用方。
 */

#ifndef AW9523_H
#define AW9523_H

#include <stdint.h>
#include <stdbool.h>
#include "esp_err.h"
#include "driver/i2c_master.h"
#include "hal/gpio_types.h"

#ifdef __cplusplus
extern "C" {
#endif

/* ========================================================================
 * 引脚编码
 * ========================================================================
 * 16 个 IO 编码成一个字节：高位选端口(P0/P1)，低 3 位选位。
 * 这组宏原先放在 DeepStoa 的板级头文件里 —— 属于分层错误，编码方式是驱动
 * 的内部约定，不该让板级定义它。
 * ======================================================================== */
#define AW9523_PIN(port, bit)   ((uint8_t)(((port) << 3) | ((bit) & 0x07)))
#define AW9523_GET_PORT(pin)    (((pin) >> 3) & 0x01)
#define AW9523_GET_BIT(pin)     ((pin) & 0x07)

#define AW9523_PORT_P0          0
#define AW9523_PORT_P1          1

#define AW9523_CHIP_ID          0x23

typedef struct aw9523_dev_t *aw9523_handle_t;

typedef struct {
    uint8_t     i2c_addr;       /* 0x58 ~ 0x5B，由 AD0/AD1 两个地址脚决定 */
    gpio_num_t  rst_pin;        /* 低有效硬复位脚；没有就填 GPIO_NUM_NC */
    uint32_t    scl_speed_hz;   /* 0 表示用默认 400000 */
} aw9523_config_t;

/**
 * @brief 在已有 I2C 总线上创建一个 AW9523 实例。
 *
 * 会做硬复位(若提供 rst_pin)、校验 chip ID、然后设置安全默认值：
 * 全部引脚为输入、全部中断关闭、P0 推挽、P0/P1 均为 GPIO 模式。
 *
 * @param bus  调用方创建的 I2C master 总线
 * @param cfg  芯片配置
 * @param out  输出 handle
 * @return ESP_ERR_NOT_FOUND 表示读到的 chip ID 不是 0x23
 */
esp_err_t aw9523_new(i2c_master_bus_handle_t bus, const aw9523_config_t *cfg,
                     aw9523_handle_t *out);

/** @brief 释放实例并从总线摘除设备。 */
esp_err_t aw9523_del(aw9523_handle_t h);

/** @brief 读 chip ID 寄存器(0x10)，正常应为 0x23。 */
esp_err_t aw9523_get_chip_id(aw9523_handle_t h, uint8_t *id);

/** @brief 软复位（寄存器 0x7F），所有寄存器回到上电默认值。 */
esp_err_t aw9523_soft_reset(aw9523_handle_t h);

/**
 * @brief 设置整个端口的方向。
 * @param dir_mask 按位：1 = 输入，0 = 输出（与数据手册一致）
 */
esp_err_t aw9523_set_port_dir(aw9523_handle_t h, uint8_t port, uint8_t dir_mask);

/** @brief 写整个端口的输出寄存器。 */
esp_err_t aw9523_write_port(aw9523_handle_t h, uint8_t port, uint8_t value);

/** @brief 读整个端口的输入寄存器。读操作会清除该端口的中断标志。 */
esp_err_t aw9523_read_port(aw9523_handle_t h, uint8_t port, uint8_t *value);

/** @brief 设置单个引脚输出电平。用 AW9523_PIN() 构造 pin 参数。 */
esp_err_t aw9523_set_pin(aw9523_handle_t h, uint8_t pin, uint8_t level);

/** @brief 读单个引脚输入电平。 */
esp_err_t aw9523_get_pin(aw9523_handle_t h, uint8_t pin, uint8_t *level);

/**
 * @brief 设置 P0 的输出级形态。
 *
 * 只有 P0 可选推挽/开漏；P1 恒为推挽，这是芯片限制，不是驱动限制。
 * 键盘矩阵驱动行时用推挽。
 */
esp_err_t aw9523_set_p0_push_pull(aw9523_handle_t h, bool push_pull);

/**
 * @brief 配置端口的引脚变化中断。
 *
 * ⚠ 极性反直觉：寄存器里 0 = 使能中断，1 = 屏蔽。本函数的参数已经正过来了，
 *   传 enable_mask 里置 1 的位表示"这个脚要中断"，内部会取反再写寄存器。
 *
 * 键盘矩阵靠这个避免空闲轮询：给所有列脚开中断，INT 拉低才去扫描。
 */
esp_err_t aw9523_set_port_int_enable(aw9523_handle_t h, uint8_t port,
                                     uint8_t enable_mask);

#ifdef __cplusplus
}
#endif

#endif /* AW9523_H */
