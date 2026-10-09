# system/uilv/CMakeLists.txt
# 引入 LVGL 后改成 file(GLOB_RECURSE) 收 framework/ widgets/ theme/ utils/ fonts/，
# 并在 REQUIRES 加 lvgl。写法参考 DeepStoa\system\uilv\CMakeLists.txt。
idf_component_register(
    SRCS "uilv.c"
    INCLUDE_DIRS "."
    REQUIRES controller
)
