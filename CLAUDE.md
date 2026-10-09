# CLAUDE.md

本文件为 Claude Code 在本仓库工作时的指引。

## 项目目标

基于 ESP32-S3 的智能键盘，用一块长条屏显示输入法候选词（需求见 `PROGRAM.md`）：

- **v1**：为墨水屏设备提供中文/日文输入
- **v2**：AI 补全与下一个词预测

## 当前状态

工程骨架已搭好并能编过（`idf.py build` 通过，固件 ~317 KB）。
线协议已实现并有 42 项自测全通过（`protocol/ref/`，含中日文、粘包、分片、重同步）。
**但固件还没有在真实硬件上跑过**，
所以标了 `TODO` 的地方基本都是等硬件确认的占位值，其中最关键的三处：

1. `boards/esp32s3/typeflow_v1.h` 的引脚分配 —— 未与原理图核对
2. `system/controller/display_control.c` 的 `LCD_X_GAP` / `LCD_Y_GAP` —— 按居中推算，必须点屏实测
3. `system/input/input.c` 的键位表 —— 全是占位，要靠 `debug/t_kbmatrix` 逐键测出来

## 硬件

- MCU：ESP32-S3 **N16R8** → 16 MB Flash + 8 MB PSRAM（Octal 八线）
- 显示：76×284 SPI 彩屏，**ST7789** 控制器
- 键盘矩阵：经 **AW9523B** I2C IO 扩展（一片 16 IO = 8×8；加第二片到 0x59 可做 8×16）

⚠ **Octal PSRAM 占用 GPIO33~37**（QUAD 版本不占），加上 Flash 占 GPIO26~32，
可用 GPIO 只剩约 26 个。这就是键盘矩阵必须走 IO 扩展的原因。

**没有 SD 卡，也不打算加。** 所有持久化都在片内 16 MB flash 上（见分区表）：
词库 6 MB、用户词频 1 MB 完全够用，而且 `esp_partition_mmap` 出来当数组查比走
文件系统快得多。项目里不应出现任何 sdmmc/sdspi 依赖 —— 参考项目 DeepStoa 有
`sd_control` 是因为它要放音频文件，TypeFlow 不需要。

## 开发环境与构建

ESP-IDF **v5.5.3**，装在 `C:\Espressif`，IDF 路径 `C:\Espressif\frameworks\esp-idf-v5.5.3`。
`idf.py` **不在全局 PATH 中**。

**关键：工具调用之间 shell 状态不保留，必须在同一条命令内先激活环境再执行 idf.py：**

```powershell
. C:\Espressif\frameworks\esp-idf-v5.5.3\export.ps1 | Out-Null; idf.py build
. C:\Espressif\frameworks\esp-idf-v5.5.3\export.ps1 | Out-Null; idf.py -p COM<N> flash monitor
. C:\Espressif\frameworks\esp-idf-v5.5.3\export.ps1 | Out-Null; idf.py size
. C:\Espressif\frameworks\esp-idf-v5.5.3\export.ps1 | Out-Null; idf.py fullclean
```

- `export.ps1` 输出很长，用 `| Out-Null` 吞掉（点号引入的环境变量仍然生效）。
- `idf.py menuconfig` 和单独的 `idf.py monitor` 是交互式的，**不要在非交互调用里跑**。
  要改配置就直接编辑 `sdkconfig.defaults`，然后 `rm sdkconfig` 让它重新生成。
- 构建输出极长，过滤后再看：
  `idf.py build 2>&1 | Select-String -Pattern "error:|Project build complete|FAILED:"`

### 已知环境坑：fatfs 必须显式配两项

本项目不用 FAT，但 IDF 会把 `fatfs` 组件一起编译（`project_description.json` 里
**没有任何组件依赖它**，IDF 仍然会编）。而 IDF 5.5.3 的 fatfs 用默认 Kconfig 编不过：

- `components/fatfs/src/ffconf.h` 里 `FF_FS_EXFAT` 硬编码为 `1`，exFAT 要求必须开 LFN，
  但 Kconfig 默认是 `FATFS_LFN_NONE` → `ff.c:503: #error LFN must be enabled when enable exFAT`
