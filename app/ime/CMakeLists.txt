# app/ime/CMakeLists.txt
#
# REQUIRES 是空的 —— 这不是漏写，是刻意的约束：ime 内核和所有引擎只依赖 C
# 标准库，这样同一份源码能用普通 gcc 编译做回归测试（tools/ime-test/），
# 贡献者不需要 ESP-IDF、不需要开发板。往这里加任何 esp_* 依赖前请先想清楚代价。

# ── 扫引擎目录 ───────────────────────────────────────────────────────────
# 贡献者加一门语言只需新建 engines/<id>/ 目录，不碰任何现有文件。
# ⚠ GLOB 只在 CMake configure 阶段求值：新增目录后必须 `idf.py reconfigure`，
#   否则表现为链接期找不到新引擎。
file(GLOB _ime_engine_dirs ${CMAKE_CURRENT_LIST_DIR}/engines/*)
set(_ime_engine_srcs "")
foreach(_d ${_ime_engine_dirs})
    if(IS_DIRECTORY ${_d})
        get_filename_component(_name ${_d} NAME)
        # 以 _ 开头的目录不编进固件（_template 是给人复制的示例）
        if(NOT _name MATCHES "^_")
            file(GLOB _srcs ${_d}/*.c)
            list(APPEND _ime_engine_srcs ${_srcs})
        endif()
    endif()
endforeach()

# ── 生成注册表 ───────────────────────────────────────────────────────────
set(_ime_registry ${CMAKE_BINARY_DIR}/gen/ime/ime_registry.c)
if(NOT PYTHON)
    set(PYTHON python)
endif()
execute_process(
    COMMAND ${PYTHON} ${CMAKE_CURRENT_LIST_DIR}/../../tools/gen_ime_registry.py
            ${CMAKE_CURRENT_LIST_DIR}/engines ${_ime_registry}
    RESULT_VARIABLE _gen_result
    OUTPUT_VARIABLE _gen_output
    ERROR_VARIABLE  _gen_error
)
if(NOT _gen_result EQUAL 0)
    message(FATAL_ERROR "生成 ime 注册表失败:\n${_gen_output}${_gen_error}")
endif()
message(STATUS "ime: ${_gen_output}")

# INCLUDE_DIRS 里刻意**不**包含各引擎目录：引擎用 #include "xxx.h" 引自己的
# 头文件时，编译器先找当前文件所在目录，所以不需要。好处是两个引擎可以同时
# 有同名的内部头文件（比如都叫 syllable.h）而不冲突。
idf_component_register(
    SRCS "core/ime_core.c" ${_ime_registry} ${_ime_engine_srcs}
    INCLUDE_DIRS "." "core"
)
