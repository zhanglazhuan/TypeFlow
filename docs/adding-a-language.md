# 加一门语言

TypeFlow 的输入法是插件。加一门语言**不需要改任何现有文件** —— 新建一个目录就行，
所以你的 PR 不会和别人的冲突。

## 最短路径

```bash
cp -r app/ime/engines/_template app/ime/engines/myim
# 把 engine.c 里的 "template" 改成 "myim"（函数名必须是 ime_engine_get_myim）

cd tools/ime-test
make && ./ime-test check myim      # 一致性测试
./ime-test type myim hello        # 试打字
```

搞定。要编进固件的话再跑一次 `idf.py reconfigure`（目录扫描只在 configure 阶段做）。

## 你只需要读一个文件

[`app/ime/engine_api.h`](../app/ime/engine_api.h) 是完整契约。核心是一张 vtable：
`accepts_char` / `feed` / `backspace` / `reset` / `preedit` / `candidates` / `commit`。

**引擎自己持有全部输入状态。** core 不替你存编码串 —— 各语言状态差别太大
（拼音是字母缓冲、谚文是初中终声槽位、Telex 是带变音标记的音节），自己管反而简单。

core 替你做的是：引擎注册与切换、候选**分页**、提交缓冲的生命周期。
所以 `candidates()` 一次返回你能给的全部候选，翻页不用管。

## 不需要词库的输入法怎么写

**本接口不假设「编码 → 查词库 → 选候选」。** 谚文是 jamo 机械合字、Telex 是
确定性变音符放置、泰文是直接映射 —— 这些输入法根本没有候选，输入即输出。

这种情况下：

- `candidates()` 直接 `return 0`，这完全合法
- `commit()` 只会收到 `index < 0`，表示"把当前 preedit 提交掉"
- `env->dict.size == 0` 时必须能正常工作

`_template` 就是这条路径的例子（希腊字母转写），只有 ~190 行，一半是注释。
需要词库的例子看 [`engines/pinyin/engine.c`](../app/ime/engines/pinyin/engine.c)。

## 三条铁律

### 1. 只能包含 C 标准库

不许 `esp_` 开头的头文件，不许 freertos 或 driver 目录下的东西。

这条不是洁癖，是为了你：守住它，你就能用普通 gcc 编译测试，**不需要 ESP-IDF、
不需要开发板、更不需要那块定制键盘 PCB**。绝大多数愿意贡献一门语言的人不会有
这些硬件 —— 如果必须有板子才能开发，这个项目就不会有语言贡献者。

平台相关的东西全部通过 `ime_env_t` 注入。这条纪律目前是实测有效的：整个
`app/ime` 的外部符号只有 `calloc free memcpy memset strcmp strlen` 这些 libc 函数。

### 2. 必须声明 `ucs_ranges`

```c
static const ime_ucs_range_t s_ranges[] = {
    { 0xAC00, 0xD7A3 },     /* 谚文音节 */
    { 0x1100, 0x11FF },     /* 谚文字母 */
};
```

**这是最容易漏的一条，漏了的症状是你的字在屏幕上全显示成方框。**
长条屏靠 flash 里的点阵字体渲染，字体是构建时按**所有启用引擎的区段并集**
裁出来的。你不声明，字体里就没有这些字形。

### 3. `id` 发布后不能改

`id` 同时是词库容器里的键、和用户配置里存的值。改了用户的设置和词库都会对不上。
用小写字母、数字、下划线（一致性测试会检查）。

## 一致性测试就是"做完了"的定义

```bash
cd tools/ime-test && make check
```

必须全绿才能提 PR。它测的是与语言无关的行为：

- 无词库无存储时 `create()` 必须成功
- `accepts_char` 返回 false 的字符，`feed` 也必须失败
- 空状态下 `backspace` 返回 false、`commit` 返回 NULL
- 输入有长度上限，灌不爆
- `reset` 后 preedit 为空、候选为 0
- **preedit 和所有候选、提交结果都必须是合法 UTF-8**
- 越界候选索引不能崩

最后两条值得单独说。非法 UTF-8 会让下游的文本发送和屏幕渲染一起出问题，
而症状离原因很远、极难排查，所以在这一层就卡掉。

语言特有的正确性（"nihao 该出「你好」"）请自己在引擎目录里加黄金用例 ——
一致性测试管不了这个，也不该管。

## 词库

如果你的语言需要词库：

- 源词表入库，**编译出的二进制不入库**（`.gitignore` 里已经忽略了 `*.dict.bin`）
- 多语言共享同一个 6 MB flash 分区，容器格式和 TOC 由 `tools/dict_build.py` 管，
  你的引擎通过 `env->dict` 只看到属于自己的那一段
- **词库授权必须在引擎目录里放一份独立的 `DICT_LICENSE`**。中文/日文词库的
  来源授权往往不清楚，别让它污染主仓库的 LICENSE

## 目前还不支持的：键盘布局

注音、俄语 ЙЦУКЕН、泰语 Kedmanee 需要不同的物理键映射。那是 `system/input`
的职责，不是引擎的。**现在所有引擎收到的都是基础 ASCII 字母**，布局覆盖是
待做项。如果你的语言必须换布局，先来开个 issue 讨论。
