#!/usr/bin/env python3
"""扫 app/ime/engines/*/ 生成引擎注册表。

为什么要生成而不是手写一个中心列表：贡献者加一门语言只需要新建一个目录，
不碰任何现有文件，所以不会和别人的 PR 冲突在同一行上。开源项目里那个
"所有人都要改的注册文件"是最典型的合并冲突热点。

约定：
  - app/ime/engines/<id>/ 里必须有 C 代码实现 ime_engine_get_<id>()
  - 以 _ 开头的目录默认跳过（_template 是给人复制的示例，不该编进固件）
  - --include-underscore 会把它们也带上，PC 测试工装用这个来测模板

用法：
  python tools/gen_ime_registry.py <engines_dir> <输出.c> [--include-underscore]
"""

import os
import sys


def discover(engines_dir, include_underscore=False):
    names = []
    for name in sorted(os.listdir(engines_dir)):
        path = os.path.join(engines_dir, name)
        if not os.path.isdir(path):
            continue
        if name.startswith(".") or name == "__pycache__":
            continue
        if name.startswith("_") and not include_underscore:
            continue
        # 目录里得真有 C 代码，否则是个空壳，早点报出来比链接期报错好
        if not any(f.endswith(".c") for f in os.listdir(path)):
            print(f"警告: engines/{name}/ 里没有 .c 文件，已跳过", file=sys.stderr)
            continue
        names.append(name)
    return names


def render(names):
    lines = [
        "/* 本文件由 tools/gen_ime_registry.py 自动生成 —— 不要手改。 */",
        "/* 加一门语言：新建 app/ime/engines/<id>/ 目录即可，然后 idf.py reconfigure。 */",
        "",
        '#include "ime_registry.h"',
        "",
    ]
    for n in names:
        lines.append(f"extern const ime_engine_t *ime_engine_get_{n}(void);")
    lines.append("")
    lines.append("const ime_engine_getter_t ime_registry[] = {")
    for n in names:
        lines.append(f"    ime_engine_get_{n},")
    lines.append("    NULL,   /* 结束哨兵 */")
    lines.append("};")
    lines.append("")
    return "\n".join(lines)


def main():
    args = [a for a in sys.argv[1:] if not a.startswith("--")]
    include_underscore = "--include-underscore" in sys.argv

    if len(args) != 2:
        print(__doc__)
        return 1

    engines_dir, out_path = args
    if not os.path.isdir(engines_dir):
        print(f"错误: 找不到目录 {engines_dir}", file=sys.stderr)
        return 1

    names = discover(engines_dir, include_underscore)
    if not names:
        print(f"错误: {engines_dir} 下没有发现任何引擎", file=sys.stderr)
        return 1

    text = render(names)

    # 内容没变就不写，避免每次构建都触发下游重编
    if os.path.exists(out_path):
        with open(out_path, encoding="utf-8") as f:
            if f.read() == text:
                print(f"注册表无变化 ({len(names)} 个引擎: {', '.join(names)})")
                return 0

    os.makedirs(os.path.dirname(out_path) or ".", exist_ok=True)
    with open(out_path, "w", encoding="utf-8", newline="\n") as f:
        f.write(text)
    print(f"已生成 {out_path}：{len(names)} 个引擎 ({', '.join(names)})")
    return 0


if __name__ == "__main__":
    sys.exit(main())
