# protocol/ref/ — 参考实现

开发期用的对端。**固件侧不必等任何品牌的接收器写好**，就能在 PC 上跑通
传输 → 协议 → 文本 这条完整链路。

它监听普通 TCP，所以对端是 Wi-Fi、USB 上的 RNDIS、还是别的什么都无所谓 ——
协议层完全一样。真机上 reMarkable 那条通道是「TypeFlow 当 USB host 连平板的
RNDIS gadget，在 usb0 上跑 TCP」，在 PC 上调试时就是一条普通的 TCP 连接。

用 Python 而不是 C：它是测试工装不是产品代码，免编译、Windows 上直接跑；
而 reMarkable 的生产接收器是 XOVI 扩展里的 C++/Qt，本来也不会复用 C 的网络层。

## 用

```bash
python receiver.py                 # 监听 0.0.0.0:14500
python receiver.py --port 9000
python receiver.py --no-focus      # 所有提交回 NO_FOCUS，测固件对「没聚焦输入框」的处理
python receiver.py --no-commit     # 握手不声明 COMMIT_TEXT，测固件降级到 ASCII 的路径
```

后两个开关很重要：真机上这两种情况都会发生（用户没点输入框、用户装的接收器版本太老），
固件必须优雅处理而不是静默丢字。

## 测

```bash
python test_protocol.py    # 28 项：CRC 向量、编解码、流式重同步、边界
python test_e2e.py         # 14 项：起接收器 + 模拟键盘跑完整握手与提交
```

两个都不需要硬件。`test_protocol.py` 里 `crc16(b"123456789") == 0x29B1` 是
CRC-16/CCITT-FALSE 的标准校验值 —— 它保证 Python 实现和 C 版 `tf_crc16`
用的是同一套参数（poly 0x1021 / init 0xFFFF / 不反转 / 不异或输出）。

`test_e2e.py` 覆盖了 TCP 上真实会遇到而最容易写错的三种情况：
**粘包**（两帧一次到）、**分片**（一帧拆两次到）、**垃圾字节后重同步**。

## 文件

| 文件 | 说明 |
|---|---|
| `protocol.py` | 协议 Python 实现，等价于 `../typeflow_proto.c` |
| `receiver.py` | TCP 服务端，收到文本打到终端，不做注入 |
| `test_protocol.py` / `test_e2e.py` | 自测 |

改协议时 `../PROTOCOL.md`、`../typeflow_proto.c`、`protocol.py`
三处都要同步，然后重跑这两个测试。
