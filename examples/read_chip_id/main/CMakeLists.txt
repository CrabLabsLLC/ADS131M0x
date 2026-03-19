idf_component_register(
    SRCS "read_chip_id_main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES ads131m0x driver
)