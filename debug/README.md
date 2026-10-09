# debug/

每个子目录是一个**独立的 ESP-IDF 工程**（自带 CMakeLists.txt、main/、sdkconfig.defaults），
不参与主工程构建。源码入库，`build/` 和 `sdkconfig` 已在 .gitignore 里忽略。

这样分的好处：调某一个外设时不用带上整机的初始化链路，烧写快、日志干净、
出问题范围明确。约定同 DeepStoa 的 `debug/t_*`。

## 构建单个调试工程

```powershell
cd debug\t_hello
. C:\Espressif\frameworks\esp-idf-v5.5.3\export.ps1; idf.py -p COM<N> flash monitor
```

注意：子工程的 `sdkconfig.defaults` 要自己带 PSRAM 和 FATFS 那几项，
它们**不会**从主工程继承（各自是独立工程）。照抄 `t_hello/sdkconfig.defaults` 即可。

## 现有

| 工程 | 验证内容 | 状态 |
|---|---|---|
| `t_hello` | 工具链、烧写、串口、PSRAM 容量 | ✅ 可用 |

## 按里程碑要补的

| 工程 | 验证内容 | 关键产出 |
|---|---|---|
| `t_lcd` | 点亮 76×284 ST7789 | **校准 `LCD_X_GAP` / `LCD_Y_GAP`**，非标分辨率这步必踩坑 |
| `t_aw9523` | I2C 通不通、chip ID 是否 0x23 | 确认地址和上拉电阻 |
| `t_kbmatrix` | 逐键打印 (row, col) | **填 `system/input/input.c` 的键位表**，现在全是占位值 |
| `t_usbhid` | 主机能否收到英文字符 | 打通 TinyUSB |
| `t_blehid` | 蓝牙配对与 HID 上报 | 打通 NimBLE |
| `t_ime` | 拼音切分与候选（可在 PC 上跑，见 `tools/tests/`） | 词库格式定稿 |
