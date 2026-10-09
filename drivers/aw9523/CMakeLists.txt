# drivers/aw9523/CMakeLists.txt
# 纯驱动，不依赖板级头文件：引脚和地址都由调用方传入。
idf_component_register(
    SRCS "aw9523.c"
    INCLUDE_DIRS "."
    REQUIRES esp_driver_i2c esp_driver_gpio
)
