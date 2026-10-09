/*
 * candbar — 候选条：把输入法内核接到按键和屏幕之间
 *
 * 这是 v1 唯一的「应用」。它持有 ime 实例，消费 input 的语义按键，
 * 更新候选条显示，并在用户选词后交给 hidout 发出去。
 *
 * 和 DeepStoa 的 apps/ 不同：那边每个 app 是可启动/可切换的，需要 appmgr
 * 管生命周期。键盘只有一个业务，所以没有 appmgr，也没有 launcher。
 */

#ifndef CANDBAR_H
#define CANDBAR_H

#include "esp_err.h"
#include "input.h"

#ifdef __cplusplus
extern "C" {
#endif

/** @brief 创建 ime 实例并接好各方。调用前 storage/hidout 必须已初始化。
 *
 *  引擎（各语言输入法）是构建时自动发现的，见 app/ime/engine_api.h。
 *  加一门语言不需要动本文件。 */
esp_err_t candbar_init(void);

/** @brief 喂一个语义按键。由 input 的回调转发过来。 */
void candbar_handle_key(const input_event_t *ev);

#ifdef __cplusplus
}
#endif

#endif /* CANDBAR_H */
