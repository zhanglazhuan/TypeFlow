/*
 * conformance — 所有输入法引擎都必须通过的通用测试
 *
 * 这套测试定义了「引擎写完了」的客观标准。PR 过不了它就不合并 ——
 * 这样 reviewer 不用逐个语言去判断实现对不对，贡献者也清楚目标在哪。
 *
 * 它只测**与语言无关**的行为：状态机的自洽性、边界、UTF-8 合法性。
 * 语言特有的正确性（"nihao 应该出「你好」"）由贡献者随引擎提交自己的黄金用例。
 *
 * 只允许 C 标准库 —— 和引擎本身一样，这样能用普通 gcc 在 PC 上跑。
 * 驱动程序见 tools/ime-test/。
 */

#ifndef IME_CONFORMANCE_H
#define IME_CONFORMANCE_H

#include "engine_api.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct {
    int passed;
    int failed;
} ime_conformance_result_t;

/**
 * @brief 对一个引擎跑完整套一致性测试。
 *
 * @param e       被测引擎
 * @param verbose 非 0 时逐项打印，0 只打印失败项
 * @return 结果计数；failed == 0 表示通过
 */
ime_conformance_result_t ime_conformance_run(const ime_engine_t *e, int verbose);

#ifdef __cplusplus
}
#endif

#endif /* IME_CONFORMANCE_H */
