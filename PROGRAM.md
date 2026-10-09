# 项目概述
基于 esp32s3 构建一个带长条屏显示候选词的智能键盘
- v1 主打给墨水屏中文/日文输入
- v2 增加 AI 功能, 比如自动补全、下一个词预测等

# 代码目录
- 参考设置以下目录: boards, drivers, system, app, debug, tools, vendor 等
- system 需包含公共组件：logs, ota, monitor, uilv 等

# 硬件
- esp32s3N16R8
- 76x284 SPI LCD彩屏