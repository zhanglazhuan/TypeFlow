/*
 * ime_registry.h — 引擎注册表
 *
 * 实现是构建时生成的：tools/gen_ime_registry.py 扫 app/ime/engines 下的每个
 * 子目录，按约定拼出注册表。所以这里只有声明，贡献者不需要碰这个文件。
 */
#ifndef IME_REGISTRY_H
#define IME_REGISTRY_H

#include "engine_api.h"

/** @brief 生成的注册表：以 NULL 结尾的 getter 数组。 */
extern const ime_engine_getter_t ime_registry[];

#endif /* IME_REGISTRY_H */
