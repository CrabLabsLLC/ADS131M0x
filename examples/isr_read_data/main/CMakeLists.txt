idf_component_register(
    SRCS "isr_read_data_main.c"
    INCLUDE_DIRS "."
    PRIV_REQUIRES ads131m0x driver esp_timer
)
