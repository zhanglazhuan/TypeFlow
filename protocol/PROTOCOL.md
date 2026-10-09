# TypeFlow 线协议 v1

**这是固件与所有接收器之间线格式的唯一真相来源。** 改这里就必须同步改
`typeflow_proto.h`、固件侧 `system/hidout/`、以及每个 `receiver/*/` 实现。

## 设计取舍

- **帧格式统一，传输无关**：同一套帧跑在 TCP（reMarkable）、BLE GATT（BOOX）、
  USB CDC 上。加新品牌只需接一个传输，不动帧解析。
- **文本用 UTF-8 而不是码点数组**：接收器侧最终都要交给 OS API
  （Qt 的 `QString`、Android 的 `commitText`），两者都吃 UTF-8/UTF-16，
  发码点数组反而要在两边各写一次转换。
- **不做加密，但要做配对校验**：键盘会把用户打的每个字发出去。一期两条通道
  （USB 点对点、BLE 配对链路）都不是共享介质，所以 v1 只用共享 token 做接入校验，
  加密留到 v2（见文末）。
- **小端序**：ESP32-S3、reMarkable(armv7 LE)、BOOX(arm64 LE) 全是小端，不做字节序转换。
- **magic 按字节定义而不是按 16 位值定义**：写成 `0x5446` 再小端打包，线上字节序会
  变成 'F','T'，抓包时反着读很容易误判。所以规定线上第 0 字节是 'T'、第 1 字节是 'F'，
  对应的小端 uint16 常量是 `0x4654`。

## 帧格式

```
偏移  长度  字段
0     2     magic       0x54 0x46，即 ASCII 'T','F'（hexdump 里直接读作 TF）
2     1     version     当前 1
3     1     type        见下表
4     2     payload_len 小端，0~1024
6     N     payload
6+N   2     crc16       对 [0, 6+N) 做 CRC-16/CCITT-FALSE，小端
```

固定头 6 字节 + 载荷 + 2 字节校验。最大帧 1032 字节。

## 帧类型

| type | 名称 | 方向 | 载荷 |
|---|---|---|---|
| 0x01 | `HELLO` | 键盘→接收器 | token(16) + 固件版本字符串 |
| 0x02 | `HELLO_ACK` | 接收器→键盘 | 品牌(1) + 能力位图(4) + 接收器版本字符串 |
| 0x10 | `COMMIT_TEXT` | 键盘→接收器 | UTF-8 文本，**不带结尾 NUL** |
| 0x11 | `COMMIT_ACK` | 接收器→键盘 | 结果码(1) |
| 0x20 | `KEY` | 键盘→接收器 | modifiers(1) + HID keycode(1)，透传退格/方向键等 |
| 0x30 | `PING` | 双向 | 无 |
| 0x31 | `PONG` | 双向 | 无 |

### 能力位图（`HELLO_ACK`）

| 位 | 含义 | 键盘侧用途 |
|---|---|---|
| 0 | 支持 `COMMIT_TEXT` | 没这位就只能发 ASCII，候选条要提示用户 |
| 1 | 支持 `KEY` 透传 | |
| 2 | 接收器能报告当前输入框是否聚焦 | 没聚焦时选词应该拦住，不然字丢了 |
| 3 | 支持候选条状态回传（v2 预留） | |

## 握手

```
键盘                        接收器
 |  HELLO(token, 版本)  ->    |
 |  <- HELLO_ACK(品牌, 能力)  |
 |                            |
 |  COMMIT_TEXT("你好")  ->   |
 |  <- COMMIT_ACK(OK)         |
```

`HELLO_ACK` 超时（建议 2s）视为接收器不在，候选条应退回 ASCII 直通模式并提示用户。

## 各传输的承载方式

| 传输 | 承载 | 备注 |
|---|---|---|
| TCP over USB（reMarkable） | 裸帧流式拼接 | TypeFlow 当 USB host 连平板的 RNDIS gadget，在 usb0 上跑 TCP。TCP 不保证帧边界，必须按 `payload_len` 累积 |
| BLE GATT（BOOX） | 一帧一次 Write | 注意 ATT MTU：默认 23 字节要协商到 ≥ 247，否则得分片 |
| USB CDC-ACM | 裸帧流式拼接 | 备选：若平板的 gadget 也暴露 ACM 串口，可省掉整个 lwIP/TCP 栈 |

## v2 待办

- **加密**：目前明文。USB 是点对点物理连接，被动嗅探门槛比 Wi-Fi 高得多，
  所以优先级不高；BLE 侧可依赖链路层配对加密。若将来要加：预共享密钥 +
  ChaCha20-Poly1305，或 TLS-PSK（mbedtls 在 IDF 里现成）。
- **重放防护**：现在没有序号，重放能重复插字。加单调递增 seq。
- **输入框聚焦状态回传**：能力位 2，避免用户没聚焦输入框时选的词凭空消失。
