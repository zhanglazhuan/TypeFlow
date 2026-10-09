/*
 * i2c_control — 共享 I2C 总线的唯一持有者
 *
 * 为什么单独抽一层：I2C 总线是共享资源，AW9523(可能两片)、以后可能的
 * 触摸/传感器都挂在同一条线上。谁先 init 谁建总线的做法会在加第二个器件时
 * 撞 ESP_ERR_INVALID_STATE，所以总线在这里集中创建，驱动只接收 handle。
 *
 * 这是从 DeepStoa 移植时刻意做的改动 —— 那边 aw9523_init 内部自己建总线。
 */

#ifndef I2C_CONTROL_H
#define I2C_CONTROL_H

#include "esp_err.h"
#include "driver/i2c_master.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 按 boards/ 里的引脚定义创建共享 I2C 总线。重复调用无副作用。 */
esp_err_t i2c_control_init(void);

/** @brief 取共享总线 handle。未 init 时返回 NULL。 */
i2c_master_bus_handle_t i2c_control_get_bus(void);

/** @brief 扫描总线并把找到的地址打到日志。仅调试用，会遍历 1~126。 */
void i2c_control_scan(void);

#ifdef __cplusplus
}
#endif

#endif /* I2C_CONTROL_H */