- `#define FF_USE_LABEL CONFIG_FATFS_USE_LABEL` 把 Kconfig 符号当**值**用，
  而该项 `default n` 时符号根本不生成，展开成空 → C 语法错

所以 `sdkconfig.defaults` 里有 `CONFIG_FATFS_LFN_HEAP=y` 和 `CONFIG_FATFS_USE_LABEL=y`，
**别删**。DeepStoa 同版本同样这么绕。`debug/` 下每个子工程也要各自带这两项。

## 目录结构

目录划分参考 `D:\Codes\DeepStoa`：组件按角色平铺在顶层，**不用** `components/`，
每个组件是一个带 `CMakeLists.txt` 的目录，头文件和源文件平铺（没有 `include/` 子目录）。
**新增组件必须同时在根 `CMakeLists.txt` 的 `EXTRA_COMPONENT_DIRS` 里加一行**，
漏了的报错是 `Failed to resolve component`。

```
main/TypeFlow.c          app_main()，只做初始化编排（顺序有依赖，文件头有说明）
boards/esp32s3/          引脚真相来源。任何 .c 里出现裸 GPIO 号都算 bug
drivers/aw9523/          AW9523B IO 扩展，handle 式，可多片
drivers/kbmatrix/        矩阵扫描，只报 (row,col)，不认识键值
system/controller/       硬件门面：i2c_control(共享总线) / display_control(ST7789)
system/input/            (row,col) → 语义按键 + 修饰键状态 + 键位表
system/target/           ★ 品牌归类：每个品牌能用哪些通道、怎么提交文本
system/hidout/           把文字发给主机，四个后端 (USB-HID/USB-host/BLE/TypeFolio)
system/storage/          片内 flash 持久化：nvs + dict分区mmap + userdict。无 SD 卡
system/logs events monitor ota uilv battery sleep settings    平台服务
app/ime/                 输入法内核 ★ 零平台依赖 + 语言插件化，见下
  engine_api.h           插件契约（加语言的人唯一必读）
  core/                  与语言无关：注册表、引擎切换、候选分页、提交
  engines/<id>/          每门语言一个目录，构建时自动发现
  conformance/           所有引擎都要过的通用测试
app/candbar/             候选条：v1 唯一的业务，串起 input→ime→hidout
app/ai/                  v2，尚未加入构建
debug/t_*/               各自独立的 ESP-IDF 小工程，见 debug/README.md
tools/                   主机侧脚本
  gen_ime_registry.py    扫 engines/ 生成注册表（贡献者不用碰）
  ime-test/              PC 上跑引擎，普通 gcc，无需 ESP-IDF 和硬件

protocol/                ★ 线协议契约。不属于任何一侧 —— typeflow_proto.c 被
                           编译进固件，同时也是接收器的实现依据
  ref/                   参考实现 + 一致性测试（可执行的规范，42 项自测）
receiver/                ★ 平板侧接收器，不参与固件构建
  remarkable/xovi-ext/   一期：XOVI 扩展（含 SPIKE.md，动手前必读）
  boox/                  二期：Android 输入法
```

与 DeepStoa 的**刻意差异**：

- 用 `app/`（单数）而非 `apps/`，且**没有** `appmgr/` 和 `launcher/`。
  键盘只有一个业务，不需要多应用生命周期状态机。
- **没有** `display_refresh_policy` 那套墨水屏局刷/防残影逻辑。76×284 一帧 RGB565
  只有 42 KB，双缓冲 84 KB 内部 SRAM 就放得下，候选条直接全量重绘。
  别把墨水屏的复杂度搬过来。
- `aw9523` 改成了 handle 式、总线由外部传入、去掉板级耦合（原版是单例 + 自建总线 +
  硬编码墨水屏引脚方向）。

## 四条架构纪律

### 1. `app/ime/` 只允许包含 C 标准库

不许出现 `esp_` 开头的头文件，也不许包含 freertos 或 driver 目录下的头文件。
它的 `CMakeLists.txt` 里 `REQUIRES` 是空的 —— 这不是漏写。

