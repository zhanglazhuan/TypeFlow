# receiver/boox/ — BOOX 接收器（二期）

一期先做 reMarkable，本目录留到二期。下面是已经定下来的方向。

## 为什么 BOOX 比 reMarkable 简单得多

| | reMarkable | BOOX |
|---|---|---|
| 提交机制 | hook Qt，**无人做过**，要先 spike | `InputMethodService.commitText()`，**Android 官方 API** |
| 传输 | 只能 USB，且 TypeFlow 要当 host（平板无蓝牙、也切不到 host） | BLE 可用，TypeFlow 当 device |
| 系统自带 CJK | **无**，可能连字体都没有 | 有 |
| 安装 | SSH + 装 XOVI，且系统更新会冲掉 | 装 APK，系统设置里启用输入法 |

Android 的 `InputMethodService` 本来就是为「第三方输入法」设计的，
我们要做的事完全在它的设计意图之内 —— 不是 hack。

## 形态：一个不显示键盘的输入法

```kotlin
class TypeFlowIME : InputMethodService() {
    // onCreateInputView 返回一个极小的（或零高度的）视图：
    // 候选词在 TypeFlow 键盘自己的长条屏上选，不占墨水屏、不触发刷新。
    // 这正是 BOOX 上做这件事的价值 —— 系统自带输入法能打中文，
    // 但屏幕输入法在墨水屏上又慢又遮挡。
    fun onTextReceived(utf8: String) {
        currentInputConnection?.commitText(utf8, 1)
    }
}
```

## 要点

- **BLE 传输**：自定义 GATT 服务跑 TypeFlow 协议。注意 ATT MTU 默认只有 23 字节，
  要协商到 ≥ 247，否则一帧就得分片（见 `../../protocol/PROTOCOL.md`）。
- **协议要移植一份 Kotlin**：Android 侧用不了 `typeflow_proto.c`。
  改协议时 `.md` / `.c` / `.py` / `.kt` **四处**都要同步。
- **前台服务**：Android 会杀后台连接。BLE 长连接需要 foreground service + 通知。
- **Onyx SDK**：`onyx-android-sdk` 可控制 e-ink 刷新模式。本接收器几乎不绘制界面，
  大概用不上，但若要做配置页会需要。

## 一期就要做的一件事

固件侧的 `TARGET_BRAND_BOOX` profile 已经在 `../../system/target/target.c` 里定好了：
`links = { BLE, USB_HID }`，需要接收器，系统自带 CJK。

注意 BOOX 的 USB 角色和 reMarkable **相反**：BOOX 是 Android，能当 OTG host，
所以 TypeFlow 在它面前是普通的 USB device（`TARGET_LINK_USB_HID`）。
而 ESP32-S3 只有一个 USB-OTG 外设，两种角色不能并存 —— 硬件上 USB-C 口
必须支持双角色（VBUS 可供可取），这条影响原理图。
