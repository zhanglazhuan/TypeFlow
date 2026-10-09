# protocol/ — 线协议契约

固件与所有接收器共享的线格式。

| 文件 | 说明 |
|---|---|
| `PROTOCOL.md` | **规范，唯一真相来源**。改帧格式先改这里 |
| `typeflow_proto.h` / `.c` | C 实现，**固件与 reMarkable 接收器共用同一份源码** |

## 固件怎么用到这份源码

`system/hidout/CMakeLists.txt` 把本目录加进 `INCLUDE_DIRS` 和 `SRCS`（相对路径
`../../protocol/`），所以**没有复制粘贴**，两边编的是同一个文件。
这是刻意的：协议双边不一致是最难查的一类 bug，共用源码从根上避免。

## BOOX 侧

Android 接收器是 Kotlin，没法直接用这个 .c。`../receiver/boox/` 下会有一份
Kotlin 移植，必须和 `PROTOCOL.md` 对齐。改协议时**三处都要动**。

## 单测

协议解析是纯函数，应该在 PC 上测（尤其 `tf_decode` 的流式重同步逻辑：
垃圾字节、被切断的帧、CRC 错、长度离谱）。测试在 `ref/` 里，见 `ref/README.md`。