这条目前是**实测有效**的：把 core + 所有引擎 + 一致性套件编成目标文件后，
整个组件引用而未定义的符号只有 `calloc free memcmp memcpy memset strcmp strlen
strncmp`，全是 libc。验证方法见 `tools/ime-test/`（它能用主机 gcc 编过，
就说明纪律还在；一旦有人加了 `esp_log.h`，那边立刻编不过）。

输入法是全项目唯一算法密集、要反复回归的部分。保持零平台依赖，就能用主机 gcc 直接编译、
喂上万条拼音串比对候选（见 `tools/tests/`），一轮几秒；一旦引入平台依赖，验证就退化成
"烧板子 + 肉眼看屏"，几分钟一轮且没法批量回归。

平台相关的两件事通过回调注入，定义在 `app/ime/ime.h`：
- 词库字节从哪来 → `ime_dict_source_t`（板上是 `esp_partition_mmap`，PC 上是读文件）
- 用户词频存哪 → `ime_userdata_store_t`（板上是 littlefs，PC 上是临时文件）

注意 `ime_dict_source_t.map` 的形状是「给 offset 要指针」而不是「整块基地址」：
这样万一 6 MB 一次性 mmap 在 S3 上失败，改成分段映射只需动 `system/storage/storage.c`
一个函数，ime 内部一行都不用改。

`system/storage` 只提供**裸字节**接口（`storage_dict_map()` 返回 `const uint8_t*`），
不返回 `ime_dict_source_t` —— 包装成 ime 回调结构的代码在 `app/candbar/candbar.c`。
这样依赖方向只有 app → system 一个朝向；让系统层去 `#include "ime.h"` 会更省事，
但那是分层倒置。ime 的平台无关性靠的是**回调注入**，不取决于谁定义结构体。

### 2. 语言是插件，加语言不改任何现有文件

`app/ime/engines/<id>/` 一个目录一门语言，构建时由 `tools/gen_ime_registry.py`
扫目录生成注册表。**没有中心注册文件** —— 那是开源项目最典型的合并冲突热点。

约定：目录里实现 `ime_engine_get_<目录名>()` 返回 `const ime_engine_t *`。
以 `_` 开头的目录不编进固件（`_template` 是给人复制的示例）。
⚠ 新增目录后要 `idf.py reconfigure`，CMake 的目录扫描只在 configure 阶段求值。

接口设计上有一条容易踩的前提：**不假设「编码 → 查词库 → 选候选」**。谚文合字、
越南语 Telex、泰文都是确定性转换，根本没有候选。所以 `candidates()` 返回 0 是
一等公民，`commit(index < 0)` 表示直接提交 preedit，`env->dict.size == 0` 必须能工作。
把词库和候选写进必选路径就排除了世界上相当一部分输入法。

另外：**英文直通不是一种引擎**，是 `app/candbar` 的一个布尔状态。
造一个 "ascii" 假引擎只会让 core 到处特判。

贡献者文档在 `docs/adding-a-language.md`，契约在 `app/ime/engine_api.h`。

### 3. 上层不直接碰 `drivers/`，一律经 `system/controller/`

I2C 总线是共享资源（AW9523 可能两片，以后可能加传感器），所以总线在
`i2c_control.c` 集中创建，驱动只接收 handle。谁先 init 谁建总线的做法会在加第二个
器件时撞 `ESP_ERR_INVALID_STATE`。

### 4. 分区表已定型，改它等于清掉用户词库

`partitions.csv` 的布局（已验证生成正确）：

```
nvs 24K / otadata 8K / phy_init 4K
ota_0  @0x020000  3M      ota_1 @0x320000  3M
dict   @0x620000  6M      只读，subtype 0x40，词库镜像，mmap 当数组查
userdict @0xc20000 1M     littlefs，用户自造词/词频，唯一可写区
```

占到 13.1 MB / 16 MB。改偏移会让设备上已有的 `userdict` 错位、用户数据全丢，
所以只在不得已时改，且要走 OTA 数据迁移。

