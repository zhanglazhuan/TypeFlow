idf_component_register(
    SRCS "input.c"
    INCLUDE_DIRS "." "../../boards/esp32s3"
    REQUIRES kbmatrix aw9523 controller
)
