# app/candbar/CMakeLists.txt
idf_component_register(
    SRCS "candbar.c"
    INCLUDE_DIRS "."
    REQUIRES ime input hidout storage
)
