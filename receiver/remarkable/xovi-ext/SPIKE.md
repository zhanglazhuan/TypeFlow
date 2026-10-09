# SPIKE：怎么把一段 UTF-8 塞进 xochitl 的输入框

**动手写扩展前先做完这个 spike。** 下面是推理出的方案和必须在真机上验证的每一步 ——
没有设备没法证实，所以每条都标了验证方法。

## 核心洞察：大概率**不需要 hook 任何 xochitl 的函数**

XOVI 的机制是 LD_PRELOAD 把我们的 .so 注入 xochitl 进程。一旦进去了，我们就和
xochitl 共享同一个 Qt 运行时 —— 而 Qt 的文本输入入口是**公开 API**。
所以我们要的可能只是「进到进程里」+「调 Qt 公开函数」，而不是覆盖 xochitl 自己的函数。

XOVI 的 override/trampoline 能力在这里可能根本用不上，它的价值只是**注入**。
这比 `rm-xovi-extensions` 里那些真的要改 xochitl 行为的扩展（如 rmfakecloud 换 URL）
简单一个量级。

### 为什么 QKeyEvent 比 QInputMethodEvent 更可能成功

Qt 的文本控件（`QQuickTextInput`、`QLineEdit`）在处理按键时，插入的是
`QKeyEvent::text()` 这个 **QString**，而不是根据 keycode 去查表。也就是说：

```cpp
QKeyEvent ev(QEvent::KeyPress, Qt::Key_unknown, Qt::NoModifier, QString("你好"));
QCoreApplication::postEvent(target, &ev);   // postEvent 是线程安全的
```

一个 keycode 为 `Key_unknown` 但 `text()` 是汉字的事件，控件会照样把那串字插进去。
**这就绕过了整个键码/Unicode 难题** —— 键码那道墙只存在于 HID 和 evdev 层，
到了 Qt 事件层，文本本来就是 QString。

`QInputMethodEvent`（带 `commitString`）是更「正规」的 IME 路径，作为备选：
它对 preedit（候选未确定时的下划线预览）支持更好，将来若想把编码串也显示在
设备屏上会用到。但它对控件的 `inputMethodQuery` 支持有要求，先试简单的那条。

## 验证步骤

### 0. 确认平板的 USB gadget 暴露了什么（最先做，一条命令）

在**连着平板的 PC** 上：

```bash
lsusb -v -d <reMarkable 的 VID:PID> | grep -A3 -i "interface descriptor\|bInterfaceClass"
```

或在平板上直接看 gadget 配置：

```bash
ssh root@10.11.99.1
ls /sys/kernel/config/usb_gadget/*/functions/
```

要回答的是：**除了 RNDIS 和存储，有没有 CDC-ACM 串口功能？**

- **有 ACM** → 固件侧用 `usb_host_cdc_acm` 跑串口，省掉整个 lwIP/TCP 栈，简单得多
- **只有 RNDIS** → 用 `espressif/iot_usbh_rndis` + lwIP + TCP

这一条决定固件侧 `hidout_usbhost.c` 的实现路线和工作量，**比下面所有步骤都该更早做**。

### 1. 确认 Qt 版本与符号可用性

```bash
ssh root@10.11.99.1
ls -la /usr/lib/libQt5*.so.5
strings /usr/lib/libQt5Core.so.5 | grep -m1 'Qt 5\.'
nm -D --defined-only /usr/lib/libQt5Gui.so.5 | grep -i focusObject
nm -D --defined-only /usr/lib/libQt5Core.so.5 | grep -i postEvent
```

要拿到的 mangled 名大致是（**必须用 `nm` 的实际输出，别照抄**）：

| 用途 | 符号（示意） |
|---|---|
| 取当前聚焦对象 | `_ZN15QGuiApplication11focusObjectEv` |
| 投递事件 | `_ZN16QCoreApplication10postEventEP7QObjectP6QEventi` |
| QKeyEvent 构造 | `_ZN9QKeyEventC1EN6QEvent4TypeEi6QFlagsIN2Qt16KeyboardModifierEERK7QStringbt` |
| QString 从 UTF-8 构造 | `_ZN7QString12fromUtf8_...` |

**如果 Qt 是静态链接进 xochitl 且符号被 strip 了，这条路就断了** —— 那才需要回到
XOVI 的 hook 能力，去找 xochitl 自己的输入处理函数。先确认这一点，它决定整个工作量。

### 2. 确认聚焦对象存在

`focusObject()` 在没有输入框聚焦时返回 `nullptr`。这对应协议里的
`TF_RESULT_NO_FOCUS`（见 `../../protocol/PROTOCOL.md`）。

最小验证：写个只做一件事的扩展 —— 每秒把 `focusObject()` 的
`metaObject()->className()` 打到日志，然后在设备上点进「重命名文档」的输入框、
点进 Wi-Fi 密码框、点进笔记本的打字模式，看分别报出什么类名。

**这一步同时回答了一个产品问题**：xochitl 里到底有哪些地方能打字。
如果只有重命名和 Wi-Fi 密码能聚焦、笔记本正文的打字模式走的是完全不同的自绘控件，
那 TypeFlow 在 reMarkable 上的可用范围就要重新评估 —— 这是整个一期最该先确认的事。

### 3. 确认线程安全

`QCoreApplication::postEvent` 文档上是线程安全的，所以 `QTcpServer` 在自己线程里
收到帧后直接 postEvent 应该没问题。但 `focusObject()` **不是**线程安全的 ——
它读的是 GUI 线程的状态。

对策：用 `QMetaObject::invokeMethod(..., Qt::QueuedConnection)` 把整个
「查 focusObject + postEvent」跳到 GUI 线程执行。先按这个写，别赌。

### 4. 端到端

扩展里起 `QTcpServer` 监听（端口见 PROTOCOL.md），用 `../../../protocol/ref/` 的
协议解析代码，收到 `COMMIT_TEXT` 就走上面的路径，回 `COMMIT_ACK`。
先用 `protocol/ref` 在 PC 上对着固件跑通，再搬进扩展。

## 风险清单

| 风险 | 影响 | 先验方法 |
|---|---|---|
| Qt 符号被 strip / 静态链接 | 方案要换成 hook xochitl，工作量翻几倍 | 步骤 1，最先做 |
| 笔记本正文打字用自绘控件、不是 Qt 文本控件 | TypeFlow 只能在重命名/密码框这类地方用，产品价值大打折扣 | 步骤 2 |
| xochitl 升级后符号变化 | 每次固件更新要重新适配 | 把符号查找做成运行时 `dlsym` + 失败时明确报错，而不是编译期硬绑 |
| 扩展崩溃带崩 xochitl | 用户丢笔记 | 收包路径全部做边界检查；postEvent 前判空；别在扩展里做任何阻塞操作 |
| 字体缺失 | 汉字显示成方框 | 设备上 `fc-list | grep -i cjk`。若没有 CJK 字体，还得解决字体安装 —— **这条也要早验** |

## 最后一条风险值得单独说

reMarkable 系统本身不支持 CJK（Type Folio 只有英法德北欧布局），
所以**设备上很可能根本没有中日文字体**。就算我们成功把「你好」postEvent 进去了，
渲染出来也可能是两个方框。

这会把一期范围从「做个接收器」扩大到「还要装字体」。
`fc-list` 那一条命令就能确认，**建议和步骤 1 一起做，在投入写扩展之前**。
