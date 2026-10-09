# TypeFlow — ESP-IDF v5.5.3
# 带长条屏候选词的智能键盘：ESP32-S3 N16R8 + 76x284 ST7789 + AW9523 键盘矩阵
cmake_minimum_required(VERSION 3.16)

# 目录约定参考 D:\Codes\DeepStoa：组件按角色平铺在顶层，不用 components/。
# 每加一个组件，这里必须补一行 —— 漏了的表现是 "Failed to resolve component"。
set(EXTRA_COMPONENT_DIRS
    # ── 驱动：直接碰寄存器的代码 ────────────────────────────────
    ${CMAKE_SOURCE_DIR}/drivers/aw9523
    ${CMAKE_SOURCE_DIR}/drivers/kbmatrix

    # ── 平台服务：可复用、不含业务 ──────────────────────────────
    ${CMAKE_SOURCE_DIR}/system/target
    ${CMAKE_SOURCE_DIR}/system/logs
    ${CMAKE_SOURCE_DIR}/system/events
    ${CMAKE_SOURCE_DIR}/system/monitor
    ${CMAKE_SOURCE_DIR}/system/ota
    ${CMAKE_SOURCE_DIR}/system/controller
    ${CMAKE_SOURCE_DIR}/system/uilv
    ${CMAKE_SOURCE_DIR}/system/input
    ${CMAKE_SOURCE_DIR}/system/hidout
    ${CMAKE_SOURCE_DIR}/system/storage
    ${CMAKE_SOURCE_DIR}/system/battery
    ${CMAKE_SOURCE_DIR}/system/sleep
    ${CMAKE_SOURCE_DIR}/system/settings

    # ── 应用：输入法本体 ────────────────────────────────────────
    ${CMAKE_SOURCE_DIR}/app/ime
    ${CMAKE_SOURCE_DIR}/app/candbar
    # app/ai 是 v2 内容，实现后再取消注释
    # ${CMAKE_SOURCE_DIR}/app/ai
)

include($ENV{IDF_PATH}/tools/cmake/project.cmake)
project(TypeFlow)