## ★ 一期目标：reMarkable 与 BOOX

**一期只面向这两个品牌，它们的系统不同、提交路径不通用。** 品牌矩阵、调研依据、
每条结论的出处都在 `receiver/README.md`。摘要：

| | reMarkable 1/2 | BOOX |
|---|---|---|
| 系统 | 定制 Linux + Qt (`xochitl`) | Android (Onyx) |
| 蓝牙 | **无硬件** | 有 |
| 平板能否当 USB host | **不可用**（2025.1 起 sysfs role 开关被禁） | 可用（Android OTG） |
| **传输** | **USB，但 TypeFlow 当 host** | **BLE** |
| **提交机制** | **XOVI 扩展 → Qt `postEvent`** | **`InputMethodService.commitText()`** |
| 系统自带 CJK | **无**（Type Folio 只有英法德北欧布局） | 有 |
| 难度 | 高，无人做过，先做 spike | 低，走 Android 官方 API |
| 实现顺序 | **一期先做** | 二期 |

为什么 HID 发不了中文、为什么必须有设备端接收器，见 `system/hidout/hidout.h` 文件头。

### ⚠ 只有 USB 和 BLE 两类通道，不走 Wi-Fi

键盘联 Wi-Fi 功耗不可接受，用户预期也不对。但 USB 有**两种角色**，区别影响硬件：

| 通道 | TypeFlow 角色 | 用在哪 |
|---|---|---|
| `TARGET_LINK_USB_HID` | **device** | BOOX（Android 能当 OTG host） |
| `TARGET_LINK_USB_HOST` | **host** | reMarkable（平板切不到 host，但它默认是 RNDIS 网卡） |

reMarkable 那条是反着接：TypeFlow 当 host 连平板默认的 RNDIS gadget，
在 usb0 上跑 TCP + TypeFlow 协议。用 Espressif 官方组件 `espressif/iot_usbh_rndis`。

**ESP32-S3 只有一个 USB-OTG 外设，两种角色不能并存**，只能按目标品牌切。
所以硬件上 USB-C 口必须支持双角色（VBUS 可供可取）—— 这条在原理图定稿前要定，
见 `system/hidout/hidout_usbhost.c` 文件头的 VBUS 说明。

### 品牌归类在 `system/target/`

传输通道和提交机制都由 profile 决定，调用方不写 `if (品牌 == ...)`。
profile 会拦掉不可能的组合 —— 最典型的是「reMarkable 1/2 + BLE」，
那两台机器压根没有蓝牙硬件，静默失败会浪费很多调试时间。

### reMarkable 的关键推论

**大概率不需要 hook xochitl 的任何函数。** XOVI 把我们的 .so LD_PRELOAD 进
xochitl 进程后，文本输入走的是 Qt 的**公开 API**，直接调即可 —— XOVI 的
override/trampoline 能力在这里用不上，只用它的注入能力。

具体走 `QKeyEvent` 而不是 `QInputMethodEvent`：Qt 文本控件插入的是
`QKeyEvent::text()` 这个 QString，不按 keycode 查表。构造一个 keycode 为
`Key_unknown` 但 `text()` 是汉字的事件，控件照样把字插进去 ——
**键码那道墙只存在于 HID 和 evdev 层，到 Qt 事件层文本本来就是 QString。**

⚠ 动手写扩展前必须先跑完 `receiver/remarkable/xovi-ext/SPIKE.md`。其中两条前提会
直接决定方案可行性和一期范围：**Qt 符号是否被 strip**、**设备上有没有 CJK 字体**
（reMarkable 不支持 CJK，很可能连字体都没有 —— 那样 postEvent 成功了也只显示方框）。

### 为什么不用 rmkit / uinput

- **rmkit** 是做独立应用的框架（接管 framebuffer + 读 `/dev/input`）。我们要往
  **别人的应用**里塞文字，不是自己画界面。它的 `/dev/input` 归一化和 e-ink 刷新
  经验值得参考，但不是本任务的工具。
