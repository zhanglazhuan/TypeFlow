# system/target/CMakeLists.txt
idf_component_register(
    SRCS "target.c"
    INCLUDE_DIRS "."
    REQUIRES esp_common
)
