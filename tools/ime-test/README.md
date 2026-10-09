# tools/ime-test — PC 上跑输入法引擎

**不需要 ESP-IDF，不需要开发板，不需要那块定制键盘 PCB。** 只要一个 C 编译器。

```bash
make
./ime-test                        # 列出所有引擎及其字形区段、资源声明
./ime-test check                  # 对所有引擎跑一致性测试
./ime-test check pinyin           # 只测一个
./ime-test type template thalassa # 喂按键，看 preedit / 候选 / 提交结果
```

写引擎时用得最多的是最后那个：

```
$ ./ime-test type template thalassa
引擎 template，输入 "thalassa"
  't' -> preedit="τ"
  'h' -> preedit="θ"
  'a' -> preedit="θα"
  ...
提交 -> "θαλασσα" (14 字节)
```

注意 `t` 先变成 τ 然后被 `h` 改写成 θ —— 这就是模板里演示的"下一个键可能改变
上一个键的结果"那种状态。

## 提 PR 前

```bash
make check
```

必须全绿。一致性测试测的是与语言无关的行为：状态机自洽性、边界、**UTF-8 合法性**。
语言特有的正确性请自己加黄金用例。

## 它为什么能工作

`app/ime/` 整个组件（包括所有引擎）只依赖 C 标准库。所以同一份源码既能被
ESP-IDF 交叉编译进固件，也能被主机 gcc 编成这个测试程序。

一旦有人往引擎里 `#include "esp_log.h"`，这里立刻编不过 —— 这是刻意的早期警报，
因为破坏这条纪律等于断掉所有没有硬件的贡献者的路。

## Windows

MSYS2/MinGW 或 WSL 都行。`make` 和 `cc` 要在 PATH 里。
