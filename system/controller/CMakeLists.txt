# system/controller/CMakeLists.txt
# 硬件门面层：上层不直接碰 drivers/，一律经过这里。
idf_component_register(
    SRCS "i2c_control.c" "display_control.c"
    INCLUDE_DIRS "." "../../boards/esp32s3"
    REQUIRES esp_driver_i2c esp_driver_spi esp_driver_gpio esp_lcd
)
