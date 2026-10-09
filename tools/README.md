# tools/

主机侧脚本，不参与固件构建。

| 脚本 | 用途 | 状态 |
|---|---|---|
| `c_font.py` | 中日文字库裁剪成 LVGL 字体 .c | 待从 DeepStoa\tools 搬 |
| `LVGLImage.py` | 图片转 LVGL 图像 .c | 待从 DeepStoa\tools 搬 |
| `dict_build.py` | 词表 → 二进制词库镜像，刷进 `dict` 分区 | 待写 |
| `tests/` | `app/ime` 的 PC 侧回归测试 | 待写 |

## 为什么字库要裁剪

中文字库全量收录约 2 万字，16px 点阵就有几 MB。长条屏只有 76×284，一屏显示不了几个字，
但候选词可能覆盖常用 6000+ 字。裁剪策略应和 `dict_build.py` 联动：
只嵌入词库里真正出现的字，避免白占 flash。

## dict_build.py 的输出要和 app/ime/dict.h 的格式约定一致

格式要点见 `app/ime/dict.h` 文件头。特别注意：**词条必须按词频预排好**，
设备上不做排序；索引用定长记录以便二分查找。
