# main/CMakeLists.txt
idf_component_register(
    SRCS "TypeFlow.c"
    INCLUDE_DIRS "." "../boards/esp32s3"
    REQUIRES logs events monitor target storage controller uilv hidout input candbar
)
