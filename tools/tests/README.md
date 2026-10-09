# tools/tests/

`app/ime` 的 PC 侧回归测试。

这是把 ime 内核做成零平台依赖的**回报所在**：同一份 `app/ime/*.c` 直接用主机
gcc/clang 编译，喂进大批拼音串比对候选输出，一轮几秒钟，而不是烧板子肉眼看屏。

## 预期用法

```bash
# 直接编译 ime 内核（注意：不需要 ESP-IDF）
gcc -I../../app/ime -o test_pinyin test_pinyin.c ../../app/ime/*.c
./test_pinyin
```

词库来源用一个读文件的 `ime_dict_source_t` 实现替代板上的 `esp_partition_mmap`，
用户词频用读写临时文件的 `ime_userdata_store_t` 替代 littlefs。
两个结构体的定义见 `app/ime/ime.h`。

## 要覆盖的用例

- 音节切分歧义：`xian` vs `xi'an`、`nvhai` 的 v=ü、`n` 结尾的回溯
- 未打完的半个音节（`zhonguo` 打到 `zhong` 时应已有候选）
- 边界：空串、超长串（`IME_MAX_INPUT_LEN`）、非法字符
- 部分转换：`woshi` 选「我」后应剩 `shi` 继续出候选（见 `ime_commit` 的 TODO）
