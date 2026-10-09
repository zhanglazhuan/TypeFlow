# system/storage/CMakeLists.txt
# 只访问片内 flash：NVS + dict 分区 mmap + userdict。没有 SD 卡相关依赖。
idf_component_register(
    SRCS "storage.c"
    INCLUDE_DIRS "."
    REQUIRES esp_partition nvs_flash
)
