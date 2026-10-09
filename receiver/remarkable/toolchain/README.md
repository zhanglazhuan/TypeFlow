# 交叉编译

**两代机器架构不同，要各出一份构件：**

| 机型 | 架构 | 工具链 |
|---|---|---|
| reMarkable 1 / 2 | armv7l (32 位, hard-float) | `arm-linux-gnueabihf` |
| Paper Pro / Pro Move / Pure | aarch64 | `aarch64-linux-gnu` |

## 官方 toolchain

reMarkable 提供 Yocto SDK（安装脚本形式），装完 `source` 它的
`environment-setup-*` 即可。这是最稳的方式 —— 它带的 sysroot 与设备上的
glibc、Qt 版本一致，不会出现链接上了但设备上跑不起来的情况。

## 为什么不用通用 arm 工具链直接编

XOVI 扩展要和 xochitl 共用同一个 Qt 运行时，Qt 的 C++ ABI 对编译器版本和
标准库版本敏感。用与设备不匹配的工具链编出来的 .so，典型症状是加载时
符号找不到、或者跑起来在 Qt 调用里随机崩 —— 很难查。

所以：**先确认设备上的 Qt 版本和 gcc/glibc 版本**（见
`../xovi-ext/SPIKE.md` 步骤 1），再选工具链。

## 部署

```bash
ssh root@10.11.99.1                 # USB 连接时的固定地址
# 扩展 .so 放到 XOVI 的扩展目录，具体路径见 XOVI 文档
systemctl restart xochitl
```

⚠ 每次 reMarkable 系统更新都会清掉 `/home/root` 之外的改动，也可能让
Qt 符号变化。部署脚本要能重复执行，且扩展里的符号查找要做成运行时 `dlsym`
加失败时明确报错，而不是编译期硬绑（理由见 SPIKE.md 风险清单）。
