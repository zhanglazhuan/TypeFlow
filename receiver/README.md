# receiver/ — 平板侧接收器

不参与固件构建。每个子目录是一个独立项目，用各自的工具链。

## 为什么需要接收器

标准 HID 键盘发的是**键码**不是**字符**。要把「你好」送进主机的输入框，
在 HID 协议层面没有通用做法（详见 `../system/hidout/hidout.h` 文件头）。
一期两个目标品牌都得靠设备端接收器，但机制完全不同 —— 因为系统不同。

## 品牌矩阵

| | reMarkable 1/2 | reMarkable Paper Pro | BOOX |
|---|---|---|---|
| 系统 | 定制 Linux + Qt (`xochitl`) | 同左 | Android (Onyx) |
| 蓝牙 | **无硬件** | 有硬件，驱动默认不加载 | 有 |
| 平板能否当 USB host | **不可用**（2025.1 起 sysfs role 开关被禁） | 同左 | 可用（Android OTG） |
| 平板作为 USB device | **RNDIS 网卡** (10.11.99.1) | 同左 | — |
| **选定传输** | **USB：TypeFlow 当 host** | 同左（BLE 备选） | **BLE** |
| **选定提交机制** | **XOVI 扩展 → Qt** | 同左 | **InputMethodService.commitText()** |
| 系统自带 CJK 输入 | **无**（Type Folio 只有英法德北欧布局） | 无 | 有（Android 自带） |
| 接收器语言 | C (.so + 守护进程)，交叉编译 armv7 | 同左 | Kotlin APK |
| 用户安装门槛 | 高：SSH + 装 XOVI | 高 | 低：装 APK 选输入法 |
| 实现顺序 | **一期** | 一期（同代码） | 二期 |

## 为什么 BOOX 也值得做（它本来就有中文输入法）

因为墨水屏上的屏幕输入法又慢又占地方。TypeFlow 把候选词放到自己的长条彩屏上选，
BOOX 端的接收器只负责 `commitText()`、不显示任何界面 —— 省掉屏幕刷新和遮挡。

## 目录

```
protocol/     ★ 固件与所有接收器共享的线格式，改动要同步双边
remarkable/   一期
boox/         二期
```

## 调研来源

- [reMarkable 蓝牙支持（按机型）](https://remarkable.guide/devel/device/bluetooth.html)
- [XOVI 扩展框架](https://github.com/asivery/xovi) / [rm-xovi-extensions 现有扩展](https://github.com/asivery/rm-xovi-extensions)
- [ghostwriter（evdev/uinput 注入的先例）](https://github.com/awwaiid/ghostwriter)
- [rmkit（framebuffer + /dev/input 应用框架）](https://github.com/rmkit-dev/rmkit)
- [remarkable-keyboard-adapter（Type Folio 协议模拟）](https://github.com/jonasoberschweiber/remarkable-keyboard-adapter)
- [reMarkable 官方支持语言](https://support.remarkable.com/s/article/Supported-languages)