- **uinput**（ghostwriter 路线）两头都不占优：内核模块设备上没有要自己编译打包，
  而且注入的还是键码、到不了 CJK。ASCII 有零安装的 Type Folio 模拟更省事。

## 线协议

`protocol/PROTOCOL.md` 是线格式的**唯一真相来源**。

`protocol/` 在顶层而不是塞在接收器目录下，是因为它**不属于任何一侧**：
`typeflow_proto.c` 被编译进固件（`system/hidout/CMakeLists.txt` 用相对路径引入），
同时又是所有接收器的实现依据。`protocol/ref/` 是它的参考实现加一致性测试 ——
可执行的规范，所以放在规范旁边，而不是和 `receiver/remarkable`、`receiver/boox`
这些真品牌并列。

- `typeflow_proto.c` 由**固件和 reMarkable 接收器共用同一份源码**
  （`system/hidout/CMakeLists.txt` 用相对路径引进来编，不是复制）。
  协议双边不一致是最难查的一类 bug，共用源码从根上避免。
- Python 等价实现在 `protocol/ref/protocol.py`，BOOX 侧二期还要一份 Kotlin。
  **改协议时 .md / .c / .py（/ 将来的 .kt）都要同步，然后重跑自测。**
- magic 按**字节**定义（线上 `'T','F'`，小端 uint16 常量 `0x4654`）。
  写成 `0x5446` 再小端打包线上会变成 `"FT"`，抓包反着读很容易误判 ——
  这个坑已经被自测抓到过一次。


## 建议的实施顺序

`hidout` 两个后端第一版只实现 USB：不用管配对和供电，串口能看日志，调 IME 时干扰最小。

1. `debug/t_hello` 烧通 —— 确认工具链、串口、**PSRAM 报出 8 MB**（报 0 就是 `SPIRAM_MODE_OCT` 没对）
2. `debug/t_lcd` 点屏 —— 产出物是校准好的 `LCD_X_GAP` / `LCD_Y_GAP`
3. `debug/t_aw9523` —— 确认 chip ID 读到 0x23
4. `debug/t_kbmatrix` —— 产出物是 `system/input/input.c` 的真实键位表
5. `debug/t_usbhid` —— 主机能收到英文
6. `app/ime` 在 PC 上跑通拼音→候选（配 `tools/dict_build.py`）
7. 接 LVGL，`candbar` 上屏（步骤见 `system/uilv/uilv.h` 文件头）

**与上面并行、且越早越好的一件事**：跑 `receiver/remarkable/xovi-ext/SPIKE.md` 的
步骤 1 和风险清单最后一条 —— 只需 SSH 上去跑几条 `nm` 和 `fc-list`，
就能确认 reMarkable 方案是否可行、一期范围要不要扩到「还得装 CJK 字体」。
这是目前**信息价值最高、成本最低**的一步，别等到固件都做完了才发现。

固件侧不必等接收器：先用 `protocol/ref/` 在 PC 上跑通
传输 → 协议 → 文本 整条链路，它的 `--no-focus` / `--no-commit` 两个开关
专门用来测固件对「没聚焦输入框」和「接收器能力不足」的降级处理。

## 尚未引入的依赖

`main/idf_component.yml` 目前只声明 `idf`，**首次构建不需要联网**。按里程碑要加：

- `lvgl/lvgl` —— 接 UI 时（第 7 步）
- `espressif/esp_tinyusb` —— 接 USB HID 时。注意 TinyUSB **不在核心 IDF 内**，
  `components/usb` 是 USB **Host** 栈，不是这个
- `joltwallet/littlefs` —— 挂 userdict 分区时

NimBLE 在核心 IDF 内，已在 `sdkconfig.defaults` 开好，不需要声明。

## 约定

- `PROGRAM.md` 是需求的唯一来源；需求变化先更新它再改代码。
- 文档与代码注释用中文；模块头用块注释说明职责与来源，踩坑处就地写明原因。
- 参考项目 `D:\Codes\DeepStoa`（同一作者、同 IDF 版本的墨水屏学习机），
  搬代码前先看它对应模块，但注意上面列的"刻意差异"。
