# receiver/remarkable/ — reMarkable 接收器（一期）

## 设备差异

| | reMarkable 1 / 2 | Paper Pro / Pro Move / Pure |
|---|---|---|
| 蓝牙 | **无硬件** | 有硬件，驱动默认不加载 (`modprobe btnxpuart`)，电池下会被省电断开 |
| 平板能否当 USB host | **不可用**（2025.1 起 sysfs role 开关被禁） | 同左 |
| 平板作为 USB device | **RNDIS 网卡** (10.11.99.1) | 同左 |
| 架构 | armv7 (32 位) | aarch64 |

**两代都要单独出构件**：armv7 和 aarch64 不通用。

## 传输：USB，但角色是反的

常规思路「键盘当 USB device、平板当 host」在 reMarkable 上走不通 —— 它切不到
host 模式。但它作为 USB **device** 的能力一直在：插到 PC 上就是个 RNDIS 网卡。

所以反过来：**TypeFlow 当 USB host 连平板**，在 usb0 上跑 TCP 承载 TypeFlow 协议。
固件侧对应 `../../system/hidout/hidout_usbhost.c`，用 Espressif 官方维护的
`espressif/iot_usbh_rndis` 组件（支持 ESP32-S3）。

**不走 Wi-Fi。** 键盘联 Wi-Fi 功耗不可接受，而且用户预期键盘就该是一根线或者蓝牙。

### 两个必须先确认的问题

**VBUS 供电方向。** USB host 按规范要给 device 供电，而 reMarkable 作为 device
需要看到 VBUS 才会枚举。也就是说 TypeFlow 的电池要去点亮平板的 USB 口。
对便携键盘是反过来的负担 —— 硬件上要么加 VBUS 开关只供最小电流，
要么用 Y 线从外部取 5V。**这条影响原理图，硬件定稿前必须定。**

**有没有更简单的 CDC-ACM。** Linux 多功能 gadget 通常是 RNDIS + ACM + 存储。
如果 reMarkable 也暴露了 ACM 串口，用 `usb_host_cdc_acm` 跑串口就行，
能省掉整个 lwIP/TCP 栈。`lsusb -v` 一条命令就能确认，**建议最先做**。

## 提交机制：三条路的取舍

| 方案 | 能发 CJK | 设备端要装什么 | 结论 |
|---|---|---|---|
| **XOVI 扩展 hook Qt** | ✅ | XOVI + 本扩展 | **目标方案**，但需先做 spike 找 hook 点 |
| Type Folio 协议模拟 | ❌ 仅键码 | 什么都不用 | ASCII 保底，固件侧 `hidout_typefolio.c` |
| uinput 虚拟键盘 | ❌ 仅键码 | 自编译的 uinput.ko | **不做** |

### 为什么放弃 uinput

ghostwriter 走的是这条路（注入笔画），但对 TypeFlow 没有优势：

1. **uinput 内核模块设备上没有** —— ghostwriter 得自己从 reMarkable 公开的内核源码
   编译一个 .ko 打包进去。多一个随固件版本失效的依赖。
2. **键码到不了 CJK**。注入的还是 evdev 键码，xochitl 的 Qt 键盘映射里没有汉字，
   和 Type Folio 模拟一样卡在同一道墙上。

既然 CJK 都得靠 hook Qt，而 ASCII 有零安装的 Type Folio 模拟，uinput 两头都不占优。

### 为什么 rmkit 不适用

rmkit 是做**独立应用**的框架（接管 framebuffer + 读 `/dev/input`）。TypeFlow 要的是
往**别人的应用**（xochitl）里塞文字，不是自己画一个界面。rmkit 的价值在别处：
它的 `/dev/input` 归一化代码和 e-ink 刷新经验值得参考，将来若要做设备端配置界面可以用。

## 目录

```
xovi-ext/     XOVI 扩展：内置 QTcpServer 收帧 + hook Qt 提交文本
  SPIKE.md    ★ 动手前必读：找 Qt hook 点的调研计划
toolchain/    armv7 / aarch64 交叉编译
```

网络层不单独做守护进程：扩展本来就跑在 xochitl 进程里、Qt 已经链好，
直接用 `QTcpServer` 比多一跳 IPC 简单，也少一个要随设备启动的服务。
代价是扩展崩了会带崩 xochitl —— 靠 systemd 重启兜底，且收包路径要写得保守。

## 开发顺序

固件侧不必等这个扩展。先用 `../../protocol/ref/` 那个参考接收器在 PC 上跑通
整条链路（传输 → 协议 → 文本），再把它的网络层搬进扩展。
