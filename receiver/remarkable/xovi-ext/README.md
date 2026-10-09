# XOVI 扩展 — reMarkable 文本注入

**动手前先读 [`SPIKE.md`](SPIKE.md)。** 有几个必须在真机上先确认的前提，
其中两条（Qt 符号是否可用、设备上有没有 CJK 字体）会直接决定方案可行性和一期范围。

## 它要做什么

1. 靠 XOVI 被 LD_PRELOAD 进 `xochitl` 进程
2. 在进程内起 `QTcpServer` 监听，按 `../../protocol/PROTOCOL.md` 收帧
3. 收到 `COMMIT_TEXT` 就把 UTF-8 变成 `QString`，投递给当前聚焦的 Qt 控件
4. 回 `COMMIT_ACK`（没有聚焦输入框时回 `NO_FOCUS`）

## 关键推论

大概率**不需要 hook xochitl 的任何函数**。XOVI 的 override/trampoline 能力在这里
用不上，我们要的只是它的**注入**能力 —— 一旦进到 xochitl 进程里，
文本输入走的是 Qt 的**公开 API**，直接调就行。

具体走 `QKeyEvent` 而不是 `QInputMethodEvent`：Qt 文本控件插入的是
`QKeyEvent::text()` 这个 QString，不是按 keycode 查表。所以构造一个 keycode 为
`Key_unknown` 但 `text()` 是汉字的事件，控件会照样把字插进去 ——
**键码那道墙只存在于 HID 和 evdev 层，到 Qt 事件层文本本来就是 QString。**

详细推理、要查的符号、线程安全处理、风险清单都在 SPIKE.md。

## 状态

尚未实现。网络层可以直接搬 `../../../protocol/ref/` 的协议处理逻辑
（那份已经有 42 项自测覆盖粘包/分片/重同步），换成 `QTcpServer` 即可。
