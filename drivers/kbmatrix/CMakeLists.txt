# drivers/kbmatrix/CMakeLists.txt
idf_component_register(
    SRCS "kbmatrix.c"
    INCLUDE_DIRS "."
    REQUIRES aw9523 esp_driver_gpio
)
